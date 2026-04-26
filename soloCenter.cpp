#include <string>
#include <vector>
#include <unordered_map>

#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/desktop/Workspace.hpp>
#include <hyprland/src/layout/space/Space.hpp>
#include <hyprland/src/layout/algorithm/Algorithm.hpp>
#include <hyprland/src/layout/target/Target.hpp>

#include <hyprutils/string/VarList.hpp>

#include "globals.hpp"
#include "soloCenter.hpp"

using namespace Hyprutils::String;

// Per-workspace: if user resized the solo window, store the adjusted box.
// Cleared when window count leaves 1.
static std::unordered_map<WORKSPACEID, CBox> g_userAdjustedBox;

static bool isWorkspaceEnabled(WORKSPACEID id) {
    static auto const PENABLED = HyprlandAPI::getConfigValue(PHANDLE, "plugin:dwindle-solo:enabled_workspaces")->getDataStaticPtr();

    // STRING pattern from CConfigValue<Hyprlang::STRING>::operator*
    const char* raw = *reinterpret_cast<const Hyprlang::STRING*>(PENABLED);
    if (!raw || raw[0] == '\0')
        return true;

    const std::string enabledStr = raw;

    CVarList vars(enabledStr, 0, ',', true);
    for (const auto& v : vars) {
        try {
            if (std::stoi(v) == id)
                return true;
        } catch (...) {
            continue;
        }
    }
    return false;
}

namespace DwindleSolo {

    // Compute the default solo box from config
    static CBox computeDefaultSoloBox(const CBox& workArea) {
        static auto const PWIDTH  = HyprlandAPI::getConfigValue(PHANDLE, "plugin:dwindle-solo:solo_width")->getDataStaticPtr();
        static auto const PHEIGHT = HyprlandAPI::getConfigValue(PHANDLE, "plugin:dwindle-solo:solo_height")->getDataStaticPtr();
        static auto const PALIGN  = HyprlandAPI::getConfigValue(PHANDLE, "plugin:dwindle-solo:solo_align")->getDataStaticPtr();

        const float soloWidth  = std::clamp(**reinterpret_cast<Hyprlang::FLOAT* const*>(PWIDTH), 0.1f, 1.0f);
        const float soloHeight = std::clamp(**reinterpret_cast<Hyprlang::FLOAT* const*>(PHEIGHT), 0.1f, 1.0f);
        const int64_t soloAlign = **reinterpret_cast<Hyprlang::INT* const*>(PALIGN);

        const float newW = workArea.w * soloWidth;
        const float newH = workArea.h * soloHeight;

        float newX, newY;
        switch (soloAlign) {
            case 1: newX = workArea.x; break;
            case 2: newX = workArea.x + workArea.w - newW; break;
            default: newX = workArea.x + (workArea.w - newW) / 2.0f; break;
        }
        newY = workArea.y + (workArea.h - newH) / 2.0f;

        return {newX, newY, newW, newH};
    }

    // Check if the algorithm has exactly 1 tiled target on an enabled workspace.
    // Returns the target and workspace ID, or nullptr if not applicable.
    struct SoloInfo {
        SP<Layout::ITarget> target;
        WORKSPACEID         wsId = -1;
        SP<Layout::CSpace>  space;
    };

    static SoloInfo getSoloTarget(Layout::CAlgorithm* algo) {
        SoloInfo info;
        if (!algo)
            return info;

        info.space = algo->space();
        if (!info.space)
            return info;

        auto workspace = info.space->workspace();
        if (!workspace)
            return info;

        info.wsId = workspace->m_id;

        if (algo->tiledTargets() != 1)
            return info;

        if (!isWorkspaceEnabled(info.wsId))
            return info;

        for (auto& tw : info.space->targets()) {
            auto t = tw.lock();
            if (!t || t->floating() || t->fullscreenMode() != eFullscreenMode::FSMODE_NONE || t->isPseudo())
                continue;
            info.target = t;
            break;
        }

        return info;
    }

    void postRecalculate(Layout::CAlgorithm* algo) {
        auto info = getSoloTarget(algo);

        // Not solo — clear any stored adjustment for this workspace
        if (!info.target) {
            if (info.wsId >= 0)
                g_userAdjustedBox.erase(info.wsId);
            return;
        }

        const auto workArea = info.space->workArea();

        CBox box;
        auto it = g_userAdjustedBox.find(info.wsId);
        if (it != g_userAdjustedBox.end()) {
            // User has resized — use their adjusted box
            box = it->second;
        } else {
            // Fresh solo — use config defaults
            box = computeDefaultSoloBox(workArea);
        }

        // If config is 1.0/1.0, nothing to do
        if (box.w >= workArea.w && box.h >= workArea.h)
            return;

        info.target->setPositionGlobal(box);
    }

    bool handleResize(Layout::CAlgorithm* algo, const Vector2D& delta, SP<Layout::ITarget> target, Layout::eRectCorner corner) {
        auto info = getSoloTarget(algo);

        // Not our solo window — let dwindle handle it
        if (!info.target || info.target != target)
            return false;

        CBox current = info.target->position();
        const auto workArea = info.space->workArea();

        // Read alignment/height config to determine anchored vs free edges
        static auto const PALIGN  = HyprlandAPI::getConfigValue(PHANDLE, "plugin:dwindle-solo:solo_align")->getDataStaticPtr();
        static auto const PHEIGHT = HyprlandAPI::getConfigValue(PHANDLE, "plugin:dwindle-solo:solo_height")->getDataStaticPtr();
        const int64_t soloAlign = **reinterpret_cast<Hyprlang::INT* const*>(PALIGN);
        const float soloHeight = std::clamp(**reinterpret_cast<Hyprlang::FLOAT* const*>(PHEIGHT), 0.1f, 1.0f);

        // Determine free edges: a free edge is one NOT flush with the work area boundary.
        // align 0=center (free left+right), 1=left (free right), 2=right (free left)
        const bool freeLeft  = (soloAlign == 2 || soloAlign == 0);
        const bool freeRight = (soloAlign == 1 || soloAlign == 0);
        const bool freeVert  = (soloHeight < 0.99f);

        // Convert the raw delta into a "grow" amount for each axis.
        // Positive growX = window gets wider; positive growY = window gets taller.
        float growX = 0, growY = 0;

        if (corner == Layout::CORNER_NONE) {
            // Keyboard resize: delta is an abstract grow amount
            growX = delta.x;
            growY = delta.y;
        } else {
            // Mouse resize: delta is raw mouse movement, corner says which edge was grabbed.
            // Only respond if the grabbed edge is a free edge.
            const bool grabbedLeft  = (corner == Layout::CORNER_TOPLEFT  || corner == Layout::CORNER_BOTTOMLEFT);
            const bool grabbedRight = (corner == Layout::CORNER_TOPRIGHT || corner == Layout::CORNER_BOTTOMRIGHT);

            if (grabbedLeft && freeLeft)
                growX = -delta.x;   // dragging left edge left (negative delta) = grow
            else if (grabbedRight && freeRight)
                growX = delta.x;    // dragging right edge right (positive delta) = grow

            if (freeVert) {
                const bool grabbedTop    = (corner == Layout::CORNER_TOPLEFT  || corner == Layout::CORNER_TOPRIGHT);
                const bool grabbedBottom = (corner == Layout::CORNER_BOTTOMLEFT || corner == Layout::CORNER_BOTTOMRIGHT);
                if (grabbedTop)
                    growY = -delta.y;
                else if (grabbedBottom)
                    growY = delta.y;
            }
        }

        // Apply growth only to the free edge(s), keeping anchored edges fixed.
        if (freeLeft && !freeRight) {
            // Right-aligned: anchor right edge, move left edge
            current.x -= growX;
            current.w += growX;
        } else if (freeRight && !freeLeft) {
            // Left-aligned: anchor left edge, move right edge
            current.w += growX;
        } else if (freeLeft && freeRight) {
            // Center: grow symmetrically from both sides
            current.x -= growX / 2.0f;
            current.w += growX;
        }

        if (freeVert) {
            // Vertically centered: grow symmetrically top and bottom
            current.y -= growY / 2.0f;
            current.h += growY;
        }

        // Clamp minimum size
        current.w = std::max(current.w, 100.0);
        current.h = std::max(current.h, 100.0);

        // Clamp to work area
        current.x = std::max(current.x, workArea.x);
        current.y = std::max(current.y, workArea.y);
        if (current.x + current.w > workArea.x + workArea.w)
            current.w = workArea.x + workArea.w - current.x;
        if (current.y + current.h > workArea.y + workArea.h)
            current.h = workArea.y + workArea.h - current.y;

        // Store the adjusted box and apply
        g_userAdjustedBox[info.wsId] = current;
        info.target->setPositionGlobal(current);
        return true;
    }
}

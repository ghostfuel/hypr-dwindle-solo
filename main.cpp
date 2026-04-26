#define WLR_USE_UNSTABLE

#include <unistd.h>
#include <any>
#include <sstream>
#include <string>
#include <vector>
#include <functional>
#include <expected>
#include <optional>

#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/plugins/HookSystem.hpp>
#include <hyprland/src/layout/algorithm/Algorithm.hpp>

#include "globals.hpp"
#include "soloCenter.hpp"

static CFunctionHook* g_pRecalcHook = nullptr;
static CFunctionHook* g_pResizeHook = nullptr;

// CAlgorithm::recalculate() — non-virtual member, no params besides this.
typedef void (*origRecalculate)(void*);

// CAlgorithm::resizeTarget(const Vector2D&, SP<ITarget>, eRectCorner)
typedef void (*origResizeTarget)(void*, const Vector2D&, SP<Layout::ITarget>, Layout::eRectCorner);

void hkRecalculate(void* thisptr) {
    // Call original recalculation first (delegates to dwindle)
    (*(origRecalculate)g_pRecalcHook->m_original)(thisptr);

    // Post-process: check for solo window and reposition
    DwindleSolo::postRecalculate(reinterpret_cast<Layout::CAlgorithm*>(thisptr));
}

void hkResizeTarget(void* thisptr, const Vector2D& delta, SP<Layout::ITarget> target, Layout::eRectCorner corner) {
    auto* algo = reinterpret_cast<Layout::CAlgorithm*>(thisptr);

    // If this is a solo window we're managing, handle resize ourselves
    if (DwindleSolo::handleResize(algo, delta, target, corner))
        return;

    // Otherwise, pass through to original dwindle resize
    (*(origResizeTarget)g_pResizeHook->m_original)(thisptr, delta, target, corner);
}

APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    const std::string HASH        = __hyprland_api_get_hash();
    const std::string CLIENT_HASH = __hyprland_api_get_client_hash();

    if (HASH != CLIENT_HASH) {
        HyprlandAPI::addNotification(PHANDLE, "[hypr-dwindle-solo] Mismatched headers! Can't proceed.",
                                     CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
        throw std::runtime_error("[hypr-dwindle-solo] Version mismatch");
    }

    // Register config values
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:dwindle-solo:solo_width", Hyprlang::FLOAT{0.65F});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:dwindle-solo:solo_height", Hyprlang::FLOAT{1.0F});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:dwindle-solo:solo_align", Hyprlang::INT{0});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:dwindle-solo:enabled_workspaces", Hyprlang::STRING{""});

    // Hook CAlgorithm::recalculate — non-virtual, called after every layout change
    static const auto METHODS = HyprlandAPI::findFunctionsByName(PHANDLE, "recalculate");

    void* hookAddr = nullptr;
    for (auto& m : METHODS) {
        if (m.demangled.contains("CAlgorithm") && !m.demangled.contains("ITiled") && !m.demangled.contains("IFloating") && !m.demangled.contains("IMode")) {
            hookAddr = m.address;
            break;
        }
    }

    if (!hookAddr) {
        HyprlandAPI::addNotification(PHANDLE, "[hypr-dwindle-solo] Could not find CAlgorithm::recalculate!",
                                     CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
        throw std::runtime_error("[hypr-dwindle-solo] Hook target not found");
    }

    g_pRecalcHook = HyprlandAPI::createFunctionHook(PHANDLE, hookAddr, (void*)&hkRecalculate);
    g_pRecalcHook->hook();

    // Hook CAlgorithm::resizeTarget — to handle resize of solo windows
    static const auto RESIZE_METHODS = HyprlandAPI::findFunctionsByName(PHANDLE, "resizeTarget");

    void* resizeAddr = nullptr;
    for (auto& m : RESIZE_METHODS) {
        if (m.demangled.contains("CAlgorithm") && m.demangled.contains("resizeTarget")) {
            resizeAddr = m.address;
            break;
        }
    }

    if (!resizeAddr) {
        HyprlandAPI::addNotification(PHANDLE, "[hypr-dwindle-solo] Could not find CAlgorithm::resizeTarget!",
                                     CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
        throw std::runtime_error("[hypr-dwindle-solo] Resize hook target not found");
    }

    g_pResizeHook = HyprlandAPI::createFunctionHook(PHANDLE, resizeAddr, (void*)&hkResizeTarget);
    g_pResizeHook->hook();

    HyprlandAPI::addNotification(PHANDLE, "[hypr-dwindle-solo] Loaded successfully!", CHyprColor{0.2, 1.0, 0.2, 1.0}, 3000);

    return {"hypr-dwindle-solo", "Sizes and aligns solo windows on dwindle workspaces", "ghostfuel", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    if (g_pRecalcHook)
        g_pRecalcHook->unhook();
    if (g_pResizeHook)
        g_pResizeHook->unhook();
}

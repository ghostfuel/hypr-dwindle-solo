#pragma once

#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/config/values/types/FloatValue.hpp>
#include <hyprland/src/config/values/types/IntValue.hpp>
#include <hyprland/src/config/values/types/StringValue.hpp>

inline HANDLE PHANDLE = nullptr;

// V2 config values — registered in PLUGIN_INIT, read elsewhere via ->value().
inline SP<Config::Values::CFloatValue>  g_pSoloWidth;
inline SP<Config::Values::CFloatValue>  g_pSoloHeight;
inline SP<Config::Values::CIntValue>    g_pSoloAlign;
inline SP<Config::Values::CStringValue> g_pEnabledWorkspaces;

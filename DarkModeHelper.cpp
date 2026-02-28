// sktoolslib - common files for SK tools

// Copyright (C) 2019-2021, 2026 - Stefan Kueng

// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
//
#include "stdafx.h"
#include "DarkModeHelper.h"
#include "StringUtils.h"
#include "PathUtils.h"
#include <vector>
#include <Shlobj.h>
#include <dwmapi.h>

#pragma comment(lib, "dwmapi.lib")

namespace
{
// Check if running on Windows 11 or later
bool IsWindows11OrGreater()
{
    PWSTR        pszPath = nullptr;
    std::wstring sysPath;
    if (SHGetKnownFolderPath(FOLDERID_System, KF_FLAG_CREATE, nullptr, &pszPath) == S_OK)
    {
        sysPath = pszPath;
        CoTaskMemFree(pszPath);
    }
    auto             version = CPathUtils::GetVersionFromFile(sysPath + L"\\shell32.dll");
    std::vector<int> versions;
    stringtok(versions, version, true, L".");
    return versions.size() > 2 && versions[2] >= 22000;
}
} // namespace

// Apply Windows 11 Mica backdrop effect
void DarkModeHelper::ApplyMicaBackdrop(HWND hwnd)
{
    if (!IsWindows11OrGreater())
        return;

    // DWMWA_SYSTEMBACKDROP_TYPE is 38 for Windows 11
    // DWMSBT_MAINWINDOW (2) enables Mica
    constexpr DWORD DWMWA_SYSTEMBACKDROP_TYPE = 38;
    constexpr int   DWMSBT_MAINWINDOW         = 2;

    DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE,
                          &DWMSBT_MAINWINDOW, sizeof(DWMSBT_MAINWINDOW));
}

// Enable dark mode for context/popup menus (Windows 10 1809+)
void DarkModeHelper::EnableDarkModeForMenu(HWND hwnd, bool isDark)
{
    // Use undocumented API to enable dark mode for menus
    // This works on Windows 10 1809+ and Windows 11
    constexpr DWORD DWMWA_USE_IMMERSIVE_DARK_MODE_BEFORE_20H1 = 19;
    constexpr DWORD DWMWA_USE_IMMERSIVE_DARK_MODE             = 20;

    BOOL            value                                     = isDark ? TRUE : FALSE;

    // Try Windows 10 20H1+ method first
    HRESULT         hr                                        = DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE,
                                                                                      &value, sizeof(value));

    // Fall back to older method if needed
    if (FAILED(hr))
    {
        DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE_BEFORE_20H1,
                              &value, sizeof(value));
    }
}

DarkModeHelper& DarkModeHelper::Instance()
{
    static DarkModeHelper helper;
    return helper;
}

bool DarkModeHelper::CanHaveDarkMode() const
{
    return m_bCanHaveDarkMode;
}

void DarkModeHelper::AllowDarkModeForApp(BOOL allow) const
{
    if (m_pAllowDarkModeForApp)
        m_pAllowDarkModeForApp(allow ? 1 : 0);
    if (m_pSetPreferredAppMode)
        m_pSetPreferredAppMode(allow ? ForceDark : Default);
}

void DarkModeHelper::AllowDarkModeForWindow(HWND hwnd, BOOL allow) const
{
    if (m_pAllowDarkModeForWindow)
        m_pAllowDarkModeForWindow(hwnd, allow);
}

BOOL DarkModeHelper::ShouldAppsUseDarkMode() const
{
    if (m_pShouldAppsUseDarkMode && m_pAllowDarkModeForApp)
        return m_pShouldAppsUseDarkMode() & 0x01;
    return ShouldSystemUseDarkMode();
}

BOOL DarkModeHelper::IsDarkModeAllowedForWindow(HWND hwnd) const
{
    if (m_pIsDarkModeAllowedForWindow)
        return m_pIsDarkModeAllowedForWindow(hwnd);
    return FALSE;
}

BOOL DarkModeHelper::IsDarkModeAllowedForApp() const
{
    if (m_pIsDarkModeAllowedForApp)
        return m_pIsDarkModeAllowedForApp();
    return FALSE;
}

BOOL DarkModeHelper::ShouldSystemUseDarkMode() const
{
    if (m_pShouldSystemUseDarkMode)
        return m_pShouldSystemUseDarkMode();
    return FALSE;
}

void DarkModeHelper::RefreshImmersiveColorPolicyState() const
{
    if (m_pRefreshImmersiveColorPolicyState)
        m_pRefreshImmersiveColorPolicyState();
}

BOOL DarkModeHelper::GetIsImmersiveColorUsingHighContrast(IMMERSIVE_HC_CACHE_MODE mode) const
{
    if (m_pGetIsImmersiveColorUsingHighContrast)
        return m_pGetIsImmersiveColorUsingHighContrast(mode);
    return FALSE;
}

void DarkModeHelper::FlushMenuThemes() const
{
    if (m_pFlushMenuThemes)
        m_pFlushMenuThemes();
}

BOOL DarkModeHelper::SetWindowCompositionAttribute(HWND hWnd, WINDOWCOMPOSITIONATTRIBDATA* data) const
{
    if (m_pSetWindowCompositionAttribute)
        return m_pSetWindowCompositionAttribute(hWnd, data);
    return FALSE;
}

void DarkModeHelper::RefreshTitleBarThemeColor(HWND hWnd, BOOL dark) const
{
    WINDOWCOMPOSITIONATTRIBDATA data = {WCA_USEDARKMODECOLORS, &dark, sizeof(dark)};
    SetWindowCompositionAttribute(hWnd, &data);
}

DarkModeHelper::DarkModeHelper()
{
    INITCOMMONCONTROLSEX used = {
        sizeof(INITCOMMONCONTROLSEX),
        ICC_STANDARD_CLASSES | ICC_BAR_CLASSES | ICC_COOL_CLASSES};
    InitCommonControlsEx(&used);

    m_bCanHaveDarkMode   = false;
    PWSTR        sysPath = nullptr;
    long         micro   = 0;
    std::wstring dllPath;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_System, 0, nullptr, &sysPath)))
    {
        dllPath = sysPath;
        CoTaskMemFree(sysPath);
        dllPath += L"\\uxtheme.dll";
        auto                      version = CPathUtils::GetVersionFromFile(dllPath);
        std::vector<std::wstring> tokens;
        stringtok(tokens, version, false, L".");
        if (tokens.size() == 4)
        {
            auto major = std::stol(tokens[0]);
            auto minor = std::stol(tokens[1]);
            micro      = std::stol(tokens[2]);
            // auto build = std::stol(tokens[3]);

            // the windows 10 update 1809 has the version
            // number as 10.0.17763.1
            if (major > 10)
                m_bCanHaveDarkMode = true;
            else if (major == 10)
            {
                if (minor > 0)
                    m_bCanHaveDarkMode = true;
                else if (micro > 17762)
                    m_bCanHaveDarkMode = true;
            }
        }
    }
    if (dllPath.empty())
        m_hUxthemeLib = LoadLibrary(L"uxtheme.dll");
    else
        m_hUxthemeLib = LoadLibrary(dllPath.c_str());
    if (m_hUxthemeLib && m_bCanHaveDarkMode)
    {
        // Note: these functions are undocumented! Which meas I shouldn't even use them.
        // So, since MS decided to keep this new feature to themselves, I have to use
        // undocumented functions to adjust.
        // Let's just hope they change their minds and document these functions one day...

        // first try with the names, just in case MS decides to properly export these functions
        if (micro < 18362)
            m_pAllowDarkModeForApp = reinterpret_cast<AllowDarkModeForAppFpn>(GetProcAddress(m_hUxthemeLib, "AllowDarkModeForApp"));
        else
            m_pSetPreferredAppMode = reinterpret_cast<SetPreferredAppModeFpn>(GetProcAddress(m_hUxthemeLib, "SetPreferredAppMode"));
        m_pAllowDarkModeForWindow               = reinterpret_cast<AllowDarkModeForWindowFpn>(GetProcAddress(m_hUxthemeLib, "AllowDarkModeForWindow"));
        m_pShouldAppsUseDarkMode                = reinterpret_cast<ShouldAppsUseDarkModeFpn>(GetProcAddress(m_hUxthemeLib, "ShouldAppsUseDarkMode"));
        m_pIsDarkModeAllowedForWindow           = reinterpret_cast<IsDarkModeAllowedForWindowFpn>(GetProcAddress(m_hUxthemeLib, "IsDarkModeAllowedForWindow"));
        m_pIsDarkModeAllowedForApp              = reinterpret_cast<IsDarkModeAllowedForAppFpn>(GetProcAddress(m_hUxthemeLib, "IsDarkModeAllowedForApp"));
        m_pShouldSystemUseDarkMode              = reinterpret_cast<ShouldSystemUseDarkModeFpn>(GetProcAddress(m_hUxthemeLib, "ShouldSystemUseDarkMode"));
        m_pRefreshImmersiveColorPolicyState     = reinterpret_cast<RefreshImmersiveColorPolicyStateFn>(GetProcAddress(m_hUxthemeLib, "RefreshImmersiveColorPolicyState"));
        m_pGetIsImmersiveColorUsingHighContrast = reinterpret_cast<GetIsImmersiveColorUsingHighContrastFn>(GetProcAddress(m_hUxthemeLib, "GetIsImmersiveColorUsingHighContrast"));
        m_pFlushMenuThemes                      = reinterpret_cast<FlushMenuThemesFn>(GetProcAddress(m_hUxthemeLib, "FlushMenuThemes"));
        m_pSetWindowCompositionAttribute        = reinterpret_cast<SetWindowCompositionAttributeFpn>(GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetWindowCompositionAttribute"));
        if (m_pAllowDarkModeForApp == nullptr && micro < 18362)
            m_pAllowDarkModeForApp = reinterpret_cast<AllowDarkModeForAppFpn>(GetProcAddress(m_hUxthemeLib, MAKEINTRESOURCEA(135)));
        if (m_pSetPreferredAppMode == nullptr && micro >= 18362)
            m_pSetPreferredAppMode = reinterpret_cast<SetPreferredAppModeFpn>(GetProcAddress(m_hUxthemeLib, MAKEINTRESOURCEA(135)));
        if (m_pAllowDarkModeForWindow == nullptr)
            m_pAllowDarkModeForWindow = reinterpret_cast<AllowDarkModeForWindowFpn>(GetProcAddress(m_hUxthemeLib, MAKEINTRESOURCEA(133)));
        if (m_pShouldAppsUseDarkMode == nullptr)
            m_pShouldAppsUseDarkMode = reinterpret_cast<ShouldAppsUseDarkModeFpn>(GetProcAddress(m_hUxthemeLib, MAKEINTRESOURCEA(132)));
        if (m_pIsDarkModeAllowedForWindow == nullptr)
            m_pIsDarkModeAllowedForWindow = reinterpret_cast<IsDarkModeAllowedForWindowFpn>(GetProcAddress(m_hUxthemeLib, MAKEINTRESOURCEA(137)));
        if (m_pIsDarkModeAllowedForApp == nullptr)
            m_pIsDarkModeAllowedForApp = reinterpret_cast<IsDarkModeAllowedForAppFpn>(GetProcAddress(m_hUxthemeLib, MAKEINTRESOURCEA(139)));
        if (m_pShouldSystemUseDarkMode == nullptr)
            m_pShouldSystemUseDarkMode = reinterpret_cast<ShouldSystemUseDarkModeFpn>(GetProcAddress(m_hUxthemeLib, MAKEINTRESOURCEA(138)));
        if (m_pRefreshImmersiveColorPolicyState == nullptr)
            m_pRefreshImmersiveColorPolicyState = reinterpret_cast<RefreshImmersiveColorPolicyStateFn>(GetProcAddress(m_hUxthemeLib, MAKEINTRESOURCEA(104)));
        if (m_pGetIsImmersiveColorUsingHighContrast == nullptr)
            m_pGetIsImmersiveColorUsingHighContrast = reinterpret_cast<GetIsImmersiveColorUsingHighContrastFn>(GetProcAddress(m_hUxthemeLib, MAKEINTRESOURCEA(106)));
        if (m_pFlushMenuThemes == nullptr)
            m_pFlushMenuThemes = reinterpret_cast<FlushMenuThemesFn>(GetProcAddress(m_hUxthemeLib, MAKEINTRESOURCEA(136)));
    }
}

DarkModeHelper::~DarkModeHelper()
{
    FreeLibrary(m_hUxthemeLib);
}

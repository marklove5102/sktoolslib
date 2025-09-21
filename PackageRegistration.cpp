// sktoolslib - common files for SK tools

// Copyright (C) 2025 - Stefan Kueng

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
#include "PackageRegistration.h"

#include <string>

#include <winrt/Windows.Management.Deployment.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.ApplicationModel.h>
#include "../../ext/sktoolslib/Registry.h"

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Management::Deployment;

#pragma comment(lib, "windowsapp.lib")

#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "comsupp.lib")

PackageRegistration::PackageRegistration(const std::wstring& extPath, const std::wstring& msixPath, const std::wstring& packageIdentity)
    : m_extPath(extPath)
    , m_msixPath(msixPath)
    , m_packageIdentity(packageIdentity)
{
    winrt::init_apartment();
}

PackageRegistration::~PackageRegistration()
{
    winrt::uninit_apartment();
}

std::wstring PackageRegistration::RegisterForCurrentUser()
{
    PackageManager                                                    manager;

    Collections::IIterable<winrt::Windows::ApplicationModel::Package> packages;
    try
    {
        packages = manager.FindPackagesForUser(L"");
    }
    catch (winrt::hresult_error const& ex)
    {
        std::wstring error = L"FindPackagesForUser failed (Errorcode: ";
        error += std::to_wstring(ex.code().value);
        error += L"):\n";
        error += ex.message();
        return error;
    }

    for (const auto& package : packages)
    {
        if (package.Id().Name() == m_packageIdentity)
        {
            // already installed
            return {};
        }
    }

    // now register the package
    Uri               externalUri(m_extPath);
    Uri               packageUri(m_msixPath);
    AddPackageOptions options;
    options.ExternalLocationUri(externalUri);
    auto deploymentOperation = manager.AddPackageByUriAsync(packageUri, options);

    auto deployResult        = deploymentOperation.get();

    if (!SUCCEEDED(deployResult.ExtendedErrorCode()))
    {
        std::wstring error = L"AddPackageByUriAsync failed (Errorcode: ";
        error += std::to_wstring(deployResult.ExtendedErrorCode());
        error += L"):\n";
        error += deployResult.ErrorText();
        return error;
    }
    return {};
}

std::wstring PackageRegistration::UnregisterForCurrentUser()
{
    PackageManager                                                    manager;
    Collections::IIterable<winrt::Windows::ApplicationModel::Package> packages;
    try
    {
        packages = manager.FindPackagesForUser(L"");
    }
    catch (winrt::hresult_error const& ex)
    {
        std::wstring error = L"FindPackagesForUser failed (Errorcode: ";
        error += std::to_wstring(ex.code().value);
        error += L"):\n";
        error += ex.message();
        return error;
    }

    // remove any existing package
    for (const auto& package : packages)
    {
        if (package.Id().Name() != m_packageIdentity)
            continue;
        winrt::hstring fullName            = package.Id().FullName();
        auto           deploymentOperation = manager.RemovePackageAsync(fullName, RemovalOptions::None);
        auto           deployResult        = deploymentOperation.get();
        if (SUCCEEDED(deployResult.ExtendedErrorCode()))
            break;

        // Undeployment failed
        std::wstring error = L"RemovePackageAsync failed (Errorcode: ";
        error += std::to_wstring(deployResult.ExtendedErrorCode());
        error += L"):\n";
        error += deployResult.ErrorText();
        return error;
    }
    return {};
}

std::wstring PackageRegistration::ReRegisterForCurrentUser()
{
    UnregisterForCurrentUser();
    return RegisterForCurrentUser();
}

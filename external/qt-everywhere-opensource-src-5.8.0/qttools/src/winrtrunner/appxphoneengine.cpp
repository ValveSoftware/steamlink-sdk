/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the tools applications of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "appxphoneengine.h"
#include "appxengine_p.h"

#include <QtCore/QDir>
#include <QtCore/QDirIterator>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QHash>
#include <QtCore/QUuid>
#include <QtCore/QLoggingCategory>
#include <QtCore/QDateTime>

#include <comdef.h>
#include <psapi.h>

#include <ShlObj.h>
#include <Shlwapi.h>
#include <wsdevlicensing.h>
#include <AppxPackaging.h>
#include <xmllite.h>
#include <wrl.h>
#include <windows.applicationmodel.h>
#include <windows.management.deployment.h>

using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Management::Deployment;
using namespace ABI::Windows::ApplicationModel;
using namespace ABI::Windows::System;

// From Microsoft.Phone.Tools.Deploy assembly
namespace PhoneTools {
    enum DeploymentOptions
    {
        None = 0,
        PA = 1,
        Debug = 2,
        Infused = 4,
        Lightup = 8,
        Enterprise = 16,
        Sideload = 32,
        TypeMask = 255,
        UninstallDisabled = 256,
        SkipUpdateAppInForeground = 512,
        DeleteXap = 1024,
        InstallOnSD = 65536,
        OptOutSD = 131072
    };
    enum PackageType
    {
        UnknownAppx = 0,
        Main = 1,
        Framework = 2,
        Resource = 4,
        Bundle = 8,
        Xap = 0
    };
}

QT_USE_NAMESPACE

#include <corecon.h>
#include <ccapi_12.h>
Q_GLOBAL_STATIC_WITH_ARGS(CoreConServer, coreConServer, (12))

#undef RETURN_IF_FAILED
#define RETURN_IF_FAILED(msg, ret) \
    if (FAILED(hr)) { \
        qCWarning(lcWinRtRunner).nospace() << msg << ": 0x" << QByteArray::number(hr, 16).constData() \
                                           << ' ' << coreConServer->formatError(hr); \
        ret; \
    }

// Set a break handler for gracefully breaking long-running ops
static bool g_ctrlReceived = false;
static bool g_handleCtrl = false;
static BOOL WINAPI ctrlHandler(DWORD type)
{
    switch (type) {
    case CTRL_C_EVENT:
    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
        g_ctrlReceived = g_handleCtrl;
        return g_handleCtrl;
    case CTRL_BREAK_EVENT:
    case CTRL_SHUTDOWN_EVENT:
    default:
        break;
    }
    return false;
}

class AppxPhoneEnginePrivate : public AppxEnginePrivate
{
public:
    QString productId;

    ComPtr<ICcConnection> connection;
    CoreConDevice *device;
    QSet<QString> dependencies;
};

static ProcessorArchitecture toProcessorArchitecture(APPX_PACKAGE_ARCHITECTURE appxArch)
{
    switch (appxArch) {
    case APPX_PACKAGE_ARCHITECTURE_X86:
        return ProcessorArchitecture_X86;
    case APPX_PACKAGE_ARCHITECTURE_ARM:
        return ProcessorArchitecture_Arm;
    case APPX_PACKAGE_ARCHITECTURE_X64:
        return ProcessorArchitecture_X64;
    case APPX_PACKAGE_ARCHITECTURE_NEUTRAL:
        // fall-through intended
    default:
        return ProcessorArchitecture_Neutral;
    }
}

static bool getPhoneProductId(IStream *manifestStream, QString *productId)
{
    // Read out the phone product ID (not supported by AppxManifestReader)
    ComPtr<IXmlReader> xmlReader;
    HRESULT hr = CreateXmlReader(IID_PPV_ARGS(&xmlReader), NULL);
    RETURN_FALSE_IF_FAILED("Failed to create XML reader");

    hr = xmlReader->SetInput(manifestStream);
    RETURN_FALSE_IF_FAILED("Failed to set manifest as input");

    while (!xmlReader->IsEOF()) {
        XmlNodeType nodeType;
        hr = xmlReader->Read(&nodeType);
        RETURN_FALSE_IF_FAILED("Failed to read next node in manifest");
        if (nodeType == XmlNodeType_Element) {
            PCWSTR uri;
            hr = xmlReader->GetNamespaceUri(&uri, NULL);
            RETURN_FALSE_IF_FAILED("Failed to read namespace URI of current node");
            if (wcscmp(uri, L"http://schemas.microsoft.com/appx/2014/phone/manifest") == 0) {
                PCWSTR localName;
                hr = xmlReader->GetLocalName(&localName, NULL);
                RETURN_FALSE_IF_FAILED("Failed to get local name of current node");
                if (wcscmp(localName, L"PhoneIdentity") == 0) {
                    hr = xmlReader->MoveToAttributeByName(L"PhoneProductId", NULL);
                    if (hr == S_FALSE)
                        continue;
                    RETURN_FALSE_IF_FAILED("Failed to seek to the PhoneProductId attribute");
                    PCWSTR phoneProductId;
                    UINT length;
                    hr = xmlReader->GetValue(&phoneProductId, &length);
                    RETURN_FALSE_IF_FAILED("Failed to read the value of the PhoneProductId attribute");
                    *productId = QLatin1Char('{') + QString::fromWCharArray(phoneProductId, length) + QLatin1Char('}');
                    return true;
                }
            }
        }
    }
    return false;
}

bool AppxPhoneEngine::canHandle(Runner *runner)
{
    return getManifestFile(runner->app());
}

RunnerEngine *AppxPhoneEngine::create(Runner *runner)
{
    QScopedPointer<AppxPhoneEngine> engine(new AppxPhoneEngine(runner));
    if (engine->d_ptr->hasFatalError)
        return 0;

    return engine.take();
}

QStringList AppxPhoneEngine::deviceNames()
{
    QStringList deviceNames;
    foreach (const CoreConDevice *device, coreConServer->devices())
        deviceNames.append(device->name());
    return deviceNames;
}

AppxPhoneEngine::AppxPhoneEngine(Runner *runner)
    : AppxEngine(runner, new AppxPhoneEnginePrivate)
{
    Q_D(AppxPhoneEngine);
    if (d->hasFatalError)
        return;
    d->hasFatalError = true;

    ComPtr<IStream> manifestStream;
    HRESULT hr;
    if (d->manifestReader) {
        hr = d->manifestReader->GetStream(&manifestStream);
        RETURN_VOID_IF_FAILED("Failed to query manifest stream from manifest reader.");
    } else {
        hr = SHCreateStreamOnFile(wchar(d->manifest), STGM_READ, &manifestStream);
        RETURN_VOID_IF_FAILED("Failed to open manifest stream");
    }

    if (!getPhoneProductId(manifestStream.Get(), &d->productId)) {
        qCWarning(lcWinRtRunner) << "Failed to read phone product ID from the manifest.";
        return;
    }

    if (!coreConServer->initialize()) {
        while (!coreConServer.exists())
            Sleep(1);
    }

    // Get the device
    d->device = coreConServer->devices().value(d->runner->deviceIndex());
    if (!d->device || !d->device->handle()) {
        d->hasFatalError = true;
        qCWarning(lcWinRtRunner) << "Invalid device specified:" << d->runner->deviceIndex();
        return;
    }



    // Set a break handler for gracefully exiting from long-running operations
    SetConsoleCtrlHandler(&ctrlHandler, true);
    d->hasFatalError = false;
}

AppxPhoneEngine::~AppxPhoneEngine()
{
}

QString AppxPhoneEngine::extensionSdkPath() const
{
#if _MSC_VER >= 1900
    const QByteArray extensionSdkDirRaw = qgetenv("ExtensionSdkDir");
    if (extensionSdkDirRaw.isEmpty()) {
        qCWarning(lcWinRtRunner) << "The environment variable ExtensionSdkDir is not set.";
        return QString();
    }
    return QString::fromLocal8Bit(extensionSdkDirRaw);
#else // _MSC_VER < 1900
    HKEY regKey;
    LONG hr = RegOpenKeyEx(
                HKEY_LOCAL_MACHINE,
                L"SOFTWARE\\Wow6432Node\\Microsoft\\Microsoft SDKs\\WindowsPhoneApp\\v8.1",
                0, KEY_READ, &regKey);
    if (hr != ERROR_SUCCESS) {
        qCWarning(lcWinRtRunner) << "Failed to open registry key:" << qt_error_string(hr);
        return QString();
    }

    wchar_t pathData[MAX_PATH];
    DWORD pathLength = MAX_PATH;
    hr = RegGetValue(regKey, L"Install Path", L"Install Path", RRF_RT_REG_SZ, NULL, pathData, &pathLength);
    if (hr != ERROR_SUCCESS) {
        qCWarning(lcWinRtRunner) << "Failed to get installation path value:" << qt_error_string(hr);
        return QString();
    }

    return QString::fromWCharArray(pathData, (pathLength - 1) / sizeof(wchar_t))
            + QLatin1String("ExtensionSDKs");
#endif // _MSC_VER < 1900
}

bool AppxPhoneEngine::installPackage(IAppxManifestReader *reader, const QString &filePath)
{
    Q_D(AppxPhoneEngine);
    qCDebug(lcWinRtRunner) << __FUNCTION__ << filePath;

    ComPtr<ICcConnection3> connection;
    HRESULT hr = d->connection.As(&connection);
    RETURN_FALSE_IF_FAILED("Failed to obtain connection object");

    ComPtr<IStream> manifestStream;
    hr = reader->GetStream(&manifestStream);
    RETURN_FALSE_IF_FAILED("Failed to get manifest stream from reader");

    QString productIdString;
    if (!getPhoneProductId(manifestStream.Get(), &productIdString)) {
        qCWarning(lcWinRtRunner) << "Failed to get phone product ID from manifest reader.";
        return false;
    }
    _bstr_t productId(wchar(productIdString));

    VARIANT_BOOL isInstalled;
    hr = connection->IsApplicationInstalled(productId, &isInstalled);
    RETURN_FALSE_IF_FAILED("Failed to determine if package is installed");
    if (isInstalled) {
        qCDebug(lcWinRtRunner) << "Package" << productIdString << "is already installed";
        return true;
    }

    ComPtr<IAppxManifestProperties> properties;
    hr = reader->GetProperties(&properties);
    RETURN_FALSE_IF_FAILED("Failed to get manifest properties");

    BOOL isFramework;
    hr = properties->GetBoolValue(L"Framework", &isFramework);
    RETURN_FALSE_IF_FAILED("Failed to determine whether package is a framework");

    const QString deploymentFlags = QString::number(isFramework ? PhoneTools::None : PhoneTools::Sideload);
    _bstr_t deploymentFlagsAsGenre(wchar(deploymentFlags));
    const QString packageType = QString::number(isFramework ? PhoneTools::Framework : PhoneTools::Main);
    _bstr_t packageTypeAsIconPath(wchar(packageType));
    _bstr_t packagePath(wchar(QDir::toNativeSeparators(filePath)));
    hr = connection->InstallApplication(productId, productId, deploymentFlagsAsGenre,
                                        packageTypeAsIconPath, packagePath);
    if (hr == 0x80073d06) { // No public E_* macro available
        qCWarning(lcWinRtRunner) << "Found a newer version of " << filePath
                                 << " on the target device, skipping...";
    } else {
        RETURN_FALSE_IF_FAILED("Failed to install the package");
    }

    return true;
}

bool AppxPhoneEngine::connect()
{
    Q_D(AppxPhoneEngine);
    qCDebug(lcWinRtRunner) << __FUNCTION__;

    HRESULT hr;
    if (!d->connection) {
        _bstr_t connectionName;
        hr = static_cast<ICcServer *>(coreConServer->handle())->GetConnection(
                    static_cast<ICcDevice *>(d->device->handle()), 5000, NULL, connectionName.GetAddress(), &d->connection);
        RETURN_FALSE_IF_FAILED("Failed to connect to device");
    }

    VARIANT_BOOL connected;
    hr = d->connection->IsConnected(&connected);
    RETURN_FALSE_IF_FAILED("Failed to determine connection state");
    if (connected)
        return true;

    hr = d->connection->ConnectDevice();
    RETURN_FALSE_IF_FAILED("Failed to connect to device");

    return true;
}

bool AppxPhoneEngine::install(bool removeFirst)
{
    Q_D(AppxPhoneEngine);
    qCDebug(lcWinRtRunner) << __FUNCTION__;

    if (!connect())
        return false;

    ComPtr<ICcConnection3> connection;
    HRESULT hr = d->connection.As(&connection);
    RETURN_FALSE_IF_FAILED("Failed to obtain connection object");

    _bstr_t productId(wchar(d->productId));
    VARIANT_BOOL isInstalled;
    hr = connection->IsApplicationInstalled(productId, &isInstalled);
    RETURN_FALSE_IF_FAILED("Failed to obtain the installation status");
    if (isInstalled) {
        if (!removeFirst)
            return true;
        if (!remove())
            return false;
    }

    if (!installDependencies())
        return false;

    const QDir base = QFileInfo(d->executable).absoluteDir();
    const bool existingPackage = d->runner->app().endsWith(QLatin1String(".appx"));
    const QString packageFileName = existingPackage
                                      ? d->runner->app()
                                      : base.absoluteFilePath(d->packageFamilyName + QStringLiteral(".appx"));
    if (!existingPackage) {
        if (!createPackage(packageFileName))
            return false;

        if (!sign(packageFileName))
            return false;
    } else {
        qCDebug(lcWinRtRunner) << "Installing existing package.";
    }

    return installPackage(d->manifestReader.Get(), packageFileName);
}

bool AppxPhoneEngine::remove()
{
    Q_D(AppxPhoneEngine);
    qCDebug(lcWinRtRunner) << __FUNCTION__;

    if (!connect())
        return false;

    if (!d->connection)
        return false;

    ComPtr<ICcConnection3> connection;
    HRESULT hr = d->connection.As(&connection);
    RETURN_FALSE_IF_FAILED("Failed to obtain connection object");

    _bstr_t app = wchar(d->productId);
    hr = connection->UninstallApplication(app);
    RETURN_FALSE_IF_FAILED("Failed to uninstall the package");

    return true;
}

bool AppxPhoneEngine::start()
{
    Q_D(AppxPhoneEngine);
    qCDebug(lcWinRtRunner) << __FUNCTION__;

    if (!connect())
        return false;

    if (!d->runner->arguments().isEmpty())
        qCWarning(lcWinRtRunner) << "Arguments are not currently supported for Windows Phone Appx packages.";

    ComPtr<ICcConnection3> connection;
    HRESULT hr = d->connection.As(&connection);
    RETURN_FALSE_IF_FAILED("Failed to cast connection object");

    _bstr_t productId(wchar(d->productId));
    DWORD pid;
    hr = connection->LaunchApplication(productId, &pid);
    RETURN_FALSE_IF_FAILED("Failed to start the package");

    d->pid = pid;
    return true;
}

bool AppxPhoneEngine::enableDebugging(const QString &debuggerExecutable, const QString &debuggerArguments)
{
    qCDebug(lcWinRtRunner) << __FUNCTION__;
    Q_UNUSED(debuggerExecutable);
    Q_UNUSED(debuggerArguments);
    return false;
}

bool AppxPhoneEngine::disableDebugging()
{
    qCDebug(lcWinRtRunner) << __FUNCTION__;
    return false;
}

bool AppxPhoneEngine::suspend()
{
    qCDebug(lcWinRtRunner) << __FUNCTION__;
    return false;
}

bool AppxPhoneEngine::waitForFinished(int secs)
{
    Q_D(AppxPhoneEngine);
    qCDebug(lcWinRtRunner) << __FUNCTION__;

    ComPtr<ICcConnection3> connection;
    HRESULT hr = d->connection.As(&connection);
    RETURN_FALSE_IF_FAILED("Failed to cast connection");

    g_handleCtrl = true;
    int time = 0;
    forever {
        ++time;
        if ((secs && time > secs) || g_ctrlReceived) {
            g_handleCtrl = false;
            return false;
        }

        Sleep(1000); // Wait one second between checks
        qCDebug(lcWinRtRunner) << "Waiting for app to quit - msecs to go: " << secs - time;
    }
    g_handleCtrl = false;
    return true;
}

bool AppxPhoneEngine::stop()
{
    qCDebug(lcWinRtRunner) << __FUNCTION__;

    if (!connect())
        return false;

#if 0 // This does not actually stop the app - QTBUG-41946
    Q_D(AppxPhoneEngine);
    ComPtr<ICcConnection3> connection;
    HRESULT hr = d->connection.As(&connection);
    RETURN_FALSE_IF_FAILED("Failed to cast connection object");

    _bstr_t productId(wchar(d->productId));
    hr = connection->TerminateRunningApplicationInstances(productId);
    RETURN_FALSE_IF_FAILED("Failed to stop the package");

    return true;
#else
    return remove();
#endif
}

QString AppxPhoneEngine::devicePath(const QString &relativePath) const
{
    Q_D(const AppxPhoneEngine);
    qCDebug(lcWinRtRunner) << __FUNCTION__;

    return QStringLiteral("%FOLDERID_APPID_ISOROOT%\\") + d->productId
            + QStringLiteral("\\%LOCL%\\") + relativePath;
}

bool AppxPhoneEngine::sendFile(const QString &localFile, const QString &deviceFile)
{
    Q_D(const AppxPhoneEngine);
    qCDebug(lcWinRtRunner) << __FUNCTION__;

    HRESULT hr = d->connection->SendFile(_bstr_t(wchar(localFile)), _bstr_t(wchar(deviceFile)),
                                         CREATE_ALWAYS, NULL);
    RETURN_FALSE_IF_FAILED("Failed to send the file");

    return true;
}

bool AppxPhoneEngine::receiveFile(const QString &deviceFile, const QString &localFile)
{
    Q_D(const AppxPhoneEngine);
    qCDebug(lcWinRtRunner) << __FUNCTION__;

    HRESULT hr = d->connection->ReceiveFile(_bstr_t(wchar(deviceFile)),
                                            _bstr_t(wchar(localFile)), uint(2));
    RETURN_FALSE_IF_FAILED("Failed to receive the file");

    return true;
}

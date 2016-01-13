/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the tools applications of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "xapengine.h"

#include <QtCore/QDir>
#include <QtCore/QDirIterator>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QHash>
#include <QtCore/QUuid>
#include <QtCore/QLoggingCategory>
#ifndef RTRUNNER_NO_ZIP
#  include <qzipwriter_p.h>
#endif

#include <comdef.h>
#include <psapi.h>
#include <wrl.h>
using namespace Microsoft::WRL;

QT_USE_NAMESPACE

#include <ccapi_11.h>
#include <corecon.h>
Q_GLOBAL_STATIC_WITH_ARGS(CoreConServer, coreConServer, (11))

#define wchar(str) reinterpret_cast<LPCWSTR>(str.utf16())

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

class XapEnginePrivate
{
public:
    Runner *runner;
    bool hasFatalError;

    QString manifest;
    QString genre;
    QString productId;
    QString executable;
    QString relativeExecutable;
    QStringList icons;
    qint64 pid;
    DWORD exitCode;

    ComPtr<ICcConnection> connection;
    CoreConDevice *device;
    QHash<QString, QString> installedApps;
};

static inline bool getManifestFile(const QString &fileName, QString *manifest = 0)
{
    if (!QFile::exists(fileName)) {
        qCWarning(lcWinRtRunner) << fileName << "doesn't exist.";
        return false;
    }

    // If it looks like an winphone manifest, we're done
    if (fileName.endsWith(QStringLiteral("WMAppManifest.xml"))) {
        if (manifest)
            *manifest = fileName;
        return true;
    }

    // If it looks like an executable, check that manifest is next to it
    if (fileName.endsWith(QStringLiteral(".exe"))) {
        const QString manifestFileName = QFileInfo(fileName).absoluteDir()
                .absoluteFilePath(QStringLiteral("WMAppManifest.xml"));
        if (!QFile::exists(manifestFileName)) {
            qCWarning(lcWinRtRunner) << fileName << "doesn't exist.";
            return false;
        }
        if (manifest)
            *manifest = manifestFileName;
        return true;
    }

    // TODO: handle already-built package as well

    return false;
}

bool XapEngine::canHandle(Runner *runner)
{
    return getManifestFile(runner->app());
}

RunnerEngine *XapEngine::create(Runner *runner)
{
    XapEngine *engine = new XapEngine(runner);
    if (engine->d_ptr->hasFatalError) {
        delete engine;
        return 0;
    }

    return engine;
}

QStringList XapEngine::deviceNames()
{
    QStringList deviceNames;
    foreach (const CoreConDevice *device, coreConServer->devices())
        deviceNames.append(device->name());
    return deviceNames;
}

XapEngine::XapEngine(Runner *runner)
    : d_ptr(new XapEnginePrivate)
{
    Q_D(XapEngine);
    d->runner = runner;
    d->hasFatalError = false;
    d->pid = -1;
    d->exitCode = UINT_MAX;

    if (!getManifestFile(runner->app(), &d->manifest)) {
        d->hasFatalError = true;
        qCWarning(lcWinRtRunner) << "Unable to determine manifest file from" << runner->app();
        return;
    }

    // Examine manifest
    QFile manifestFile(d->manifest);
    if (!manifestFile.open(QFile::ReadOnly)) {
        qCWarning(lcWinRtRunner) << "Unable to open manifest:" << manifestFile.errorString();
        return;
    }
    const QString contents = QString::fromUtf8(manifestFile.readAll());

    // Product ID
    QRegExp productIdPattern(QStringLiteral("ProductID=\"(\\{[a-fA-F0-9]{8}-[a-fA-F0-9]{4}-[a-fA-F0-9]{4}-[a-fA-F0-9]{4}-[a-fA-F0-9]{12}\\})\""));
    if (productIdPattern.indexIn(contents) < 0) {
        d->hasFatalError = true;
        qCWarning(lcWinRtRunner) << "Unable to determine product ID in manifest:" << d->manifest;
        return;
    }
    d->productId = productIdPattern.cap(1);

    // Executable
    QRegExp executablePattern(QStringLiteral("ImagePath=\"([a-zA-Z0-9_-]*\\.exe)\""));
    if (executablePattern.indexIn(contents) < 0) {
        d->hasFatalError = true;
        qCWarning(lcWinRtRunner) << "Unable to determine executable in manifest: " << d->manifest;
        return;
    }
    d->relativeExecutable = executablePattern.cap(1);
    d->executable = QFileInfo(d->manifest).absoluteDir().absoluteFilePath(d->relativeExecutable);

    // Icons
    QRegExp iconPattern(QStringLiteral("[\\\\/a-zA-Z0-9_\\-\\!]*\\.(png|jpg|jpeg)"));
    int iconIndex = 0;
    while ((iconIndex = iconPattern.indexIn(contents, iconIndex)) >= 0) {
        const QString icon = iconPattern.cap(0);
        d->icons.append(icon);
        iconIndex += icon.length();
    }
    // At least the first icon is required
    if (d->icons.isEmpty()) {
        d->hasFatalError = true;
        qCWarning(lcWinRtRunner) << "The application icon was not specified in the manifest.";
        return;
    }

    // Genre
    QRegExp genrePattern(QStringLiteral("Genre=\"(apps\\.(?:normal|games))\""));
    if (genrePattern.indexIn(contents) < 0) {
        d->hasFatalError = true;
        qCWarning(lcWinRtRunner) << "The application genre was not specified in the manifest.";
        return;
    }
    d->genre = genrePattern.cap(1);

    // Get the device
    d->device = coreConServer->devices().value(d->runner->deviceIndex());
    if (!d->device->handle()) {
        d->hasFatalError = true;
        qCWarning(lcWinRtRunner) << "Invalid device specified." << d->manifest;
        return;
    }

    // Set a break handler for gracefully exiting from long-running operations
    SetConsoleCtrlHandler(&ctrlHandler, true);
}

XapEngine::~XapEngine()
{
}

bool XapEngine::connect()
{
    Q_D(XapEngine);
    qCDebug(lcWinRtRunner) << __FUNCTION__;

    HRESULT hr;
    if (!d->connection) {
        _bstr_t connectionName;
        hr = static_cast<ICcServer *>(coreConServer->handle())->GetConnection(
                    static_cast<ICcDevice *>(d->device->handle()), 5000, NULL, connectionName.GetAddress(), &d->connection);
        if (FAILED(hr)) {
            qCWarning(lcWinRtRunner) << "Unable to connect to device." << coreConServer->formatError(hr);
            return false;
        }
    }

    VARIANT_BOOL connected;
    hr = d->connection->IsConnected(&connected);
    if (FAILED(hr)) {
        qCWarning(lcWinRtRunner) << "Unable to determine connection state." << coreConServer->formatError(hr);
        return false;
    }
    if (connected)
        return true;

    hr = d->connection->ConnectDevice();
    if (FAILED(hr)) {
        qCWarning(lcWinRtRunner) << "Unable to connect to device." << coreConServer->formatError(hr);
        return false;
    }

    // Enumerate apps for later comparison
    ComPtr<ICcConnection3> connection;
    hr = d->connection.As(&connection);
    if (FAILED(hr)) {
        qCWarning(lcWinRtRunner) << "Unable to obtain connection object." << coreConServer->formatError(hr);
        return false;
    }

    SAFEARRAY *productIds, *instanceIds;
    hr = connection->GetInstalledApplicationIDs(&productIds, &instanceIds);
    if (FAILED(hr)) {
        qCWarning(lcWinRtRunner) << "Unable to get installed applications" << coreConServer->formatError(hr);
        return false;
    }
    d->installedApps.clear();
    if (productIds && instanceIds && productIds->rgsabound[0].cElements == instanceIds->rgsabound[0].cElements) {
        for (ulong i = 0; i < productIds->rgsabound[0].cElements; ++i) {
            LONG indices[] = { i };
            _bstr_t productId;
            _bstr_t instanceId;
            if (SUCCEEDED(SafeArrayGetElement(productIds, indices, productId.GetAddress()))
                    && SUCCEEDED(SafeArrayGetElement(instanceIds, indices, instanceId.GetAddress()))) {
                d->installedApps.insert(QString::fromWCharArray(productId),
                                        QString::fromWCharArray(instanceId));
            }
        }
        SafeArrayDestroy(productIds);
        SafeArrayDestroy(instanceIds);
    }
    return SUCCEEDED(hr);
}

uint XapEngine::fetchPid()
{
    Q_D(XapEngine);
    qCDebug(lcWinRtRunner) << __FUNCTION__;

    ComPtr<ICcConnection4> connection;
    HRESULT hr = d->connection.As(&connection);

    // Fetch the PID file from the remote directory
    QString dir = devicePath(QString());
    _bstr_t remoteDir(wchar(dir));
    SAFEARRAY *listing;
    hr = connection->GetDirectoryListing(remoteDir, &listing);
    if (FAILED(hr)) {
        qCWarning(lcWinRtRunner) << "Unable to obtain directory listing.";
        return 0;
    }
    uint pid = 0;
    if (listing) {
        for (ulong i = 0; i < listing->rgsabound[0].cElements; ++i) {
            LONG indices[] = { i };
            _bstr_t fileName;
            if (SUCCEEDED(SafeArrayGetElement(listing, indices, fileName.GetAddress()))) {
                QString possiblePidFile = QString::fromWCharArray(fileName);
                if (possiblePidFile.endsWith(QStringLiteral(".pid"))) {
                    bool ok;
                    pid = possiblePidFile.mid(0, possiblePidFile.length() - 4).toUInt(&ok);
                    if (ok) {
                        QString pidFile = dir + QLatin1Char('\\') + possiblePidFile;
                        FileInfo fileInfo;
                        hr = d->connection->GetFileInfo(_bstr_t(wchar(pidFile)), &fileInfo);
                        if (FAILED(hr)) {
                            qCWarning(lcWinRtRunner) << "Unable to obtain file info for the process ID.";
                            return 0;
                        }
                        d->pid = pid;
                        break;
                    }
                }
            }
        }
    }
    return pid;
}

bool XapEngine::install(bool removeFirst)
{
    Q_D(XapEngine);
    qCDebug(lcWinRtRunner) << __FUNCTION__;

    if (!connect())
        return false;

    ComPtr<ICcConnection3> connection;
    HRESULT hr = d->connection.As(&connection);
    if (FAILED(hr)) {
        qCWarning(lcWinRtRunner) << "Unable to obtain connection object." << coreConServer->formatError(hr);
        return false;
    }

    _bstr_t app = wchar(d->productId);
    VARIANT_BOOL isInstalled;
    hr = connection->IsApplicationInstalled(app, &isInstalled);
    if (isInstalled) {
        if (!removeFirst)
            return true;
        if (!remove())
            return false;
    }

    // Check for package map, or create one if needed
    QDir base = QFileInfo(d->manifest).absoluteDir();
    QFile xapFile(base.absoluteFilePath(d->productId + QStringLiteral(".xap")));
    if (!xapFile.open(QFile::ReadWrite)) {
        qCWarning(lcWinRtRunner) << "Unable to open Xap for writing: " << xapFile.errorString();
        return false;
    }

    QHash<QString, QString> xapFiles;
    QFile mappingFile(base.absoluteFilePath(QStringLiteral("WMAppManifest.map")));
    if (mappingFile.exists()) {
        if (mappingFile.open(QFile::ReadOnly)) {
            QRegExp pattern(QStringLiteral("^\"([^\"]*)\"\\s*\"([^\"]*)\"$"));
            bool inFileSection = false;
            while (!mappingFile.atEnd()) {
                const QString line = QString::fromUtf8(mappingFile.readLine()).trimmed();
                if (line.startsWith(QLatin1Char('['))) {
                    inFileSection = line == QStringLiteral("[Files]");
                    continue;
                }
                if (inFileSection && pattern.indexIn(line) >= 0 && pattern.captureCount() == 2)
                    xapFiles.insert(pattern.cap(1), pattern.cap(2));
            }
        } else {
            qCWarning(lcWinRtRunner) << "Found, but unable to open mapping file.";
        }
    } else {
        qCWarning(lcWinRtRunner) << "No mapping file exists. Only recognized files will be packaged.";
        // Add manifest
        xapFiles.insert(d->manifest, QFileInfo(d->manifest).fileName());
        // Add executable
        xapFiles.insert(d->executable, QFileInfo(d->executable).fileName());
        // Add icons
        foreach (const QString &icon, d->icons)
            xapFiles.insert(base.absoluteFilePath(icon), icon);
        // Add potential Qt files
        const QStringList fileTypes = QStringList()
                << QStringLiteral("*.dll") << QStringLiteral("*.png") << QStringLiteral("*.qm")
                << QStringLiteral("*.qml") << QStringLiteral("*.qmldir");
        QDirIterator dirIterator(base.absolutePath(), fileTypes, QDir::Files, QDirIterator::Subdirectories);
        while (dirIterator.hasNext()) {
            const QString filePath = dirIterator.next();
            xapFiles.insert(filePath, base.relativeFilePath(filePath));
        }
    }

    QZipWriter xapPackage(&xapFile);
    for (QHash<QString, QString>::const_iterator i = xapFiles.begin(); i != xapFiles.end(); ++i) {
        QFile file(i.key());
        if (!file.open(QFile::ReadOnly))
            continue;

        if (QFileInfo(i.key()) == QFileInfo(d->manifest)) {
            const QStringList args = QStringList(d->relativeExecutable)
                    << d->runner->arguments() << QStringLiteral("-qdevel");
            QByteArray manifestWithArgs = file.readAll();
            manifestWithArgs.replace(QByteArrayLiteral("ImageParams=\"\""),
                                     QByteArrayLiteral("ImageParams=\"")
                                     + args.join(QLatin1Char(' ')).toUtf8()
                                     + '"');
            xapPackage.addFile(i.value(), manifestWithArgs);
            continue;
        }
        xapPackage.addFile(i.value(), &file);
    }
    xapPackage.close();

    _bstr_t productId(wchar(d->productId));
    QString uuid = QUuid::createUuid().toString();
    _bstr_t instanceId(wchar(uuid));
    _bstr_t genre(wchar(d->genre));
    _bstr_t iconPath(wchar(QDir::toNativeSeparators(base.absoluteFilePath(d->icons.first()))));
    _bstr_t xapPath(wchar(QDir::toNativeSeparators(xapFile.fileName())));
    hr = connection->InstallApplication(productId, instanceId, genre, iconPath, xapPath);
    if (FAILED(hr)) {
        qCWarning(lcWinRtRunner) << "Unable to install application." << coreConServer->formatError(hr);
        return false;
    }
    d->installedApps.insert(d->productId, uuid);
    return true;
}

bool XapEngine::remove()
{
    Q_D(XapEngine);
    qCDebug(lcWinRtRunner) << __FUNCTION__;

    if (!connect())
        return false;

    if (!d->connection)
        return false;

    ComPtr<ICcConnection3> connection;
    HRESULT hr = d->connection.As(&connection);
    if (FAILED(hr)) {
        qCWarning(lcWinRtRunner) << "Unable to obtain connection object." << coreConServer->formatError(hr);
        return false;
    }

    _bstr_t app = wchar(d->productId);
    hr = connection->UninstallApplication(app);
    if (FAILED(hr)) {
        qCWarning(lcWinRtRunner) << "Unable to uninstall the package." << coreConServer->formatError(hr);
        return false;
    }

    return true;
}

bool XapEngine::start()
{
    Q_D(XapEngine);
    qCDebug(lcWinRtRunner) << __FUNCTION__;

    if (!connect())
        return false;

    ComPtr<ICcConnection3> connection;
    HRESULT hr = d->connection.As(&connection);
    if (FAILED(hr)) {
        qCWarning(lcWinRtRunner) << "Unable to obtain connection object." << coreConServer->formatError(hr);
        return false;
    }

    _bstr_t productId(wchar(d->productId));
    DWORD pid = 0;
    hr = connection->LaunchApplication(productId, &pid);
    if (FAILED(hr)) {
        qCWarning(lcWinRtRunner) << "Unable to start the package." << coreConServer->formatError(hr);
        return false;
    }

    // Wait up to 5 seconds for PID to appear
    int count = 0;
    while (!pid && count++ < 50) {
        Sleep(100); // Normally appears within first 100ms
        pid = fetchPid();
    }
    if (pid)
        d->pid = pid;

    return true;
}

bool XapEngine::enableDebugging(const QString &debuggerExecutable, const QString &debuggerArguments)
{
    Q_UNUSED(debuggerExecutable);
    Q_UNUSED(debuggerArguments);
    return false;
}

bool XapEngine::disableDebugging()
{
    return false;
}

bool XapEngine::suspend()
{
    qCDebug(lcWinRtRunner) << __FUNCTION__;

    return false;
}

bool XapEngine::waitForFinished(int secs)
{
    qCDebug(lcWinRtRunner) << __FUNCTION__;

    g_handleCtrl = true;
    int time = 0;
    forever {
        if (!fetchPid())
            break;

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

bool XapEngine::stop()
{
    Q_D(XapEngine);
    qCDebug(lcWinRtRunner) << __FUNCTION__;

    if (!connect())
        return false;

    ComPtr<ICcConnection3> connection;
    HRESULT hr = d->connection.As(&connection);
    if (FAILED(hr)) {
        qCWarning(lcWinRtRunner) << "Unable to obtain connection object." << coreConServer->formatError(hr);
        return false;
    }

    hr = connection->TerminateRunningApplicationInstances(_bstr_t(wchar(d->productId)));
    if (FAILED(hr)) {
        qCWarning(lcWinRtRunner) << "Unable to stop the package." << coreConServer->formatError(hr);
        return false;
    }

    return true;
}

qint64 XapEngine::pid() const
{
    Q_D(const XapEngine);
    qCDebug(lcWinRtRunner) << __FUNCTION__;

    return d->pid;
}

int XapEngine::exitCode() const
{
    // So far, no working implementation
    return -1;
}

QString XapEngine::executable() const
{
    Q_D(const XapEngine);
    qCDebug(lcWinRtRunner) << __FUNCTION__;

    return d->executable;
}

QString XapEngine::devicePath(const QString &relativePath) const
{
    Q_D(const XapEngine);
    qCDebug(lcWinRtRunner) << __FUNCTION__;

    return QStringLiteral("%FOLDERID_APPID_ISOROOT%\\") + d->productId
            + QStringLiteral("\\") + relativePath;
}

bool XapEngine::sendFile(const QString &localFile, const QString &deviceFile)
{
    Q_D(const XapEngine);
    qCDebug(lcWinRtRunner) << __FUNCTION__;

    HRESULT hr = d->connection->SendFile(_bstr_t(wchar(localFile)), _bstr_t(wchar(deviceFile)),
                                         CREATE_ALWAYS, NULL);
    if (FAILED(hr)) {
        qCWarning(lcWinRtRunner) << "Unable to send the file." << coreConServer->formatError(hr);
        return false;
    }

    return true;
}

bool XapEngine::receiveFile(const QString &deviceFile, const QString &localFile)
{
    Q_D(const XapEngine);
    qCDebug(lcWinRtRunner) << __FUNCTION__;

    HRESULT hr = d->connection->ReceiveFile(_bstr_t(wchar(deviceFile)),
                                            _bstr_t(wchar(localFile)), uint(2));
    if (FAILED(hr)) {
        qCWarning(lcWinRtRunner) << "Unable to receive the file." << coreConServer->formatError(hr);
        return false;
    }

    return true;
}

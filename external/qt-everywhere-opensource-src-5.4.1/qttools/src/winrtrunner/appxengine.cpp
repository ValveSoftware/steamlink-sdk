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

#include "appxengine.h"
#include "appxengine_p.h"

#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QDirIterator>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QLoggingCategory>
#include <QtCore/QStandardPaths>

#include <ShlObj.h>
#include <Shlwapi.h>
#include <wsdevlicensing.h>
#include <AppxPackaging.h>
#include <wrl.h>
#include <windows.applicationmodel.h>
#include <windows.management.deployment.h>

using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Management::Deployment;
using namespace ABI::Windows::ApplicationModel;
using namespace ABI::Windows::System;

QT_USE_NAMESPACE

bool AppxEngine::getManifestFile(const QString &fileName, QString *manifest)
{
    if (!QFile::exists(fileName)) {
        qCWarning(lcWinRtRunner) << fileName << "does not exist.";
        return false;
    }

    // If it looks like an appx manifest, we're done
    if (fileName.endsWith(QStringLiteral("AppxManifest.xml"))) {

        if (manifest)
            *manifest = fileName;
        return true;
    }

    // If it looks like an executable, check that manifest is next to it
    if (fileName.endsWith(QStringLiteral(".exe"))) {
        QDir appDir = QFileInfo(fileName).absoluteDir();
        QString manifestFileName = appDir.absoluteFilePath(QStringLiteral("AppxManifest.xml"));
        if (!QFile::exists(manifestFileName)) {
            qCWarning(lcWinRtRunner) << manifestFileName << "does not exist.";
            return false;
        }

        if (manifest)
            *manifest = manifestFileName;
        return true;
    }

    // TODO: handle already-built package as well

    qCWarning(lcWinRtRunner) << "Appx: unable to determine manifest for" << fileName << ".";
    return false;
}

#define CHECK_RESULT(errorMessage, action)\
    do {\
        if (FAILED(hr)) {\
            qCWarning(lcWinRtRunner).nospace() << errorMessage " (0x"\
                                               << QByteArray::number(hr, 16).constData()\
                                               << ' ' << qt_error_string(hr) << ')';\
            action;\
        }\
    } while (false)

#define CHECK_RESULT_FATAL(errorMessage, action)\
    do {CHECK_RESULT(errorMessage, d->hasFatalError = true; action;);} while (false)

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

AppxEngine::AppxEngine(Runner *runner, AppxEnginePrivate *dd)
    : d_ptr(dd)
{
    Q_D(AppxEngine);
    if (d->hasFatalError)
        return;

    d->runner = runner;
    d->processHandle = NULL;
    d->pid = -1;
    d->exitCode = UINT_MAX;

    if (!getManifestFile(runner->app(), &d->manifest)) {
        qCWarning(lcWinRtRunner) << "Unable to determine manifest file from" << runner->app();
        d->hasFatalError = true;
        return;
    }

    HRESULT hr;
    hr = RoGetActivationFactory(HString::MakeReference(RuntimeClass_Windows_Foundation_Uri).Get(),
                                IID_PPV_ARGS(&d->uriFactory));
    CHECK_RESULT_FATAL("Failed to instantiate URI factory.", return);

    hr = CoCreateInstance(CLSID_AppxFactory, nullptr, CLSCTX_INPROC_SERVER,
                          IID_IAppxFactory, &d->packageFactory);
    CHECK_RESULT_FATAL("Failed to instantiate package factory.", return);

    ComPtr<IStream> manifestStream;
    hr = SHCreateStreamOnFile(wchar(d->manifest), STGM_READ, &manifestStream);
    CHECK_RESULT_FATAL("Failed to open manifest stream.", return);

    ComPtr<IAppxManifestReader> manifestReader;
    hr = d->packageFactory->CreateManifestReader(manifestStream.Get(), &manifestReader);
    if (FAILED(hr)) {
        qCWarning(lcWinRtRunner).nospace() << "Failed to instantiate manifest reader. (0x"
                                           << QByteArray::number(hr, 16).constData()
                                           << ' ' << qt_error_string(hr) << ')';
        // ### TODO: read detailed error from event log directly
        if (hr == APPX_E_INVALID_MANIFEST) {
            qCWarning(lcWinRtRunner) << "More information on the error can "
                                      "be found in the event log under "
                                      "Microsoft\\Windows\\AppxPackagingOM";
        }
        d->hasFatalError = true;
        return;
    }

    ComPtr<IAppxManifestPackageId> packageId;
    hr = manifestReader->GetPackageId(&packageId);
    CHECK_RESULT_FATAL("Unable to obtain the package ID from the manifest.", return);

    APPX_PACKAGE_ARCHITECTURE arch;
    hr = packageId->GetArchitecture(&arch);
    CHECK_RESULT_FATAL("Failed to retrieve the app's architecture.", return);
    d->packageArchitecture = toProcessorArchitecture(arch);

    LPWSTR packageFullName;
    hr = packageId->GetPackageFullName(&packageFullName);
    CHECK_RESULT_FATAL("Unable to obtain the package full name from the manifest.", return);
    d->packageFullName = QString::fromWCharArray(packageFullName);
    CoTaskMemFree(packageFullName);

    LPWSTR packageFamilyName;
    hr = packageId->GetPackageFamilyName(&packageFamilyName);
    CHECK_RESULT_FATAL("Unable to obtain the package full family name from the manifest.", return);
    d->packageFamilyName = QString::fromWCharArray(packageFamilyName);
    CoTaskMemFree(packageFamilyName);

    ComPtr<IAppxManifestApplicationsEnumerator> applications;
    hr = manifestReader->GetApplications(&applications);
    CHECK_RESULT_FATAL("Failed to get a list of applications from the manifest.", return);

    BOOL hasCurrent;
    hr = applications->GetHasCurrent(&hasCurrent);
    CHECK_RESULT_FATAL("Failed to iterate over applications in the manifest.", return);

    // For now, we are only interested in the first application
    ComPtr<IAppxManifestApplication> application;
    hr = applications->GetCurrent(&application);
    CHECK_RESULT_FATAL("Failed to access the first application in the manifest.", return);

    LPWSTR executable;
    application->GetStringValue(L"Executable", &executable);
    CHECK_RESULT_FATAL("Failed to retrieve the application executable from the manifest.", return);
    d->executable = QFileInfo(d->manifest).absoluteDir()
            .absoluteFilePath(QString::fromWCharArray(executable));
    CoTaskMemFree(executable);

    ComPtr<IAppxManifestPackageDependenciesEnumerator> dependencies;
    hr = manifestReader->GetPackageDependencies(&dependencies);
    CHECK_RESULT_FATAL("Failed to retrieve the package dependencies from the manifest.", return);

    hr = dependencies->GetHasCurrent(&hasCurrent);
    CHECK_RESULT_FATAL("Failed to iterate over dependencies in the manifest.", return);
    while (SUCCEEDED(hr) && hasCurrent) {
        ComPtr<IAppxManifestPackageDependency> dependency;
        hr = dependencies->GetCurrent(&dependency);
        CHECK_RESULT_FATAL("Failed to access dependency in the manifest.", return);

        LPWSTR name;
        hr = dependency->GetName(&name);
        CHECK_RESULT_FATAL("Failed to access dependency name.", return);
        d->dependencies.insert(QString::fromWCharArray(name));
        CoTaskMemFree(name);
        hr = dependencies->MoveNext(&hasCurrent);
    }
}

AppxEngine::~AppxEngine()
{
    Q_D(const AppxEngine);
    CloseHandle(d->processHandle);
}

qint64 AppxEngine::pid() const
{
    Q_D(const AppxEngine);
    qCDebug(lcWinRtRunner) << __FUNCTION__;

    return d->pid;
}

int AppxEngine::exitCode() const
{
    Q_D(const AppxEngine);
    qCDebug(lcWinRtRunner) << __FUNCTION__;

    return d->exitCode == UINT_MAX ? -1 : HRESULT_CODE(d->exitCode);
}

QString AppxEngine::executable() const
{
    Q_D(const AppxEngine);
    qCDebug(lcWinRtRunner) << __FUNCTION__;

    return d->executable;
}

bool AppxEngine::installDependencies()
{
    Q_D(AppxEngine);
    qCDebug(lcWinRtRunner) << __FUNCTION__;

    QSet<QString> toInstall;
    foreach (const QString &dependencyName, d->dependencies) {
        toInstall.insert(dependencyName);
        qCDebug(lcWinRtRunner).nospace()
            << "dependency to be installed: " << dependencyName;
    }

    if (toInstall.isEmpty())
        return true;

    const QString extensionSdkDir = extensionSdkPath();
    if (!QFile::exists(extensionSdkDir)) {
        qCWarning(lcWinRtRunner).nospace()
                << QString(QStringLiteral("The directory '%1' does not exist.")).arg(
                       QDir::toNativeSeparators(extensionSdkDir));
        return false;
    }
    qCDebug(lcWinRtRunner).nospace()
        << "looking for dependency packages in " << extensionSdkDir;
    QDirIterator dit(extensionSdkDir, QStringList() << QStringLiteral("*.appx"),
                     QDir::Files,
                     QDirIterator::Subdirectories);
    while (dit.hasNext()) {
        dit.next();

        HRESULT hr;
        ComPtr<IStream> inputStream;
        hr = SHCreateStreamOnFileEx(wchar(dit.filePath()),
                                    STGM_READ | STGM_SHARE_EXCLUSIVE,
                                    0, FALSE, NULL, &inputStream);
        CHECK_RESULT("Failed to create input stream for package in ExtensionSdkDir.", continue);

        ComPtr<IAppxPackageReader> packageReader;
        hr = d->packageFactory->CreatePackageReader(inputStream.Get(), &packageReader);
        CHECK_RESULT("Failed to create package reader for package in ExtensionSdkDir.", continue);

        ComPtr<IAppxManifestReader> manifestReader;
        hr = packageReader->GetManifest(&manifestReader);
        CHECK_RESULT("Failed to create manifest reader for package in ExtensionSdkDir.", continue);

        ComPtr<IAppxManifestPackageId> packageId;
        hr = manifestReader->GetPackageId(&packageId);
        CHECK_RESULT("Failed to retrieve package id for package in ExtensionSdkDir.", continue);

        LPWSTR sz;
        hr = packageId->GetName(&sz);
        CHECK_RESULT("Failed to retrieve name from package in ExtensionSdkDir.", continue);
        const QString name = QString::fromWCharArray(sz);
        CoTaskMemFree(sz);

        if (!toInstall.contains(name))
            continue;

        APPX_PACKAGE_ARCHITECTURE arch;
        hr = packageId->GetArchitecture(&arch);
        CHECK_RESULT("Failed to retrieve architecture from package in ExtensionSdkDir.", continue);
        if (d->packageArchitecture != arch)
            continue;

        qCDebug(lcWinRtRunner).nospace()
            << "installing dependency " << name << " from " << dit.filePath();
        if (!installPackage(manifestReader.Get(), dit.filePath())) {
            qCWarning(lcWinRtRunner) << "Failed to install package:" << name;
            return false;
        }
    }

    return true;
}

bool AppxEngine::createPackage(const QString &packageFileName)
{
    Q_D(AppxEngine);

    static QHash<QString, QString> contentTypes;
    if (contentTypes.isEmpty()) {
        contentTypes.insert(QStringLiteral("dll"), QStringLiteral("application/x-msdownload"));
        contentTypes.insert(QStringLiteral("exe"), QStringLiteral("application/x-msdownload"));
        contentTypes.insert(QStringLiteral("png"), QStringLiteral("image/png"));
        contentTypes.insert(QStringLiteral("xml"), QStringLiteral("vnd.ms-appx.manifest+xml"));
    }

    // Check for package map, or create one if needed
    QDir base = QFileInfo(d->manifest).absoluteDir();
    QFile packageFile(packageFileName);

    QHash<QString, QString> files;
    QFile mappingFile(base.absoluteFilePath(QStringLiteral("AppxManifest.map")));
    if (mappingFile.exists()) {
        qCWarning(lcWinRtRunner) << "Creating package from mapping file:" << mappingFile.fileName();
        if (!mappingFile.open(QFile::ReadOnly)) {
            qCWarning(lcWinRtRunner) << "Unable to read mapping file:" << mappingFile.errorString();
            return false;
        }

        QRegExp pattern(QStringLiteral("^\"([^\"]*)\"\\s*\"([^\"]*)\"$"));
        bool inFileSection = false;
        while (!mappingFile.atEnd()) {
            const QString line = QString::fromUtf8(mappingFile.readLine()).trimmed();
            if (line.startsWith(QLatin1Char('['))) {
                inFileSection = line == QStringLiteral("[Files]");
                continue;
            }
            if (pattern.cap(2).compare(QStringLiteral("AppxManifest.xml"), Qt::CaseInsensitive) == 0)
                continue;
            if (inFileSection && pattern.indexIn(line) >= 0 && pattern.captureCount() == 2) {
                QString inputFile = pattern.cap(1);
                if (!QFile::exists(inputFile))
                    inputFile = base.absoluteFilePath(inputFile);
                files.insert(QDir::toNativeSeparators(inputFile), QDir::toNativeSeparators(pattern.cap(2)));
            }
        }
    } else {
        qCWarning(lcWinRtRunner) << "No mapping file exists. Only recognized files will be packaged.";
        // Add executable
        files.insert(QDir::toNativeSeparators(d->executable), QFileInfo(d->executable).fileName());
        // Add potential Qt files
        const QStringList fileTypes = QStringList()
                << QStringLiteral("*.dll") << QStringLiteral("*.png") << QStringLiteral("*.qm")
                << QStringLiteral("*.qml") << QStringLiteral("*.qmldir");
        QDirIterator dirIterator(base.absolutePath(), fileTypes, QDir::Files, QDirIterator::Subdirectories);
        while (dirIterator.hasNext()) {
            const QString filePath = dirIterator.next();
            files.insert(QDir::toNativeSeparators(filePath), QDir::toNativeSeparators(base.relativeFilePath(filePath)));
        }
    }

    ComPtr<IStream> outputStream;
    HRESULT hr = SHCreateStreamOnFile(wchar(packageFile.fileName()), STGM_WRITE|STGM_CREATE, &outputStream);
    RETURN_FALSE_IF_FAILED("Failed to create package file output stream");

    ComPtr<IUri> hashMethod;
    hr = CreateUri(L"http://www.w3.org/2001/04/xmlenc#sha512", Uri_CREATE_CANONICALIZE, 0, &hashMethod);
    RETURN_FALSE_IF_FAILED("Failed to create the has method URI");

    APPX_PACKAGE_SETTINGS packageSettings = { FALSE, hashMethod.Get() };
    ComPtr<IAppxPackageWriter> packageWriter;
    hr = d->packageFactory->CreatePackageWriter(outputStream.Get(), &packageSettings, &packageWriter);
    RETURN_FALSE_IF_FAILED("Failed to create package writer");

    for (QHash<QString, QString>::const_iterator i = files.begin(); i != files.end(); ++i) {
        qCDebug(lcWinRtRunner) << "Packaging" << i.key() << i.value();
        ComPtr<IStream> inputStream;
        hr = SHCreateStreamOnFile(wchar(i.key()), STGM_READ, &inputStream);
        RETURN_FALSE_IF_FAILED("Failed to open file");
        const QString contentType = contentTypes.value(QFileInfo(i.key()).suffix().toLower(),
                                                       QStringLiteral("application/octet-stream"));
        hr = packageWriter->AddPayloadFile(wchar(i.value()), wchar(contentType),
                                           APPX_COMPRESSION_OPTION_NORMAL, inputStream.Get());
        RETURN_FALSE_IF_FAILED("Failed to add payload file");
    }

    // Write out the manifest
    ComPtr<IStream> manifestStream;
    hr = SHCreateStreamOnFile(wchar(d->manifest), STGM_READ, &manifestStream);
    RETURN_FALSE_IF_FAILED("Failed to open manifest for packaging");
    hr = packageWriter->Close(manifestStream.Get());
    RETURN_FALSE_IF_FAILED("Failed to finalize package.");

    return true;
}

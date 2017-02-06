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
#if _MSC_VER >= 1900
#include <wincrypt.h>
#endif

using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Management::Deployment;
using namespace ABI::Windows::ApplicationModel;
using namespace ABI::Windows::System;

QT_USE_NAMESPACE

#if _MSC_VER >= 1900
// *********** Taken from MSDN Example code
// https://msdn.microsoft.com/en-us/library/windows/desktop/jj835834%28v=vs.85%29.aspx?f=255&MSPPError=-2147217396

#define SIGNER_SUBJECT_FILE    0x01
#define SIGNER_NO_ATTR          0x00
#define SIGNER_CERT_POLICY_CHAIN_NO_ROOT    0x08
#define SIGNER_CERT_STORE        0x02

typedef struct _SIGNER_FILE_INFO
{
    DWORD cbSize;
    LPCWSTR pwszFileName;
    HANDLE hFile;
} SIGNER_FILE_INFO;

typedef struct _SIGNER_BLOB_INFO
{
    DWORD cbSize;
    GUID *pGuidSubject;
    DWORD cbBlob;
    BYTE *pbBlob;
    LPCWSTR pwszDisplayName;
} SIGNER_BLOB_INFO;

typedef struct _SIGNER_SUBJECT_INFO
{
    DWORD cbSize;
    DWORD *pdwIndex;
    DWORD dwSubjectChoice;
    union
    {
        SIGNER_FILE_INFO *pSignerFileInfo;
        SIGNER_BLOB_INFO *pSignerBlobInfo;
    };
} SIGNER_SUBJECT_INFO, *PSIGNER_SUBJECT_INFO;

typedef struct _SIGNER_ATTR_AUTHCODE
{
    DWORD cbSize;
    BOOL fCommercial;
    BOOL fIndividual;
    LPCWSTR pwszName;
    LPCWSTR pwszInfo;
} SIGNER_ATTR_AUTHCODE;

typedef struct _SIGNER_SIGNATURE_INFO
{
    DWORD cbSize;
    ALG_ID algidHash;
    DWORD dwAttrChoice;
    union
    {
        SIGNER_ATTR_AUTHCODE *pAttrAuthcode;
    };
    PCRYPT_ATTRIBUTES psAuthenticated;
    PCRYPT_ATTRIBUTES psUnauthenticated;
} SIGNER_SIGNATURE_INFO, *PSIGNER_SIGNATURE_INFO;

typedef struct _SIGNER_PROVIDER_INFO
{
    DWORD cbSize;
    LPCWSTR pwszProviderName;
    DWORD dwProviderType;
    DWORD dwKeySpec;
    DWORD dwPvkChoice;
    union
    {
        LPWSTR pwszPvkFileName;
        LPWSTR pwszKeyContainer;
    };
} SIGNER_PROVIDER_INFO, *PSIGNER_PROVIDER_INFO;

typedef struct _SIGNER_SPC_CHAIN_INFO
{
    DWORD cbSize;
    LPCWSTR pwszSpcFile;
    DWORD dwCertPolicy;
    HCERTSTORE hCertStore;
} SIGNER_SPC_CHAIN_INFO;

typedef struct _SIGNER_CERT_STORE_INFO
{
    DWORD cbSize;
    PCCERT_CONTEXT pSigningCert;
    DWORD dwCertPolicy;
    HCERTSTORE hCertStore;
} SIGNER_CERT_STORE_INFO;

typedef struct _SIGNER_CERT
{
    DWORD cbSize;
    DWORD dwCertChoice;
    union
    {
        LPCWSTR pwszSpcFile;
        SIGNER_CERT_STORE_INFO *pCertStoreInfo;
        SIGNER_SPC_CHAIN_INFO *pSpcChainInfo;
    };
    HWND hwnd;
} SIGNER_CERT, *PSIGNER_CERT;

typedef struct _SIGNER_CONTEXT
{
    DWORD cbSize;
    DWORD cbBlob;
    BYTE *pbBlob;
} SIGNER_CONTEXT, *PSIGNER_CONTEXT;

typedef struct _SIGNER_SIGN_EX2_PARAMS
{
    DWORD dwFlags;
    PSIGNER_SUBJECT_INFO pSubjectInfo;
    PSIGNER_CERT pSigningCert;
    PSIGNER_SIGNATURE_INFO pSignatureInfo;
    PSIGNER_PROVIDER_INFO pProviderInfo;
    DWORD dwTimestampFlags;
    PCSTR pszAlgorithmOid;
    PCWSTR pwszTimestampURL;
    PCRYPT_ATTRIBUTES pCryptAttrs;
    PVOID pSipData;
    PSIGNER_CONTEXT *pSignerContext;
    PVOID pCryptoPolicy;
    PVOID pReserved;
} SIGNER_SIGN_EX2_PARAMS, *PSIGNER_SIGN_EX2_PARAMS;

typedef struct _APPX_SIP_CLIENT_DATA
{
    PSIGNER_SIGN_EX2_PARAMS pSignerParams;
    IUnknown* pAppxSipState;
} APPX_SIP_CLIENT_DATA, *PAPPX_SIP_CLIENT_DATA;

bool signAppxPackage(PCCERT_CONTEXT signingCertContext, LPCWSTR packageFilePath)
{
    HRESULT hr = S_OK;

    DWORD signerIndex = 0;

    SIGNER_FILE_INFO fileInfo = {};
    fileInfo.cbSize = sizeof(SIGNER_FILE_INFO);
    fileInfo.pwszFileName = packageFilePath;

    SIGNER_SUBJECT_INFO subjectInfo = {};
    subjectInfo.cbSize = sizeof(SIGNER_SUBJECT_INFO);
    subjectInfo.pdwIndex = &signerIndex;
    subjectInfo.dwSubjectChoice = SIGNER_SUBJECT_FILE;
    subjectInfo.pSignerFileInfo = &fileInfo;

    SIGNER_CERT_STORE_INFO certStoreInfo = {};
    certStoreInfo.cbSize = sizeof(SIGNER_CERT_STORE_INFO);
    certStoreInfo.dwCertPolicy = SIGNER_CERT_POLICY_CHAIN_NO_ROOT;
    certStoreInfo.pSigningCert = signingCertContext;

    SIGNER_CERT cert = {};
    cert.cbSize = sizeof(SIGNER_CERT);
    cert.dwCertChoice = SIGNER_CERT_STORE;
    cert.pCertStoreInfo = &certStoreInfo;

    // The algidHash of the signature to be created must match the
    // hash algorithm used to create the app package
    SIGNER_SIGNATURE_INFO signatureInfo = {};
    signatureInfo.cbSize = sizeof(SIGNER_SIGNATURE_INFO);
    signatureInfo.algidHash = CALG_SHA_512;
    signatureInfo.dwAttrChoice = SIGNER_NO_ATTR;

    SIGNER_SIGN_EX2_PARAMS signerParams = {};
    signerParams.pSubjectInfo = &subjectInfo;
    signerParams.pSigningCert = &cert;
    signerParams.pSignatureInfo = &signatureInfo;

    APPX_SIP_CLIENT_DATA sipClientData = {};
    sipClientData.pSignerParams = &signerParams;
    signerParams.pSipData = &sipClientData;

    // Type definition for invoking SignerSignEx2 via GetProcAddress
    typedef HRESULT (WINAPI *SignerSignEx2Function)(
        DWORD,
        PSIGNER_SUBJECT_INFO,
        PSIGNER_CERT,
        PSIGNER_SIGNATURE_INFO,
        PSIGNER_PROVIDER_INFO,
        DWORD,
        PCSTR,
        PCWSTR,
        PCRYPT_ATTRIBUTES,
        PVOID,
        PSIGNER_CONTEXT *,
        PVOID,
        PVOID);

    // Load the SignerSignEx2 function from MSSign32.dll
    HMODULE msSignModule = LoadLibraryEx(
        L"MSSign32.dll",
        NULL,
        LOAD_LIBRARY_SEARCH_SYSTEM32);

    if (!msSignModule) {
        qCWarning(lcWinRtRunner) << "LoadLibraryEx failed to load MSSign32.dll.";
        return false;
    }

    SignerSignEx2Function SignerSignEx2 = reinterpret_cast<SignerSignEx2Function>(
                                          GetProcAddress(msSignModule, "SignerSignEx2"));
    if (!SignerSignEx2) {
        qCWarning(lcWinRtRunner) << "Could not resolve SignerSignEx2";
        FreeLibrary(msSignModule);
        return false;
    }
    hr = SignerSignEx2(signerParams.dwFlags,
                       signerParams.pSubjectInfo,
                       signerParams.pSigningCert,
                       signerParams.pSignatureInfo,
                       signerParams.pProviderInfo,
                       signerParams.dwTimestampFlags,
                       signerParams.pszAlgorithmOid,
                       signerParams.pwszTimestampURL,
                       signerParams.pCryptAttrs,
                       signerParams.pSipData,
                       signerParams.pSignerContext,
                       signerParams.pCryptoPolicy,
                       signerParams.pReserved);

    FreeLibrary(msSignModule);

    RETURN_FALSE_IF_FAILED("Could not sign package.");

    if (sipClientData.pAppxSipState)
        sipClientData.pAppxSipState->Release();

    return true;
}
// ************ MSDN
#endif // MSC_VER >= 1900

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
    if (fileName.endsWith(QLatin1String(".exe"))) {
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

    if (fileName.endsWith(QLatin1String(".appx"))) {
        // For existing appx packages the manifest reader will be
        // instantiated later.
        return true;
    }

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

    HRESULT hr;
    hr = RoGetActivationFactory(HString::MakeReference(RuntimeClass_Windows_Foundation_Uri).Get(),
                                IID_PPV_ARGS(&d->uriFactory));
    CHECK_RESULT_FATAL("Failed to instantiate URI factory.", return);

    hr = CoCreateInstance(CLSID_AppxFactory, nullptr, CLSCTX_INPROC_SERVER,
                          IID_IAppxFactory, &d->packageFactory);
    CHECK_RESULT_FATAL("Failed to instantiate package factory.", return);

    bool existingPackage = runner->app().endsWith(QLatin1String(".appx"));

    if (existingPackage) {
        ComPtr<IStream> appxStream;
        hr = SHCreateStreamOnFile(wchar(runner->app()), STGM_READ, &appxStream);
        CHECK_RESULT_FATAL("Failed to open appx stream.", return);

        ComPtr<IAppxPackageReader> packageReader;
        hr = d->packageFactory->CreatePackageReader(appxStream.Get(), &packageReader);
        if (FAILED(hr)) {
            qCWarning(lcWinRtRunner).nospace() << "Failed to instantiate package reader. (0x"
                                               << QByteArray::number(hr, 16).constData()
                                               << ' ' << qt_error_string(hr) << ')';
            d->hasFatalError = true;
            return;
        }

        hr = packageReader->GetManifest(&d->manifestReader);
        if (FAILED(hr)) {
            qCWarning(lcWinRtRunner).nospace() << "Failed to query manifext reader from package";
            d->hasFatalError = true;
            return;
        }
    } else {
        if (!getManifestFile(runner->app(), &d->manifest)) {
            qCWarning(lcWinRtRunner) << "Unable to determine manifest file from" << runner->app();
            d->hasFatalError = true;
            return;
        }

        ComPtr<IStream> manifestStream;
        hr = SHCreateStreamOnFile(wchar(d->manifest), STGM_READ, &manifestStream);
        CHECK_RESULT_FATAL("Failed to open manifest stream.", return);

        hr = d->packageFactory->CreateManifestReader(manifestStream.Get(), &d->manifestReader);
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
    }
    ComPtr<IAppxManifestPackageId> packageId;
    hr = d->manifestReader->GetPackageId(&packageId);
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

#if _MSC_VER >= 1900
    LPWSTR publisher;
    packageId->GetPublisher(&publisher);
    CHECK_RESULT_FATAL("Failed to retrieve publisher name from package.", return);
    d->publisherName = QString::fromWCharArray(publisher);
    CoTaskMemFree(publisher);
#endif // _MSC_VER >= 1900

    ComPtr<IAppxManifestApplicationsEnumerator> applications;
    hr = d->manifestReader->GetApplications(&applications);
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
    d->executable = QFileInfo(runner->app()).absoluteDir()
            .absoluteFilePath(QString::fromWCharArray(executable));
    CoTaskMemFree(executable);

    ComPtr<IAppxManifestPackageDependenciesEnumerator> dependencies;
    hr = d->manifestReader->GetPackageDependencies(&dependencies);
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
        qCWarning(lcWinRtRunner).nospace().noquote()
                << QStringLiteral("The directory \"%1\" does not exist.").arg(
                       QDir::toNativeSeparators(extensionSdkDir));
        return false;
    }
    qCDebug(lcWinRtRunner).nospace().noquote()
        << "looking for dependency packages in \""
        << QDir::toNativeSeparators(extensionSdkDir) << '"';
    QDirIterator dit(extensionSdkDir, QStringList() << QStringLiteral("*.appx"),
                     QDir::Files,
                     QDirIterator::Subdirectories);
    while (dit.hasNext()) {
        dit.next();

        HRESULT hr;
        ComPtr<IStream> inputStream;
        forever {
            hr = SHCreateStreamOnFileEx(wchar(dit.filePath()),
                                        STGM_READ | STGM_SHARE_EXCLUSIVE,
                                        0, FALSE, NULL, &inputStream);
            if (HRESULT_CODE(hr) == ERROR_SHARING_VIOLATION) {
                qCWarning(lcWinRtRunner).nospace()
                    << "Input stream is locked by another process. Will retry...";
                Sleep(1000);
            } else {
                break;
            }
        }
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

        qCDebug(lcWinRtRunner).nospace().noquote()
            << "installing dependency \"" << name << "\" from \""
            << QDir::toNativeSeparators(dit.filePath()) << '"';
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
        // Add all files but filtered artifacts
        const QStringList excludeFileTypes = QStringList()
                << QStringLiteral("ilk") << QStringLiteral("pdb") << QStringLiteral("obj")
                << QStringLiteral("appx");

        QDirIterator dirIterator(base.absolutePath(), QDir::Files, QDirIterator::Subdirectories);
        while (dirIterator.hasNext()) {
            const QString filePath = dirIterator.next();
            if (filePath.endsWith(QLatin1String("AppxManifest.xml"), Qt::CaseInsensitive))
                continue;
            const QFileInfo fileInfo(filePath);
            if (!excludeFileTypes.contains(fileInfo.suffix()))
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

bool AppxEngine::sign(const QString &fileName)
{
#if _MSC_VER >= 1900
    Q_D(const AppxEngine);
    BYTE buffer[256];
    DWORD bufferSize = 256;

    if (!CertStrToName(X509_ASN_ENCODING, wchar(d->publisherName), CERT_X500_NAME_STR, 0, buffer, &bufferSize, 0)) {
        qCWarning(lcWinRtRunner) << "CertStrToName failed";
        return false;
    }
    CERT_NAME_BLOB certBlob;
    certBlob.cbData = bufferSize;
    certBlob.pbData = buffer;

    CRYPT_ALGORITHM_IDENTIFIER identifier;
    identifier.pszObjId = strdup(szOID_RSA_SHA256RSA);
    identifier.Parameters.cbData = 0;
    identifier.Parameters.pbData = NULL;

    CERT_EXTENSIONS extensions;
    extensions.cExtension = 2;
    extensions.rgExtension = new CERT_EXTENSION[2];

    // Basic Constraints
    CERT_BASIC_CONSTRAINTS2_INFO constraintsInfo;
    constraintsInfo.fCA = FALSE;
    constraintsInfo.fPathLenConstraint = FALSE;
    constraintsInfo.dwPathLenConstraint = 0;

    BYTE *constraintsEncoded = NULL;
    DWORD encodedSize = 0;
    CryptEncodeObject(X509_ASN_ENCODING, X509_BASIC_CONSTRAINTS2, &constraintsInfo,
                      constraintsEncoded, &encodedSize);
    constraintsEncoded = new BYTE[encodedSize];
    if (!CryptEncodeObject(X509_ASN_ENCODING, X509_BASIC_CONSTRAINTS2, &constraintsInfo,
                           constraintsEncoded, &encodedSize)) {
        qCWarning(lcWinRtRunner) << "Could not encode basic constraints.";
        delete [] constraintsEncoded;
        return false;
    }

    extensions.rgExtension[0].pszObjId = strdup(szOID_BASIC_CONSTRAINTS2);
    extensions.rgExtension[0].fCritical = TRUE;
    extensions.rgExtension[0].Value.cbData = encodedSize;
    extensions.rgExtension[0].Value.pbData = constraintsEncoded;

    // Code Signing
    char *codeSign = strdup(szOID_PKIX_KP_CODE_SIGNING);
    CERT_ENHKEY_USAGE enhancedUsage;
    enhancedUsage.cUsageIdentifier = 1;
    enhancedUsage.rgpszUsageIdentifier = &codeSign;

    BYTE *enhancedKeyEncoded = 0;
    encodedSize = 0;
    CryptEncodeObject(X509_ASN_ENCODING, X509_ENHANCED_KEY_USAGE, &enhancedUsage,
                      enhancedKeyEncoded, &encodedSize);
    enhancedKeyEncoded = new BYTE[encodedSize];
    if (!CryptEncodeObject(X509_ASN_ENCODING, X509_ENHANCED_KEY_USAGE, &enhancedUsage,
                           enhancedKeyEncoded, &encodedSize)) {
        qCWarning(lcWinRtRunner) << "Could not encode enhanced key usage.";
        delete [] constraintsEncoded;
        return false;
    }

    extensions.rgExtension[1].pszObjId = strdup(szOID_ENHANCED_KEY_USAGE);
    extensions.rgExtension[1].fCritical = TRUE;
    extensions.rgExtension[1].Value.cbData = encodedSize;
    extensions.rgExtension[1].Value.pbData = enhancedKeyEncoded;

    PCCERT_CONTEXT context = CertCreateSelfSignCertificate(NULL, &certBlob, NULL, NULL,
                                                           &identifier, NULL, NULL, &extensions);

    delete [] constraintsEncoded;

    if (!context) {
        qCWarning(lcWinRtRunner) << "Failed to create self sign certificate:" << GetLastError();
        return false;
    }

    return signAppxPackage(context, wchar(fileName));
#else // _MSC_VER < 1900
    Q_UNUSED(fileName);
    return true;
#endif // _MSC_VER < 1900
}

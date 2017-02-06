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

#ifndef APPXENGINE_P_H
#define APPXENGINE_P_H

#include <QtCore/qt_windows.h>
#include <QtCore/QSet>
#include <QtCore/QString>

#include <wrl.h>
#include <windows.system.h>

QT_USE_NAMESPACE

class Runner;
struct IAppxFactory;
class AppxEnginePrivate
{
public:
    AppxEnginePrivate()
    {
        HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
        if (FAILED(hr)) {
            qCWarning(lcWinRtRunner) << "Failed to initialize COM:" << qt_error_string(hr);
            hasFatalError = true;
        }
        hasFatalError = false;
    }

    virtual ~AppxEnginePrivate()
    {
        packageFactory.Reset();
        CoUninitialize();
    }

    Runner *runner;
    bool hasFatalError;

    QString manifest;
    QString packageFullName;
    QString packageFamilyName;
    QString publisherName;
    ABI::Windows::System::ProcessorArchitecture packageArchitecture;
    QString executable;
    qint64 pid;
    HANDLE processHandle;
    DWORD exitCode;
    QSet<QString> dependencies;
    QSet<QString> installedPackages;

    Microsoft::WRL::ComPtr<ABI::Windows::Foundation::IUriRuntimeClassFactory> uriFactory;
    Microsoft::WRL::ComPtr<IAppxFactory> packageFactory;
    Microsoft::WRL::ComPtr<IAppxManifestReader> manifestReader;
};

#define wchar(str) reinterpret_cast<LPCWSTR>(str.utf16())
#define hStringFromQString(str) HStringReference(reinterpret_cast<const wchar_t *>(str.utf16())).Get()
#define QStringFromHString(hstr) QString::fromWCharArray(WindowsGetStringRawBuffer(hstr, Q_NULLPTR))

#define RETURN_IF_FAILED(msg, ret) \
    if (FAILED(hr)) { \
        qCWarning(lcWinRtRunner).nospace() << msg << ": 0x" << QByteArray::number(hr, 16).constData() \
                                           << ' ' << qt_error_string(hr); \
        ret; \
    }

#define RETURN_HR_IF_FAILED(msg) RETURN_IF_FAILED(msg, return hr)
#define RETURN_OK_IF_FAILED(msg) RETURN_IF_FAILED(msg, return S_OK)
#define RETURN_FALSE_IF_FAILED(msg) RETURN_IF_FAILED(msg, return false)
#define RETURN_VOID_IF_FAILED(msg) RETURN_IF_FAILED(msg, return)

#endif // APPXENGINE_P_H

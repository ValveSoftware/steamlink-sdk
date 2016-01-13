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
    ABI::Windows::System::ProcessorArchitecture packageArchitecture;
    QString executable;
    qint64 pid;
    HANDLE processHandle;
    DWORD exitCode;
    QSet<QString> dependencies;
    QSet<QString> installedPackages;

    Microsoft::WRL::ComPtr<ABI::Windows::Foundation::IUriRuntimeClassFactory> uriFactory;
    Microsoft::WRL::ComPtr<IAppxFactory> packageFactory;
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

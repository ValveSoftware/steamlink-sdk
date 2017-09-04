/****************************************************************************
 **
 ** Copyright (C) 2016 Ivan Vizir <define-true-false@yandex.com>
 ** Contact: https://www.qt.io/licensing/
 **
 ** This file is part of the QtWinExtras module of the Qt Toolkit.
 **
 ** $QT_BEGIN_LICENSE:LGPL$
 ** Commercial License Usage
 ** Licensees holding valid commercial Qt licenses may use this file in
 ** accordance with the commercial license agreement provided with the
 ** Software or, alternatively, in accordance with the terms contained in
 ** a written agreement between you and The Qt Company. For licensing terms
 ** and conditions see https://www.qt.io/terms-conditions. For further
 ** information use the contact form at https://www.qt.io/contact-us.
 **
 ** GNU Lesser General Public License Usage
 ** Alternatively, this file may be used under the terms of the GNU Lesser
 ** General Public License version 3 as published by the Free Software
 ** Foundation and appearing in the file LICENSE.LGPL3 included in the
 ** packaging of this file. Please review the following information to
 ** ensure the GNU Lesser General Public License version 3 requirements
 ** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
 **
 ** GNU General Public License Usage
 ** Alternatively, this file may be used under the terms of the GNU
 ** General Public License version 2.0 or (at your option) the GNU General
 ** Public license version 3 or any later version approved by the KDE Free
 ** Qt Foundation. The licenses are as published by the Free Software
 ** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
 ** included in the packaging of this file. Please review the following
 ** information to ensure the GNU General Public License requirements will
 ** be met: https://www.gnu.org/licenses/gpl-2.0.html and
 ** https://www.gnu.org/licenses/gpl-3.0.html.
 **
 ** $QT_END_LICENSE$
 **
 ****************************************************************************/

#include "qwinfunctions_p.h"

#include <qt_windows.h>

QT_BEGIN_NAMESPACE

// in order to allow binary to load on WinXP...
QtDwmApiDll qtDwmApiDll;
QtShell32Dll qtShell32Dll;

void QtDwmApiDll::resolve()
{
    if (QSysInfo::windowsVersion() >= QSysInfo::WV_WINDOWS7) {
        if (const HMODULE dwmapi = LoadLibraryW(L"dwmapi.dll")) {
            dwmSetIconicThumbnail =
                (DwmSetIconicThumbnail) GetProcAddress(dwmapi, "DwmSetIconicThumbnail");
            dwmSetIconicLivePreviewBitmap =
                (DwmSetIconicLivePreviewBitmap) GetProcAddress(dwmapi, "DwmSetIconicLivePreviewBitmap");
            dwmInvalidateIconicBitmaps =
                (DwmInvalidateIconicBitmaps) GetProcAddress(dwmapi, "DwmInvalidateIconicBitmaps");
        }
    }
}

void QtShell32Dll::resolve()
{
    if (QSysInfo::windowsVersion() >= QSysInfo::WV_WINDOWS7) {
        if (const HMODULE shell32 = LoadLibraryW(L"shell32.dll")) {
            setCurrentProcessExplicitAppUserModelID =
                    (SetCurrentProcessExplicitAppUserModelID) GetProcAddress(shell32, "SetCurrentProcessExplicitAppUserModelID");
        }
    }
}

QT_END_NAMESPACE

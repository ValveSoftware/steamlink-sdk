/****************************************************************************
 **
 ** Copyright (C) 2013 Ivan Vizir <define-true-false@yandex.com>
 ** Contact: http://www.qt-project.org/legal
 **
 ** This file is part of the QtWinExtras module of the Qt Toolkit.
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

#include "qwinfunctions_p.h"

#include <qt_windows.h>

QT_BEGIN_NAMESPACE

// in order to allow binary to load on WinXP...
QtDwmApiDll qtDwmApiDll;
QtShell32Dll qtShell32Dll;

void QtDwmApiDll::resolve()
{
    if (const HMODULE dwmapi = LoadLibraryW(L"dwmapi.dll")) {
        dwmExtendFrameIntoClientArea =
            (DwmExtendFrameIntoClientArea) GetProcAddress(dwmapi, "DwmExtendFrameIntoClientArea");
        dwmEnableBlurBehindWindow =
            (DwmEnableBlurBehindWindow) GetProcAddress(dwmapi, "DwmEnableBlurBehindWindow");
        dwmGetColorizationColor =
            (DwmGetColorizationColor) GetProcAddress(dwmapi, "DwmGetColorizationColor");
        dwmSetWindowAttribute =
            (DwmSetWindowAttribute) GetProcAddress(dwmapi, "DwmSetWindowAttribute");
        dwmGetWindowAttribute =
            (DwmGetWindowAttribute) GetProcAddress(dwmapi, "DwmGetWindowAttribute");
        dwmIsCompositionEnabled =
            (DwmIsCompositionEnabled) GetProcAddress(dwmapi, "DwmIsCompositionEnabled");
        dwmEnableComposition =
            (DwmEnableComposition) GetProcAddress(dwmapi, "DwmEnableComposition");
        if (QSysInfo::windowsVersion() >= QSysInfo::WV_WINDOWS7) {
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
    if (const HMODULE shell32 = LoadLibraryW(L"shell32.dll")) {
        sHCreateItemFromParsingName =
            (SHCreateItemFromParsingName) GetProcAddress(shell32, "SHCreateItemFromParsingName");
        setCurrentProcessExplicitAppUserModelID =
            (SetCurrentProcessExplicitAppUserModelID) GetProcAddress(shell32, "SetCurrentProcessExplicitAppUserModelID");
    }
}

QT_END_NAMESPACE

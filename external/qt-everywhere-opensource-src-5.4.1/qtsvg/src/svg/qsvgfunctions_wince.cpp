/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt SVG module of the Qt Toolkit.
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
#ifdef _WIN32_WCE //Q_OS_WINCE

#include <windows.h>
#include <winbase.h>
#include <kfuncs.h>
#include <stdio.h>
#include <altcecrt.h>

#include "qplatformdefs.h"
#include "qsvgfunctions_wince_p.h"
#include "qstring.h"
#include "qbytearray.h"
#include "qhash.h"

QT_USE_NAMESPACE

#ifdef __cplusplus
extern "C" {
#endif

// File I/O ---------------------------------------------------------
int errno = 0;

int qt_wince_open(const char *filename, int oflag, int pmode)
{
    QString fn(QString::fromLatin1(filename));
    return _wopen((wchar_t*)fn.utf16(), oflag, pmode);
}

int qt_wince__wopen(const wchar_t *filename, int oflag, int /*pmode*/)
{
    wchar_t *flag;

    if (oflag & _O_APPEND) {
        if (oflag & _O_WRONLY) {
            flag = L"a";
        } else if (oflag & _O_RDWR) {
            flag = L"a+";
        }
    } else if (oflag & _O_BINARY) {
        if (oflag & _O_WRONLY) {
            flag = L"wb";
        } else if (oflag & _O_RDWR) {
            flag = L"w+b"; // slightly different from "r+" where the file must exist
        } else if (oflag & _O_RDONLY) {
            flag = L"rb";
        } else {
            flag = L"b";
        }
    } else {
        if (oflag & _O_WRONLY) {
            flag = L"wt";
        } else if (oflag & _O_RDWR) {
            flag = L"w+t"; // slightly different from "r+" where the file must exist
        } else if (oflag & _O_RDONLY) {
            flag = L"rt";
        } else {
            flag = L"t";
        }
    }

    int retval = (int)_wfopen(filename, flag);
    return (retval == NULL) ? -1 : retval;
}

long qt_wince__lseek(int handle, long offset, int origin)
{
    return fseek((FILE*)handle, offset, origin);
}

int qt_wince__read(int handle, void *buffer, unsigned int count)
{
    return fread(buffer, 1, count, (FILE*)handle);
}

int qt_wince__write(int handle, const void *buffer, unsigned int count)
{
    return fwrite(buffer, 1, count, (FILE*)handle);
}

int qt_wince__close(int handle)
{
    if (!handle)
        return 0;
    return fclose((FILE*)handle);
}

#ifdef __cplusplus
} // extern "C"
#endif

#endif // Q_OS_WINCE

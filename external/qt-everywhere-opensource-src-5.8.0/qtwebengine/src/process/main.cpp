/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
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

#include "process_main.h"

#include <QCoreApplication>
#include <stdio.h>

#if defined(Q_OS_LINUX)

struct tm;
struct stat;
struct stat64;

// exported in zygote_main_linux.cc
namespace content {
struct tm* localtime_override(const time_t* timep);
struct tm* localtime64_override(const time_t* timep);
struct tm* localtime_r_override(const time_t* timep, struct tm* result);
struct tm* localtime64_r_override(const time_t* timep, struct tm* result);
}

// from zygote_main_linux.cc
__attribute__ ((__visibility__("default")))
struct tm* localtime_proxy(const time_t* timep) __asm__ ("localtime");
struct tm* localtime_proxy(const time_t* timep)
{
    return content::localtime_override(timep);
}

__attribute__ ((__visibility__("default")))
struct tm* localtime64_proxy(const time_t* timep) __asm__ ("localtime64");
struct tm* localtime64_proxy(const time_t* timep)
{
    return content::localtime64_override(timep);
}

__attribute__ ((__visibility__("default")))
struct tm* localtime_r_proxy(const time_t* timep, struct tm* result) __asm__ ("localtime_r");
struct tm* localtime_r_proxy(const time_t* timep, struct tm* result)
{
    return content::localtime_r_override(timep, result);
}

__attribute__ ((__visibility__("default")))
struct tm* localtime64_r_proxy(const time_t* timep, struct tm* result) __asm__ ("localtime64_r");
struct tm* localtime64_r_proxy(const time_t* timep, struct tm* result)
{
    return content::localtime64_r_override(timep, result);
}

#endif // defined(OS_LINUX)

#ifdef Q_OS_WIN
void initDpiAwareness();
#endif // defined(Q_OS_WIN)

int main(int argc, const char **argv)
{
#ifdef Q_OS_WIN
    initDpiAwareness();
#endif

    // QCoreApplication needs a non-const pointer, while the
    // ContentMain in Chromium needs the pointer to be const.
    QCoreApplication qtApplication(argc, const_cast<char**>(argv));

    return QtWebEngine::processMain(argc, argv);
}


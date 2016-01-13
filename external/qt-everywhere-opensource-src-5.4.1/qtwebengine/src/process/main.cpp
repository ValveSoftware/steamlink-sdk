/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "process_main.h"

#include <QCoreApplication>
#include <stdio.h>

#if defined(OS_LINUX)
#if defined(__GLIBC__) && !defined(__UCLIBC__) && !defined(OS_ANDROID) && !defined(HAVE_XSTAT)
#define HAVE_XSTAT 1
#endif

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

// exported in libc_urandom_proxy.cc
namespace sandbox {
FILE* fopen_override(const char* path, const char* mode);
FILE* fopen64_override(const char* path, const char* mode);
#if HAVE_XSTAT
int xstat_override(int version, const char *path, struct stat *buf);
int xstat64_override(int version, const char *path, struct stat64 *buf);
#else
int stat_override(const char *path, struct stat *buf);
int stat64_override(const char *path, struct stat64 *buf);
#endif
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

// from libc_urandom_proxy.cc
__attribute__ ((__visibility__("default")))
FILE* fopen_proxy(const char* path, const char* mode)  __asm__ ("fopen");
FILE* fopen_proxy(const char* path, const char* mode)
{
    return sandbox::fopen_override(path, mode);
}

__attribute__ ((__visibility__("default")))
FILE* fopen64_proxy(const char* path, const char* mode)  __asm__ ("fopen64");
FILE* fopen64_proxy(const char* path, const char* mode)
{
    return sandbox::fopen64_override(path, mode);
}

#if HAVE_XSTAT
__attribute__ ((__visibility__("default")))
int xstat_proxy(int version, const char *path, struct stat *buf)  __asm__ ("__xstat");
int xstat_proxy(int version, const char *path, struct stat *buf)
{
    return sandbox::xstat_override(version, path, buf);
}

__attribute__ ((__visibility__("default")))
int xstat64_proxy(int version, const char *path, struct stat64 *buf)  __asm__ ("__xstat64");
int xstat64_proxy(int version, const char *path, struct stat64 *buf)
{
    return sandbox::xstat64_override(version, path, buf);
}

#else
__attribute__ ((__visibility__("default")))
int stat_proxy(const char *path, struct stat *buf)  __asm__ ("stat");
int stat_proxy(const char *path, struct stat *buf)
{
    return sandbox::stat_override(path, buf);
}

__attribute__ ((__visibility__("default")))
int stat64_proxy(const char *path, struct stat64 *buf)  __asm__ ("stat64");
int stat64_proxy(const char *path, struct stat64 *buf)
{
    return sandbox::stat64_override(path, buf);
}

#endif
#endif // defined(OS_LINUX)

int main(int argc, const char **argv)
{
    // QCoreApplication needs a non-const pointer, while the
    // ContentMain in Chromium needs the pointer to be const.
    QCoreApplication qtApplication(argc, const_cast<char**>(argv));

    return QtWebEngine::processMain(argc, argv);
}


/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#include "qqmlmemoryprofiler_p.h"

QT_BEGIN_NAMESPACE


QQmlMemoryScope::LibraryState QQmlMemoryScope::state = QQmlMemoryScope::Unloaded;

typedef void (qmlmemprofile_stats)(int *allocCount, int *bytesAllocated);
typedef void (qmlmemprofile_clear)();
typedef void (qmlmemprofile_enable)();
typedef void (qmlmemprofile_disable)();
typedef void (qmlmemprofile_push_location)(const char *filename, int lineNumber);
typedef void (qmlmemprofile_pop_location)();
typedef void (qmlmemprofile_save)(const char *filename);
typedef int (qmlmemprofile_is_enabled)();

static qmlmemprofile_stats *memprofile_stats;
static qmlmemprofile_clear *memprofile_clear;
static qmlmemprofile_enable *memprofile_enable;
static qmlmemprofile_disable *memprofile_disable;
static qmlmemprofile_push_location *memprofile_push_location;
static qmlmemprofile_pop_location *memprofile_pop_location;
static qmlmemprofile_save *memprofile_save;
static qmlmemprofile_is_enabled *memprofile_is_enabled;

#ifndef QT_NO_LIBRARY
extern QFunctionPointer qt_linux_find_symbol_sys(const char *symbol);
#endif

bool QQmlMemoryScope::doOpenLibrary()
{
#if defined(Q_OS_LINUX) && !defined(QT_NO_LIBRARY)
    if (state == Unloaded) {
        memprofile_stats = (qmlmemprofile_stats *) qt_linux_find_symbol_sys("qmlmemprofile_stats");
        memprofile_clear = (qmlmemprofile_clear *) qt_linux_find_symbol_sys("qmlmemprofile_clear");
        memprofile_enable = (qmlmemprofile_enable *) qt_linux_find_symbol_sys("qmlmemprofile_enable");
        memprofile_disable = (qmlmemprofile_disable *) qt_linux_find_symbol_sys("qmlmemprofile_disable");
        memprofile_push_location = (qmlmemprofile_push_location *) qt_linux_find_symbol_sys("qmlmemprofile_push_location");
        memprofile_pop_location = (qmlmemprofile_pop_location *) qt_linux_find_symbol_sys("qmlmemprofile_pop_location");
        memprofile_save = (qmlmemprofile_save *) qt_linux_find_symbol_sys("qmlmemprofile_save");
        memprofile_is_enabled = (qmlmemprofile_is_enabled *) qt_linux_find_symbol_sys("qmlmemprofile_is_enabled");

        if (memprofile_stats && memprofile_clear && memprofile_enable && memprofile_disable &&
            memprofile_push_location && memprofile_pop_location && memprofile_save && memprofile_is_enabled)
            state = Loaded;
        else
            state = Failed;
    }
#endif  // Q_OS_LINUX

    return state == Loaded;
}

void QQmlMemoryScope::init(const char *string)
{
    if (memprofile_is_enabled()) {
        memprofile_push_location(string, 0);
        pushed = true;
    }
}

void QQmlMemoryScope::done()
{
    memprofile_pop_location();
}

bool QQmlMemoryProfiler::isEnabled()
{
    if (QQmlMemoryScope::openLibrary())
        return memprofile_is_enabled();

    return false;
}

void QQmlMemoryProfiler::enable()
{
    if (QQmlMemoryScope::openLibrary())
        memprofile_enable();
}

void QQmlMemoryProfiler::disable()
{
    if (QQmlMemoryScope::openLibrary())
        memprofile_disable();
}

void QQmlMemoryProfiler::clear()
{
    if (QQmlMemoryScope::openLibrary())
        memprofile_clear();
}

void QQmlMemoryProfiler::stats(int *allocCount, int *bytesAllocated)
{
    if (QQmlMemoryScope::openLibrary())
        memprofile_stats(allocCount, bytesAllocated);
}

void QQmlMemoryProfiler::save(const char *filename)
{
    if (QQmlMemoryScope::openLibrary())
        memprofile_save(filename);
}

QT_END_NAMESPACE

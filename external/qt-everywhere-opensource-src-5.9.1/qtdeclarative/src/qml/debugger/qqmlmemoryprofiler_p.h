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

#ifndef QQMLMEMORYPROFILER_H
#define QQMLMEMORYPROFILER_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <private/qtqmlglobal_p.h>
#include <QUrl>

QT_BEGIN_NAMESPACE

#ifdef QT_NO_QML_DEBUGGER

#define QML_MEMORY_SCOPE_URL(url)
#define QML_MEMORY_SCOPE_STRING(s)

#else

class Q_QML_PRIVATE_EXPORT QQmlMemoryScope
{
public:
    explicit QQmlMemoryScope(const QUrl &url)
        : pushed(false)
    {
        if (Q_UNLIKELY(openLibrary()))
            init(url.path().toUtf8().constData());
    }

    explicit QQmlMemoryScope(const char *string)
        : pushed(false)
    {
        if (Q_UNLIKELY(openLibrary()))
            init(string);
    }

    ~QQmlMemoryScope()
    {
        if (Q_UNLIKELY(pushed))
            done();
    }

    enum LibraryState
    {
        Unloaded,
        Failed,
        Loaded
    };

    static bool openLibrary()
    {
        if (Q_LIKELY(state == Loaded))
            return true;
        if (state == Failed)
            return false;

        return doOpenLibrary();
    }

private:
    Q_NEVER_INLINE void init(const char *string);
    Q_NEVER_INLINE void done();
    Q_NEVER_INLINE static bool doOpenLibrary();

    static LibraryState state;

    bool pushed;
};

class Q_QML_PRIVATE_EXPORT QQmlMemoryProfiler
{
public:
    static void enable();
    static void disable();
    static bool isEnabled();

    static void clear();
    static void stats(int *allocCount, int *bytesAllocated);
    static void save(const char *filename);
};

#define QML_MEMORY_SCOPE_URL(url)       QQmlMemoryScope _qml_memory_scope(url)
#define QML_MEMORY_SCOPE_STRING(s)      QQmlMemoryScope _qml_memory_scope(s)

#endif

QT_END_NAMESPACE
#endif // QQMLMEMORYPROFILER_H

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
#ifndef QV4REGEXP_H
#define QV4REGEXP_H

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

#include <QString>
#include <QVector>

#include <wtf/RefPtr.h>
#include <wtf/FastAllocBase.h>
#include <wtf/BumpPointerAllocator.h>

#include <limits.h>

#include <yarr/Yarr.h>
#include <yarr/YarrInterpreter.h>
#include <yarr/YarrJIT.h>

#include "qv4managed_p.h"
#include "qv4engine_p.h"

QT_BEGIN_NAMESPACE

namespace QV4 {

struct ExecutionEngine;
struct RegExpCacheKey;

namespace Heap {

struct RegExp : Base {
    void init(ExecutionEngine* engine, const QString& pattern, bool ignoreCase, bool multiline);
    void destroy();

    QString *pattern;
    JSC::Yarr::BytecodePattern *byteCode;
#if ENABLE(YARR_JIT)
    JSC::Yarr::YarrCodeBlock *jitCode;
#endif
    RegExpCache *cache;
    int subPatternCount;
    bool ignoreCase;
    bool multiLine;

    int captureCount() const { return subPatternCount + 1; }
};
V4_ASSERT_IS_TRIVIAL(RegExp)

}

struct RegExp : public Managed
{
    V4_MANAGED(RegExp, Managed)
    Q_MANAGED_TYPE(RegExp)
    V4_NEEDS_DESTROY

    QString pattern() const { return *d()->pattern; }
    JSC::Yarr::BytecodePattern *byteCode() { return d()->byteCode; }
#if ENABLE(YARR_JIT)
    JSC::Yarr::YarrCodeBlock *jitCode() const { return d()->jitCode; }
#endif
    RegExpCache *cache() const { return d()->cache; }
    int subPatternCount() const { return d()->subPatternCount; }
    bool ignoreCase() const { return d()->ignoreCase; }
    bool multiLine() const { return d()->multiLine; }

    static Heap::RegExp *create(ExecutionEngine* engine, const QString& pattern, bool ignoreCase = false, bool multiline = false);

    bool isValid() const { return d()->byteCode; }

    uint match(const QString& string, int start, uint *matchOffsets);

    int captureCount() const { return subPatternCount() + 1; }

    static void markObjects(Heap::Base *that, QV4::ExecutionEngine *e);

    friend class RegExpCache;
};

struct RegExpCacheKey
{
    RegExpCacheKey(const QString &pattern, bool ignoreCase, bool multiLine)
        : pattern(pattern)
        , ignoreCase(ignoreCase)
        , multiLine(multiLine)
    { }
    explicit inline RegExpCacheKey(const RegExp::Data *re);

    bool operator==(const RegExpCacheKey &other) const
    { return pattern == other.pattern && ignoreCase == other.ignoreCase && multiLine == other.multiLine; }
    bool operator!=(const RegExpCacheKey &other) const
    { return !operator==(other); }

    QString pattern;
    uint ignoreCase : 1;
    uint multiLine : 1;
};

inline RegExpCacheKey::RegExpCacheKey(const RegExp::Data *re)
    : pattern(*re->pattern)
    , ignoreCase(re->ignoreCase)
    , multiLine(re->multiLine)
{}

inline uint qHash(const RegExpCacheKey& key, uint seed = 0) Q_DECL_NOTHROW
{ return qHash(key.pattern, seed); }

class RegExpCache : public QHash<RegExpCacheKey, WeakValue>
{
public:
    ~RegExpCache();
};



}

QT_END_NAMESPACE

#endif // QV4REGEXP_H

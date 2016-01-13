/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#include "qv4regexp_p.h"
#include "qv4engine_p.h"
#include "qv4scopedvalue_p.h"
#include "qv4mm_p.h"

using namespace QV4;

RegExpCache::~RegExpCache()
{
    for (RegExpCache::Iterator it = begin(), e = end();
         it != e; ++it)
        it.value()->d()->cache = 0;
    clear();
}

DEFINE_MANAGED_VTABLE(RegExp);

uint RegExp::match(const QString &string, int start, uint *matchOffsets)
{
    if (!isValid())
        return JSC::Yarr::offsetNoMatch;

    WTF::String s(string);

#if ENABLE(YARR_JIT)
    if (!jitCode().isFallBack() && jitCode().has16BitCode())
        return jitCode().execute(s.characters16(), start, s.length(), (int*)matchOffsets).start;
#endif

    return JSC::Yarr::interpret(byteCode().get(), s.characters16(), string.length(), start, matchOffsets);
}

RegExp* RegExp::create(ExecutionEngine* engine, const QString& pattern, bool ignoreCase, bool multiline)
{
    RegExpCacheKey key(pattern, ignoreCase, multiline);

    RegExpCache *cache = engine->regExpCache;
    if (cache) {
        if (RegExp *result = cache->value(key))
            return result;
    }

    RegExp *result = engine->memoryManager->alloc<RegExp>(engine, pattern, ignoreCase, multiline);

    if (!cache)
        cache = engine->regExpCache = new RegExpCache;

    result->d()->cache = cache;
    cache->insert(key, result);

    return result;
}

RegExp::Data::Data(ExecutionEngine* engine, const QString &pattern, bool ignoreCase, bool multiline)
    : Managed::Data(engine->regExpValueClass)
    , pattern(pattern)
    , ignoreCase(ignoreCase)
    , multiLine(multiline)
{
    setVTable(staticVTable());
    const char* error = 0;
    JSC::Yarr::YarrPattern yarrPattern(WTF::String(pattern), ignoreCase, multiline, &error);
    if (error)
        return;
    subPatternCount = yarrPattern.m_numSubpatterns;
    byteCode = JSC::Yarr::byteCompile(yarrPattern, engine->bumperPointerAllocator);
#if ENABLE(YARR_JIT)
    if (!yarrPattern.m_containsBackreferences && engine->iselFactory->jitCompileRegexps()) {
        JSC::JSGlobalData dummy(engine->regExpAllocator);
        JSC::Yarr::jitCompile(yarrPattern, JSC::Yarr::Char16, &dummy, jitCode);
    }
#endif
}

RegExp::Data::~Data()
{
    if (cache) {
        RegExpCacheKey key(this);
        cache->remove(key);
    }
}

void RegExp::destroy(Managed *that)
{
    static_cast<RegExp*>(that)->d()->~Data();
}

void RegExp::markObjects(Managed *that, ExecutionEngine *e)
{
    Q_UNUSED(that);
    Q_UNUSED(e);
}

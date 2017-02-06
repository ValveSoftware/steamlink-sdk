/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2012 Intel Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PerformanceEntry_h
#define PerformanceEntry_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ScriptState;
class ScriptValue;
class V8ObjectBuilder;

using PerformanceEntryType = unsigned char;
using PerformanceEntryTypeMask = unsigned char;

class CORE_EXPORT PerformanceEntry : public GarbageCollectedFinalized<PerformanceEntry>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    virtual ~PerformanceEntry();

    enum EntryType {
        Invalid = 0,
        Composite = 1 << 1,
        Mark = 1 << 2,
        Measure = 1 << 3,
        Render = 1 << 4,
        Resource = 1 << 5,
    };

    String name() const;
    String entryType() const;
    double startTime() const;
    double duration() const;

    ScriptValue toJSONForBinding(ScriptState*) const;

    PerformanceEntryType entryTypeEnum() const { return m_entryTypeEnum; }

    bool isResource() const { return m_entryTypeEnum == Resource; }
    bool isRender()  const { return m_entryTypeEnum == Render; }
    bool isComposite()  const { return m_entryTypeEnum == Composite; }
    bool isMark()  const { return m_entryTypeEnum == Mark; }
    bool isMeasure()  const { return m_entryTypeEnum == Measure; }

    static bool startTimeCompareLessThan(PerformanceEntry* a, PerformanceEntry* b)
    {
        return a->startTime() < b->startTime();
    }

    static EntryType toEntryTypeEnum(const String& entryType);

    DEFINE_INLINE_VIRTUAL_TRACE() { }

protected:
    PerformanceEntry(const String& name, const String& entryType, double startTime, double finishTime);
    virtual void buildJSONValue(V8ObjectBuilder&) const;

private:
    const String m_name;
    const String m_entryType;
    const double m_startTime;
    const double m_duration;
    const PerformanceEntryType m_entryTypeEnum;
};

} // namespace blink

#endif // PerformanceEntry_h

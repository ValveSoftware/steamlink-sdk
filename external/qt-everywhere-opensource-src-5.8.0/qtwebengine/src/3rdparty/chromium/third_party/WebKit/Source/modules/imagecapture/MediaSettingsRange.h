// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaSettingsRange_h
#define MediaSettingsRange_h

#include "bindings/core/v8/ScriptWrappable.h"

namespace blink {

class MediaSettingsRange final
    : public GarbageCollected<MediaSettingsRange>
    , public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    static MediaSettingsRange* create(unsigned long max, unsigned long min, unsigned long current)
    {
        return new MediaSettingsRange(max, min, current);
    }

    unsigned long max() const { return m_max; }
    unsigned long min() const { return m_min; }
    unsigned long current() const { return m_current; }

    DEFINE_INLINE_TRACE() {}

private:
    MediaSettingsRange(unsigned long max, unsigned long min, unsigned long current)
        : m_max(max)
        , m_min(min)
        , m_current(current) { }

    unsigned long m_max;
    unsigned long m_min;
    unsigned long m_current;
};

} // namespace blink

#endif // MediaSettingsRange_h

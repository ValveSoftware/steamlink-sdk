// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NoEventDispatchAssertion_h
#define NoEventDispatchAssertion_h

#include "wtf/MainThread.h"
#include "wtf/TemporaryChange.h"

namespace WebCore {

#ifndef NDEBUG

class NoEventDispatchAssertion {
public:
    NoEventDispatchAssertion()
    {
        if (!isMainThread())
            return;
        ++s_count;
    }

    ~NoEventDispatchAssertion()
    {
        if (!isMainThread())
            return;
        ASSERT(s_count);
        --s_count;
    }

    static bool isEventDispatchForbidden()
    {
        if (!isMainThread())
            return false;
        return s_count;
    }

    // It's safe to dispatch events in SVGImage since there can't be any script
    // listeners.
    class AllowSVGImageEvents {
    public:
        AllowSVGImageEvents()
            : m_change(s_count, 0)
        {
        }

        ~AllowSVGImageEvents()
        {
            ASSERT(!s_count);
        }

        TemporaryChange<unsigned> m_change;
    };

private:
    static unsigned s_count;
};

#else

class NoEventDispatchAssertion {
public:
    NoEventDispatchAssertion() { }

    class AllowSVGImageEvents {
    public:
        AllowSVGImageEvents() { }
    };
};

#endif // NDEBUG

} // namespace WebCore

#endif // NoEventDispatchAssertion_h

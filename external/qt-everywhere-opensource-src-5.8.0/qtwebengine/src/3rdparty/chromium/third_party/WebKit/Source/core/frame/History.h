/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef History_h
#define History_h

#include "base/gtest_prod_util.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "bindings/core/v8/SerializedScriptValue.h"
#include "core/loader/FrameLoaderTypes.h"
#include "core/frame/DOMWindowProperty.h"
#include "platform/heap/Handle.h"
#include "wtf/Forward.h"

namespace blink {

class LocalFrame;
class KURL;
class ExecutionContext;
class ExceptionState;
class SecurityOrigin;

class CORE_EXPORT History final : public GarbageCollectedFinalized<History>, public ScriptWrappable, public DOMWindowProperty {
    DEFINE_WRAPPERTYPEINFO();
    USING_GARBAGE_COLLECTED_MIXIN(History);
public:
    static History* create(LocalFrame* frame)
    {
        return new History(frame);
    }

    unsigned length() const;
    SerializedScriptValue* state();

    void back(ExecutionContext*);
    void forward(ExecutionContext*);
    void go(ExecutionContext*, int delta);

    void pushState(PassRefPtr<SerializedScriptValue> data, const String& title, const String& url, ExceptionState& exceptionState)
    {
        stateObjectAdded(data, title, url, scrollRestorationInternal(), FrameLoadTypeStandard, exceptionState);
    }

    void replaceState(PassRefPtr<SerializedScriptValue> data, const String& title, const String& url, ExceptionState& exceptionState)
    {
        stateObjectAdded(data, title, url, scrollRestorationInternal(), FrameLoadTypeReplaceCurrentItem, exceptionState);
    }

    void setScrollRestoration(const String& value);
    String scrollRestoration();

    bool stateChanged() const;
    bool isSameAsCurrentState(SerializedScriptValue*) const;


    DECLARE_VIRTUAL_TRACE();

private:
    FRIEND_TEST_ALL_PREFIXES(HistoryTest, CanChangeToURL);
    FRIEND_TEST_ALL_PREFIXES(HistoryTest, CanChangeToURLInFileOrigin);
    FRIEND_TEST_ALL_PREFIXES(HistoryTest, CanChangeToURLInUniqueOrigin);

    explicit History(LocalFrame*);

    static bool canChangeToUrl(const KURL&, SecurityOrigin*, const KURL& documentURL);

    KURL urlForState(const String& url);

    void stateObjectAdded(PassRefPtr<SerializedScriptValue>, const String& title, const String& url, HistoryScrollRestorationType, FrameLoadType, ExceptionState&);
    SerializedScriptValue* stateInternal() const;
    HistoryScrollRestorationType scrollRestorationInternal() const;

    RefPtr<SerializedScriptValue> m_lastStateObjectRequested;
};

} // namespace blink

#endif // History_h

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RejectedPromises_h
#define RejectedPromises_h

#include "bindings/core/v8/SourceLocation.h"
#include "core/fetch/AccessControlStatus.h"
#include "wtf/Deque.h"
#include "wtf/Forward.h"
#include "wtf/RefCounted.h"
#include "wtf/Vector.h"
#include <memory>

namespace v8 {
class PromiseRejectMessage;
};

namespace blink {

class ScriptState;

class RejectedPromises final : public RefCounted<RejectedPromises> {
    USING_FAST_MALLOC(RejectedPromises);
public:
    static PassRefPtr<RejectedPromises> create()
    {
        return adoptRef(new RejectedPromises());
    }

    ~RejectedPromises();
    void dispose();

    void rejectedWithNoHandler(ScriptState*, v8::PromiseRejectMessage, const String& errorMessage, std::unique_ptr<SourceLocation>, AccessControlStatus);
    void handlerAdded(v8::PromiseRejectMessage);

    void processQueue();

private:
    class Message;

    RejectedPromises();

    using MessageQueue = Deque<std::unique_ptr<Message>>;
    std::unique_ptr<MessageQueue> createMessageQueue();

    void processQueueNow(std::unique_ptr<MessageQueue>);
    void revokeNow(std::unique_ptr<Message>);

    MessageQueue m_queue;
    Vector<std::unique_ptr<Message>> m_reportedAsErrors;
};

} // namespace blink

#endif // RejectedPromises_h

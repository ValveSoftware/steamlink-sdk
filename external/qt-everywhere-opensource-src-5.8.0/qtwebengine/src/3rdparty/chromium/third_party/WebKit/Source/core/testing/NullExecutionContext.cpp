// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/testing/NullExecutionContext.h"

#include "core/dom/ExecutionContextTask.h"
#include "core/events/Event.h"
#include "core/frame/DOMTimer.h"

namespace blink {

namespace {

class NullEventQueue final : public EventQueue {
public:
    NullEventQueue() { }
    ~NullEventQueue() override { }
    bool enqueueEvent(Event*) override { return true; }
    bool cancelEvent(Event*) override { return true; }
    void close() override { }
};

} // namespace

NullExecutionContext::NullExecutionContext()
    : m_tasksNeedSuspension(false)
    , m_isSecureContext(true)
    , m_queue(new NullEventQueue())
{
}

void NullExecutionContext::postTask(const WebTraceLocation&, std::unique_ptr<ExecutionContextTask>, const String&)
{
}

void NullExecutionContext::setIsSecureContext(bool isSecureContext)
{
    m_isSecureContext = isSecureContext;
}

bool NullExecutionContext::isSecureContext(String& errorMessage, const SecureContextCheck privilegeContextCheck) const
{
    if (!m_isSecureContext)
        errorMessage = "A secure context is required";
    return m_isSecureContext;
}

} // namespace blink

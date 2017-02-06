/*
 * Copyright (c) 2011 Google Inc. All rights reserved.
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

#ifndef MainThreadDebugger_h
#define MainThreadDebugger_h

#include "bindings/core/v8/ScriptState.h"
#include "core/CoreExport.h"
#include "core/inspector/InspectorTaskRunner.h"
#include "core/inspector/ThreadDebugger.h"
#include "platform/heap/Handle.h"
#include <memory>
#include <v8.h>

namespace blink {

class LocalFrame;
class SecurityOrigin;

class CORE_EXPORT MainThreadDebugger final : public ThreadDebugger {
    WTF_MAKE_NONCOPYABLE(MainThreadDebugger);
public:
    class ClientMessageLoop {
        USING_FAST_MALLOC(ClientMessageLoop);
    public:
        virtual ~ClientMessageLoop() { }
        virtual void run(LocalFrame*) = 0;
        virtual void quitNow() = 0;
    };

    explicit MainThreadDebugger(v8::Isolate*);
    ~MainThreadDebugger() override;

    static MainThreadDebugger* instance();
    static void interruptMainThreadAndRun(std::unique_ptr<InspectorTaskRunner::Task>);

    InspectorTaskRunner* taskRunner() const { return m_taskRunner.get(); }
    bool isWorker() override { return false; }
    void setClientMessageLoop(std::unique_ptr<ClientMessageLoop>);
    int contextGroupId(LocalFrame*);
    void didClearContextsForFrame(LocalFrame*);
    void contextCreated(ScriptState*, LocalFrame*, SecurityOrigin*);
    void contextWillBeDestroyed(ScriptState*);

    void installAdditionalCommandLineAPI(v8::Local<v8::Context>, v8::Local<v8::Object>) override;

    v8::MaybeLocal<v8::Value> memoryInfo(v8::Isolate*, v8::Local<v8::Context>) override;
    void messageAddedToConsole(int contextGroupId, MessageSource, MessageLevel, const String16& message, const String16& url, unsigned lineNumber, unsigned columnNumber, V8StackTrace*) override;

private:
    // V8DebuggerClient implementation.
    void runMessageLoopOnPause(int contextGroupId) override;
    void quitMessageLoopOnPause() override;
    void muteWarningsAndDeprecations() override;
    void unmuteWarningsAndDeprecations() override;
    bool callingContextCanAccessContext(v8::Local<v8::Context> calling, v8::Local<v8::Context> target) override;
    v8::Local<v8::Context> ensureDefaultContextInGroup(int contextGroupId) override;

    std::unique_ptr<ClientMessageLoop> m_clientMessageLoop;
    std::unique_ptr<InspectorTaskRunner> m_taskRunner;

    static MainThreadDebugger* s_instance;

    static void querySelectorCallback(const v8::FunctionCallbackInfo<v8::Value>&);
    static void querySelectorAllCallback(const v8::FunctionCallbackInfo<v8::Value>&);
    static void xpathSelectorCallback(const v8::FunctionCallbackInfo<v8::Value>&);
};

} // namespace blink


#endif // MainThreadDebugger_h

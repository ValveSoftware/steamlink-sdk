/*
* Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef InspectorInstrumentationCustom_inl_h
#define InspectorInstrumentationCustom_inl_h

namespace WebCore {

namespace InspectorInstrumentation {

bool isDebuggerPausedImpl(InstrumentingAgents*);
bool collectingHTMLParseErrorsImpl(InstrumentingAgents*);
PassOwnPtr<ScriptSourceCode> preprocessImpl(InstrumentingAgents*, LocalFrame*, const ScriptSourceCode&);
String preprocessEventListenerImpl(InstrumentingAgents*, LocalFrame*, const String& source, const String& url, const String& functionName);

bool canvasAgentEnabled(ExecutionContext*);
bool consoleAgentEnabled(ExecutionContext*);
bool timelineAgentEnabled(ExecutionContext*);

inline bool isDebuggerPaused(LocalFrame* frame)
{
    FAST_RETURN_IF_NO_FRONTENDS(false);
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsFor(frame))
        return isDebuggerPausedImpl(instrumentingAgents);
    return false;
}

inline bool collectingHTMLParseErrors(Page* page)
{
    FAST_RETURN_IF_NO_FRONTENDS(false);
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsFor(page))
        return collectingHTMLParseErrorsImpl(instrumentingAgents);
    return false;
}

inline String preprocessEventListener(LocalFrame* frame, const String& source, const String& url, const String& functionName)
{
    FAST_RETURN_IF_NO_FRONTENDS(source);
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsFor(frame))
        return preprocessEventListenerImpl(instrumentingAgents, frame, source, url, functionName);
    return source;
}

inline PassOwnPtr<ScriptSourceCode> preprocess(LocalFrame* frame, const ScriptSourceCode& sourceCode)
{
    FAST_RETURN_IF_NO_FRONTENDS(PassOwnPtr<ScriptSourceCode>());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsFor(frame))
        return preprocessImpl(instrumentingAgents, frame, sourceCode);
    return PassOwnPtr<ScriptSourceCode>();
}

} // namespace InspectorInstrumentation

} // namespace WebCore

#endif // !defined(InspectorInstrumentationCustom_inl_h)

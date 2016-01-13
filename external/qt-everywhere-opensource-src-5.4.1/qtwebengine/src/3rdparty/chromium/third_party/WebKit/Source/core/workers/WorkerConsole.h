/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WorkerConsole_h
#define WorkerConsole_h

#include "bindings/v8/ScriptWrappable.h"
#include "core/inspector/ConsoleAPITypes.h"
#include "core/frame/ConsoleBase.h"
#include "core/frame/ConsoleTypes.h"
#include "core/workers/WorkerGlobalScope.h"
#include "platform/heap/Handle.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class ScriptArguments;

class WorkerConsole FINAL : public ConsoleBase, public ScriptWrappable {
public:
    static PassRefPtrWillBeRawPtr<WorkerConsole> create(WorkerGlobalScope* scope)
    {
        return adoptRefWillBeNoop(new WorkerConsole(scope));
    }
    virtual ~WorkerConsole();

    virtual void trace(Visitor*) OVERRIDE;

protected:
    virtual ExecutionContext* context() OVERRIDE;
    virtual void reportMessageToClient(MessageLevel, const String& message, PassRefPtrWillBeRawPtr<ScriptCallStack>) OVERRIDE;

private:
    explicit WorkerConsole(WorkerGlobalScope*);

    RawPtrWillBeMember<WorkerGlobalScope> m_scope;
};

} // namespace WebCore

#endif // WorkerConsole_h

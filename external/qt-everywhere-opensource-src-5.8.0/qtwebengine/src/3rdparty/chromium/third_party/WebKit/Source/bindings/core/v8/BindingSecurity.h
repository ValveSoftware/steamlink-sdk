/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef BindingSecurity_h
#define BindingSecurity_h

#include "core/CoreExport.h"
#include "wtf/Allocator.h"
#include <v8.h>

namespace blink {

class DOMWindow;
class EventTarget;
class ExceptionState;
class ExecutionContext;
class Frame;
class LocalDOMWindow;
class Location;
class MainThreadWorkletGlobalScope;
class Node;

enum SecurityReportingOption {
    DoNotReportSecurityError,
    ReportSecurityError,
};

// TODO(yukishiino): Remove the first argument of v8::Isolate*.  These functions
// just check if it's accessible or not, and never create anything.  So there is
// no need to pass the isolate.
class CORE_EXPORT BindingSecurity {
    STATIC_ONLY(BindingSecurity);
public:
    // Check if the caller (|accessingWindow|) is allowed to access the JS
    // receiver object (|target|), where the receiver object is the JS object
    // for which the DOM attribute or DOM operation is being invoked (in the
    // form of receiver.domAttr or receiver.domOp()).
    // Note that only Window and Location objects are cross-origin accessible
    // and that EventTarget interface is the parent interface of Window
    // interface.  So the receiver object must be of type DOMWindow,
    // EventTarget, or Location.
    //
    // DOMWindow
    static bool shouldAllowAccessTo(v8::Isolate*, const LocalDOMWindow* accessingWindow, const DOMWindow* target, ExceptionState&);
    static bool shouldAllowAccessTo(v8::Isolate*, const LocalDOMWindow* accessingWindow, const DOMWindow* target, SecurityReportingOption);
    // EventTarget (as the parent of DOMWindow)
    static bool shouldAllowAccessTo(v8::Isolate*, const LocalDOMWindow* accessingWindow, const EventTarget* target, ExceptionState&);  // NOLINT(readability/parameter_name)
    // Location
    static bool shouldAllowAccessTo(v8::Isolate*, const LocalDOMWindow* accessingWindow, const Location* target, ExceptionState&);
    static bool shouldAllowAccessTo(v8::Isolate*, const LocalDOMWindow* accessingWindow, const Location* target, SecurityReportingOption);
    // MainThreadWorkletGlobalScope
    static bool shouldAllowAccessTo(v8::Isolate*, v8::Local<v8::Context>, const ExecutionContext*, const MainThreadWorkletGlobalScope*, SecurityReportingOption);

    static bool shouldAllowAccessTo(v8::Isolate*, v8::Local<v8::Context> calling, v8::Local<v8::Context> target, SecurityReportingOption);
    // Prefer to use the previous overloads instead of falling back to using
    // Frame*.
    static bool shouldAllowAccessToFrame(v8::Isolate*, const LocalDOMWindow* accessingWindow, const Frame* target, SecurityReportingOption); // OBSOLETE

    // Check if the caller (|accessingWindow|) is allowed to access the JS
    // returned object (|target|), where the returned object is the JS object
    // which is returned as a result of invoking a DOM attribute or DOM
    // operation (in the form of
    //   var x = receiver.domAttr // or receiver.domOp()
    // where |x| is the returned object).
    // See window.frameElement for example, which may return a frame object.
    // The object returned from window.frameElement must be the same origin if
    // it's not null.
    //
    // Node
    static bool shouldAllowAccessTo(v8::Isolate*, const LocalDOMWindow* accessingWindow, const Node* target, ExceptionState&);
    static bool shouldAllowAccessTo(v8::Isolate*, const LocalDOMWindow* accessingWindow, const Node* target, SecurityReportingOption);
};

} // namespace blink

#endif

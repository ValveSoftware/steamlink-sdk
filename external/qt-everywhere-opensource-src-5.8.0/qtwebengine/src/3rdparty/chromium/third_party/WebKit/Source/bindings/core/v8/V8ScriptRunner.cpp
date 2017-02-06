/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "bindings/core/v8/V8ScriptRunner.h"

#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/ScriptStreamer.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8GCController.h"
#include "bindings/core/v8/V8ThrowException.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/fetch/CachedMetadata.h"
#include "core/fetch/ScriptResource.h"
#include "core/frame/LocalFrame.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "core/inspector/ThreadDebugger.h"
#include "platform/Histogram.h"
#include "platform/ScriptForbiddenScope.h"
#include "platform/TraceEvent.h"
#include "public/platform/Platform.h"
#include "wtf/CurrentTime.h"

#if OS(WIN)
#include <malloc.h>
#else
#include <alloca.h>
#endif

namespace blink {

namespace {

// Used to throw an exception before we exceed the C++ stack and crash.
// This limit was arrived at arbitrarily. crbug.com/449744
const int kMaxRecursionDepth = 44;

class V8CompileHistogram {
public:
    enum Cacheability {
        Cacheable,
        Noncacheable,
        InlineScript
    };
    explicit V8CompileHistogram(Cacheability);
    ~V8CompileHistogram();

private:
    Cacheability m_cacheability;
    double m_timeStamp;
};

V8CompileHistogram::V8CompileHistogram(V8CompileHistogram::Cacheability cacheability)
    : m_cacheability(cacheability)
    , m_timeStamp(WTF::currentTime())
{
}

V8CompileHistogram::~V8CompileHistogram()
{
    int64_t elapsedMicroSeconds = static_cast<int64_t>((WTF::currentTime() - m_timeStamp) * 1000000);
    switch (m_cacheability) {
    case Cacheable: {
        DEFINE_THREAD_SAFE_STATIC_LOCAL(CustomCountHistogram, compileCacheableHistogram, new CustomCountHistogram("V8.CompileCacheableMicroSeconds", 0, 1000000, 50));
        compileCacheableHistogram.count(elapsedMicroSeconds);
        break;
    }
    case Noncacheable: {
        DEFINE_THREAD_SAFE_STATIC_LOCAL(CustomCountHistogram, compileNonCacheableHistogram, new CustomCountHistogram("V8.CompileNoncacheableMicroSeconds", 0, 1000000, 50));
        compileNonCacheableHistogram.count(elapsedMicroSeconds);
        break;
    }
    case InlineScript: {
        DEFINE_THREAD_SAFE_STATIC_LOCAL(CustomCountHistogram, compileInlineHistogram, new CustomCountHistogram("V8.CompileInlineScriptMicroSeconds", 0, 1000000, 50));
        compileInlineHistogram.count(elapsedMicroSeconds);
        break;
    }
    }
}

// In order to make sure all pending messages to be processed in
// v8::Function::Call, we don't call throwStackOverflowException
// directly. Instead, we create a v8::Function of
// throwStackOverflowException and call it.
void throwStackOverflowException(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    V8ThrowException::throwRangeError(info.GetIsolate(), "Maximum call stack size exceeded.");
}

void throwScriptForbiddenException(v8::Isolate* isolate)
{
    V8ThrowException::throwGeneralError(isolate, "Script execution is forbidden.");
}

v8::Local<v8::Value> throwStackOverflowExceptionIfNeeded(v8::Isolate* isolate)
{
    if (V8PerIsolateData::from(isolate)->isHandlingRecursionLevelError()) {
        // If we are already handling a recursion level error, we should
        // not invoke v8::Function::Call.
        return v8::Undefined(isolate);
    }
    v8::MicrotasksScope microtasksScope(isolate, v8::MicrotasksScope::kDoNotRunMicrotasks);
    V8PerIsolateData::from(isolate)->setIsHandlingRecursionLevelError(true);
    v8::Local<v8::Value> result = v8::Function::New(isolate->GetCurrentContext(), throwStackOverflowException, v8::Local<v8::Value>(), 0, v8::ConstructorBehavior::kThrow).ToLocalChecked()->Call(v8::Undefined(isolate), 0, 0);
    V8PerIsolateData::from(isolate)->setIsHandlingRecursionLevelError(false);
    return result;
}

// Compile a script without any caching or compile options.
v8::MaybeLocal<v8::Script> compileWithoutOptions(V8CompileHistogram::Cacheability cacheability, v8::Isolate* isolate, v8::Local<v8::String> code, v8::ScriptOrigin origin)
{
    V8CompileHistogram histogramScope(cacheability);
    v8::ScriptCompiler::Source source(code, origin);
    return v8::ScriptCompiler::Compile(isolate->GetCurrentContext(), &source, v8::ScriptCompiler::kNoCompileOptions);
}

// Compile a script, and consume a V8 cache that was generated previously.
v8::MaybeLocal<v8::Script> compileAndConsumeCache(CachedMetadataHandler* cacheHandler, unsigned tag, v8::ScriptCompiler::CompileOptions compileOptions, v8::Isolate* isolate, v8::Local<v8::String> code, v8::ScriptOrigin origin)
{
    V8CompileHistogram histogramScope(V8CompileHistogram::Cacheable);
    CachedMetadata* cachedMetadata = cacheHandler->cachedMetadata(tag);
    const char* data = cachedMetadata->data();
    int length = cachedMetadata->size();
    v8::ScriptCompiler::CachedData* cachedData = new v8::ScriptCompiler::CachedData(
        reinterpret_cast<const uint8_t*>(data), length, v8::ScriptCompiler::CachedData::BufferNotOwned);
    v8::ScriptCompiler::Source source(code, origin, cachedData);
    v8::MaybeLocal<v8::Script> script = v8::ScriptCompiler::Compile(isolate->GetCurrentContext(), &source, compileOptions);
    if (cachedData->rejected)
        cacheHandler->clearCachedMetadata(CachedMetadataHandler::SendToPlatform);
    return script;
}

// Compile a script, and produce a V8 cache for future use.
v8::MaybeLocal<v8::Script> compileAndProduceCache(CachedMetadataHandler* cacheHandler, unsigned tag, v8::ScriptCompiler::CompileOptions compileOptions, CachedMetadataHandler::CacheType cacheType, v8::Isolate* isolate, v8::Local<v8::String> code, v8::ScriptOrigin origin)
{
    V8CompileHistogram histogramScope(V8CompileHistogram::Cacheable);
    v8::ScriptCompiler::Source source(code, origin);
    v8::MaybeLocal<v8::Script> script = v8::ScriptCompiler::Compile(isolate->GetCurrentContext(), &source, compileOptions);
    const v8::ScriptCompiler::CachedData* cachedData = source.GetCachedData();
    if (cachedData) {
        const char* data = reinterpret_cast<const char*>(cachedData->data);
        int length = cachedData->length;
        if (length > 1024) {
            // Omit histogram samples for small cache data to avoid outliers.
            int cacheSizeRatio = static_cast<int>(100.0 * length / code->Length());
            DEFINE_THREAD_SAFE_STATIC_LOCAL(CustomCountHistogram, codeCacheSizeHistogram, new CustomCountHistogram("V8.CodeCacheSizeRatio", 0, 10000, 50));
            codeCacheSizeHistogram.count(cacheSizeRatio);
        }
        cacheHandler->clearCachedMetadata(CachedMetadataHandler::CacheLocally);
        cacheHandler->setCachedMetadata(tag, data, length, cacheType);
    }
    return script;
}

// Compile a script, and consume or produce a V8 Cache, depending on whether the
// given resource already has cached data available.
v8::MaybeLocal<v8::Script> compileAndConsumeOrProduce(CachedMetadataHandler* cacheHandler, unsigned tag, v8::ScriptCompiler::CompileOptions consumeOptions, v8::ScriptCompiler::CompileOptions produceOptions, CachedMetadataHandler::CacheType cacheType, v8::Isolate* isolate, v8::Local<v8::String> code, v8::ScriptOrigin origin)
{
    return cacheHandler->cachedMetadata(tag)
        ? compileAndConsumeCache(cacheHandler, tag, consumeOptions, isolate, code, origin)
        : compileAndProduceCache(cacheHandler, tag, produceOptions, cacheType, isolate, code, origin);
}

enum CacheTagKind {
    CacheTagParser = 0,
    CacheTagCode = 1,
    CacheTagTimeStamp = 3,
    CacheTagLast
};

static const int kCacheTagKindSize = 2;

unsigned cacheTag(CacheTagKind kind, CachedMetadataHandler* cacheHandler)
{
    static_assert((1 << kCacheTagKindSize) >= CacheTagLast, "CacheTagLast must be large enough");

    static unsigned v8CacheDataVersion = v8::ScriptCompiler::CachedDataVersionTag() << kCacheTagKindSize;

    // A script can be (successfully) interpreted with different encodings,
    // depending on the page it appears in. The cache doesn't know anything
    // about encodings, but the cached data is specific to one encoding. If we
    // later load the script from the cache and interpret it with a different
    // encoding, the cached data is not valid for that encoding.
    return (v8CacheDataVersion | kind) + StringHash::hash(cacheHandler->encoding());
}

// Check previously stored timestamp.
bool isResourceHotForCaching(CachedMetadataHandler* cacheHandler, int hotHours)
{
    const double cacheWithinSeconds = hotHours * 60 * 60;
    unsigned tag = cacheTag(CacheTagTimeStamp, cacheHandler);
    CachedMetadata* cachedMetadata = cacheHandler->cachedMetadata(tag);
    if (!cachedMetadata)
        return false;
    double timeStamp;
    const int size = sizeof(timeStamp);
    ASSERT(cachedMetadata->size() == size);
    memcpy(&timeStamp, cachedMetadata->data(), size);
    return (WTF::currentTime() - timeStamp) < cacheWithinSeconds;
}

// Final compile call for a streamed compilation. Most decisions have already
// been made, but we need to write back data into the cache.
v8::MaybeLocal<v8::Script> postStreamCompile(V8CacheOptions cacheOptions, CachedMetadataHandler* cacheHandler, ScriptStreamer* streamer, v8::Isolate* isolate, v8::Local<v8::String> code, v8::ScriptOrigin origin)
{
    V8CompileHistogram histogramScope(V8CompileHistogram::Noncacheable);
    v8::MaybeLocal<v8::Script> script = v8::ScriptCompiler::Compile(isolate->GetCurrentContext(), streamer->source(), code, origin);

    if (!cacheHandler)
        return script;

    // If the non-streaming compiler uses the parser cache, retrieve and store
    // the cache data. If the code cache uses time stamp as heuristic, set that
    // time stamp.
    switch (cacheOptions) {
    case V8CacheOptionsParse: {
        const v8::ScriptCompiler::CachedData* newCachedData = streamer->source()->GetCachedData();
        if (!newCachedData)
            break;
        CachedMetadataHandler::CacheType cacheType = (cacheOptions == V8CacheOptionsParse) ? CachedMetadataHandler::SendToPlatform : CachedMetadataHandler::CacheLocally;
        cacheHandler->clearCachedMetadata(cacheType);
        cacheHandler->setCachedMetadata(cacheTag(CacheTagParser, cacheHandler), reinterpret_cast<const char*>(newCachedData->data), newCachedData->length, cacheType);
        break;
    }

    case V8CacheOptionsDefault:
    case V8CacheOptionsCode:
        V8ScriptRunner::setCacheTimeStamp(cacheHandler);
        break;

    case V8CacheOptionsAlways:
        // Currently V8CacheOptionsAlways doesn't support streaming.
        ASSERT_NOT_REACHED();
    case V8CacheOptionsNone:
        break;
    }
    return script;
}

typedef Function<v8::MaybeLocal<v8::Script>(v8::Isolate*, v8::Local<v8::String>, v8::ScriptOrigin)> CompileFn;

// A notation convenience: WTF::bind<...> needs to be given the right argument
// types. We have an awful lot of bind calls below, all with the same types, so
// this local bind lets WTF::bind to all the work, but 'knows' the right
// parameter types.
// This version isn't quite as smart as the real WTF::bind, though, so you
// sometimes may still have to call the original.
template<typename... A>
std::unique_ptr<CompileFn> bind(const A&... args)
{
    return WTF::bind(args...);
}

// Select a compile function from any of the above, mainly depending on
// cacheOptions.
std::unique_ptr<CompileFn> selectCompileFunction(V8CacheOptions cacheOptions, CachedMetadataHandler* cacheHandler, v8::Local<v8::String> code, V8CompileHistogram::Cacheability cacheabilityIfNoHandler)
{
    static const int minimalCodeLength = 1024;
    static const int hotHours = 72;

    // Caching is not available in this case.
    if (!cacheHandler)
        return bind(compileWithoutOptions, cacheabilityIfNoHandler);

    if (cacheOptions == V8CacheOptionsNone)
        return bind(compileWithoutOptions, V8CompileHistogram::Cacheable);

    // Caching is not worthwhile for small scripts.  Do not use caching
    // unless explicitly expected, indicated by the cache option.
    if (code->Length() < minimalCodeLength)
        return bind(compileWithoutOptions, V8CompileHistogram::Cacheable);

    // The cacheOptions will guide our strategy:
    switch (cacheOptions) {
    case V8CacheOptionsParse:
        // Use parser-cache; in-memory only.
        return bind(compileAndConsumeOrProduce, wrapPersistent(cacheHandler), cacheTag(CacheTagParser, cacheHandler), v8::ScriptCompiler::kConsumeParserCache, v8::ScriptCompiler::kProduceParserCache, CachedMetadataHandler::CacheLocally);
        break;

    case V8CacheOptionsDefault:
    case V8CacheOptionsCode:
    case V8CacheOptionsAlways: {
        // Use code caching for recently seen resources.
        // Use compression depending on the cache option.
        unsigned codeCacheTag = cacheTag(CacheTagCode, cacheHandler);
        CachedMetadata* codeCache = cacheHandler->cachedMetadata(codeCacheTag);
        if (codeCache)
            return bind(compileAndConsumeCache, wrapPersistent(cacheHandler), codeCacheTag, v8::ScriptCompiler::kConsumeCodeCache);
        if (cacheOptions != V8CacheOptionsAlways && !isResourceHotForCaching(cacheHandler, hotHours)) {
            V8ScriptRunner::setCacheTimeStamp(cacheHandler);
            return bind(compileWithoutOptions, V8CompileHistogram::Cacheable);
        }
        return bind(compileAndProduceCache, wrapPersistent(cacheHandler), codeCacheTag, v8::ScriptCompiler::kProduceCodeCache, CachedMetadataHandler::SendToPlatform);
        break;
    }

    case V8CacheOptionsNone:
        // Shouldn't happen, as this is handled above.
        // Case is here so that compiler can check all cases are handled.
        ASSERT_NOT_REACHED();
        break;
    }

    // All switch branches should return and we should never get here.
    // But some compilers aren't sure, hence this default.
    ASSERT_NOT_REACHED();
    return bind(compileWithoutOptions, V8CompileHistogram::Cacheable);
}

// Select a compile function for a streaming compile.
std::unique_ptr<CompileFn> selectCompileFunction(V8CacheOptions cacheOptions, ScriptResource* resource, ScriptStreamer* streamer)
{
    // We don't stream scripts which don't have a Resource.
    ASSERT(resource);
    // Failed resources should never get this far.
    ASSERT(!resource->errorOccurred());
    ASSERT(streamer->isFinished());
    ASSERT(!streamer->streamingSuppressed());
    return WTF::bind(postStreamCompile, cacheOptions, wrapPersistent(resource->cacheHandler()), wrapPersistent(streamer));
}
} // namespace

v8::MaybeLocal<v8::Script> V8ScriptRunner::compileScript(const ScriptSourceCode& source, v8::Isolate* isolate, AccessControlStatus accessControlStatus, V8CacheOptions cacheOptions)
{
    if (source.source().length() >= v8::String::kMaxLength) {
        V8ThrowException::throwGeneralError(isolate, "Source file too large.");
        return v8::Local<v8::Script>();
    }
    return compileScript(v8String(isolate, source.source()), source.url(), source.sourceMapUrl(), source.startPosition(), isolate, source.resource(), source.streamer(), source.resource() ? source.resource()->cacheHandler() : nullptr, accessControlStatus, cacheOptions);
}

v8::MaybeLocal<v8::Script> V8ScriptRunner::compileScript(const CompressibleString& code, const String& fileName, const String& sourceMapUrl, const TextPosition& textPosition, v8::Isolate* isolate, CachedMetadataHandler* cacheMetadataHandler, AccessControlStatus accessControlStatus, V8CacheOptions v8CacheOptions)
{
    if (code.length() >= v8::String::kMaxLength) {
        V8ThrowException::throwGeneralError(isolate, "Source file too large.");
        return v8::Local<v8::Script>();
    }
    return compileScript(v8String(isolate, code), fileName, sourceMapUrl, textPosition, isolate, nullptr, nullptr, cacheMetadataHandler, accessControlStatus, v8CacheOptions);
}

v8::MaybeLocal<v8::Script> V8ScriptRunner::compileScript(v8::Local<v8::String> code, const String& fileName, const String& sourceMapUrl, const TextPosition& scriptStartPosition, v8::Isolate* isolate, ScriptResource* resource, ScriptStreamer* streamer, CachedMetadataHandler* cacheHandler, AccessControlStatus accessControlStatus, V8CacheOptions cacheOptions, bool isInternalScript)
{
    TRACE_EVENT2("v8,devtools.timeline", "v8.compile", "fileName", fileName.utf8(), "data", InspectorCompileScriptEvent::data(fileName, scriptStartPosition));
    TRACE_EVENT_SCOPED_SAMPLING_STATE("v8", "V8Compile");

    ASSERT(!streamer || resource);
    ASSERT(!resource || resource->cacheHandler() == cacheHandler);

    // NOTE: For compatibility with WebCore, ScriptSourceCode's line starts at
    // 1, whereas v8 starts at 0.
    v8::ScriptOrigin origin(
        v8String(isolate, fileName),
        v8::Integer::New(isolate, scriptStartPosition.m_line.zeroBasedInt()),
        v8::Integer::New(isolate, scriptStartPosition.m_column.zeroBasedInt()),
        v8Boolean(accessControlStatus == SharableCrossOrigin, isolate),
        v8::Local<v8::Integer>(),
        v8Boolean(isInternalScript, isolate),
        v8String(isolate, sourceMapUrl),
        v8Boolean(accessControlStatus == OpaqueResource, isolate));

    V8CompileHistogram::Cacheability cacheabilityIfNoHandler = V8CompileHistogram::Cacheability::Noncacheable;
    if (!cacheHandler && (scriptStartPosition.m_line.zeroBasedInt() == 0) && (scriptStartPosition.m_column.zeroBasedInt() == 0))
        cacheabilityIfNoHandler = V8CompileHistogram::Cacheability::InlineScript;

    std::unique_ptr<CompileFn> compileFn = streamer
        ? selectCompileFunction(cacheOptions, resource, streamer)
        : selectCompileFunction(cacheOptions, cacheHandler, code, cacheabilityIfNoHandler);

    return (*compileFn)(isolate, code, origin);
}

v8::MaybeLocal<v8::Value> V8ScriptRunner::runCompiledScript(v8::Isolate* isolate, v8::Local<v8::Script> script, ExecutionContext* context)
{
    ASSERT(!script.IsEmpty());
    TRACE_EVENT_SCOPED_SAMPLING_STATE("v8", "V8Execution");
    TRACE_EVENT1("v8", "v8.run", "fileName", TRACE_STR_COPY(*v8::String::Utf8Value(script->GetUnboundScript()->GetScriptName())));

    if (v8::MicrotasksScope::GetCurrentDepth(isolate) >= kMaxRecursionDepth)
        return throwStackOverflowExceptionIfNeeded(isolate);

    RELEASE_ASSERT(!context->isIteratingOverObservers());

    // Run the script and keep track of the current recursion depth.
    v8::MaybeLocal<v8::Value> result;
    {
        if (ScriptForbiddenScope::isScriptForbidden()) {
            throwScriptForbiddenException(isolate);
            return v8::MaybeLocal<v8::Value>();
        }
        v8::MicrotasksScope microtasksScope(isolate, v8::MicrotasksScope::kRunMicrotasks);
        ThreadDebugger::willExecuteScript(isolate, script->GetUnboundScript()->GetId());
        result = script->Run(isolate->GetCurrentContext());
        ThreadDebugger::didExecuteScript(isolate);
    }

    crashIfIsolateIsDead(isolate);
    return result;
}

v8::MaybeLocal<v8::Value> V8ScriptRunner::compileAndRunInternalScript(v8::Local<v8::String> source, v8::Isolate* isolate, const String& fileName, const TextPosition& scriptStartPosition)
{
    v8::Local<v8::Script> script;
    if (!V8ScriptRunner::compileScript(source, fileName, String(), scriptStartPosition, isolate, nullptr, nullptr, nullptr, SharableCrossOrigin, V8CacheOptionsDefault, true).ToLocal(&script))
        return v8::MaybeLocal<v8::Value>();

    TRACE_EVENT0("v8", "v8.run");
    TRACE_EVENT_SCOPED_SAMPLING_STATE("v8", "V8Execution");
    v8::MicrotasksScope microtasksScope(isolate, v8::MicrotasksScope::kDoNotRunMicrotasks);
    v8::MaybeLocal<v8::Value> result = script->Run(isolate->GetCurrentContext());
    crashIfIsolateIsDead(isolate);
    return result;
}

v8::MaybeLocal<v8::Value> V8ScriptRunner::runCompiledInternalScript(v8::Isolate* isolate, v8::Local<v8::Script> script)
{
    TRACE_EVENT0("v8", "v8.run");
    TRACE_EVENT_SCOPED_SAMPLING_STATE("v8", "V8Execution");
    v8::MicrotasksScope microtasksScope(isolate, v8::MicrotasksScope::kDoNotRunMicrotasks);
    v8::MaybeLocal<v8::Value> result = script->Run(isolate->GetCurrentContext());
    crashIfIsolateIsDead(isolate);
    return result;
}

v8::MaybeLocal<v8::Value> V8ScriptRunner::callAsConstructor(v8::Isolate* isolate, v8::Local<v8::Object> constructor, ExecutionContext* context, int argc, v8::Local<v8::Value> argv[])
{
    TRACE_EVENT0("v8", "v8.callAsConstructor");
    TRACE_EVENT_SCOPED_SAMPLING_STATE("v8", "V8Execution");

    int depth = v8::MicrotasksScope::GetCurrentDepth(isolate);
    if (depth >= kMaxRecursionDepth)
        return v8::MaybeLocal<v8::Value>(throwStackOverflowExceptionIfNeeded(isolate));

    CHECK(!context->isIteratingOverObservers());

    if (ScriptForbiddenScope::isScriptForbidden()) {
        throwScriptForbiddenException(isolate);
        return v8::MaybeLocal<v8::Value>();
    }

    // TODO(dominicc): When inspector supports tracing object
    // invocation, change this to use v8::Object instead of
    // v8::Function. All callers use functions because
    // CustomElementsRegistry#define's IDL signature is Function.
    CHECK(constructor->IsFunction());
    v8::Local<v8::Function> function = constructor.As<v8::Function>();

    if (!depth)
        TRACE_EVENT_BEGIN1("devtools.timeline", "FunctionCall", "data", InspectorFunctionCallEvent::data(context, function));
    v8::MaybeLocal<v8::Value> result;
    {
        // Create an extra block so FunctionCall trace event end phase is recorded after
        // v8::MicrotasksScope destructor, as the latter is running microtasks.
        v8::MicrotasksScope microtasksScope(isolate, v8::MicrotasksScope::kRunMicrotasks);
        ThreadDebugger::willExecuteScript(isolate, function->ScriptId());
        result = constructor->CallAsConstructor(isolate->GetCurrentContext(), argc, argv);
        crashIfIsolateIsDead(isolate);
        ThreadDebugger::didExecuteScript(isolate);
    }
    if (!depth)
        TRACE_EVENT_END0("devtools.timeline", "FunctionCall");
    return result;
}

v8::MaybeLocal<v8::Value> V8ScriptRunner::callFunction(v8::Local<v8::Function> function, ExecutionContext* context, v8::Local<v8::Value> receiver, int argc, v8::Local<v8::Value> args[], v8::Isolate* isolate)
{
    ScopedFrameBlamer frameBlamer(context->isDocument() ? toDocument(context)->frame() : nullptr);
    TRACE_EVENT0("v8", "v8.callFunction");
    TRACE_EVENT_SCOPED_SAMPLING_STATE("v8", "V8Execution");

    int depth = v8::MicrotasksScope::GetCurrentDepth(isolate);
    if (depth >= kMaxRecursionDepth)
        return v8::MaybeLocal<v8::Value>(throwStackOverflowExceptionIfNeeded(isolate));

    RELEASE_ASSERT(!context->isIteratingOverObservers());

    if (ScriptForbiddenScope::isScriptForbidden()) {
        throwScriptForbiddenException(isolate);
        return v8::MaybeLocal<v8::Value>();
    }
    if (!depth)
        TRACE_EVENT_BEGIN1("devtools.timeline", "FunctionCall", "data", InspectorFunctionCallEvent::data(context, function));
    v8::MaybeLocal<v8::Value> result;
    {
        // Create an extra block so FunctionCall trace event end phase is recorded after
        // v8::MicrotasksScope destructor, as the latter is running microtasks.
        v8::MicrotasksScope microtasksScope(isolate, v8::MicrotasksScope::kRunMicrotasks);
        ThreadDebugger::willExecuteScript(isolate, function->ScriptId());
        result = function->Call(isolate->GetCurrentContext(), receiver, argc, args);
        crashIfIsolateIsDead(isolate);
        ThreadDebugger::didExecuteScript(isolate);
    }
    if (!depth)
        TRACE_EVENT_END0("devtools.timeline", "FunctionCall");
    return result;
}

v8::MaybeLocal<v8::Value> V8ScriptRunner::callInternalFunction(v8::Local<v8::Function> function, v8::Local<v8::Value> receiver, int argc, v8::Local<v8::Value> args[], v8::Isolate* isolate)
{
    TRACE_EVENT0("v8", "v8.callFunction");
    TRACE_EVENT_SCOPED_SAMPLING_STATE("v8", "V8Execution");
    v8::MicrotasksScope microtasksScope(isolate, v8::MicrotasksScope::kDoNotRunMicrotasks);
    v8::MaybeLocal<v8::Value> result = function->Call(isolate->GetCurrentContext(), receiver, argc, args);
    crashIfIsolateIsDead(isolate);
    return result;
}

v8::MaybeLocal<v8::Object> V8ScriptRunner::instantiateObject(v8::Isolate* isolate, v8::Local<v8::ObjectTemplate> objectTemplate)
{
    TRACE_EVENT0("v8", "v8.newInstance");
    TRACE_EVENT_SCOPED_SAMPLING_STATE("v8", "V8Execution");

    v8::MicrotasksScope microtasksScope(isolate, v8::MicrotasksScope::kDoNotRunMicrotasks);
    v8::MaybeLocal<v8::Object> result = objectTemplate->NewInstance(isolate->GetCurrentContext());
    crashIfIsolateIsDead(isolate);
    return result;
}

v8::MaybeLocal<v8::Object> V8ScriptRunner::instantiateObject(v8::Isolate* isolate, v8::Local<v8::Function> function, int argc, v8::Local<v8::Value> argv[])
{
    TRACE_EVENT0("v8", "v8.newInstance");
    TRACE_EVENT_SCOPED_SAMPLING_STATE("v8", "V8Execution");

    v8::MicrotasksScope microtasksScope(isolate, v8::MicrotasksScope::kDoNotRunMicrotasks);
    v8::MaybeLocal<v8::Object> result = function->NewInstance(isolate->GetCurrentContext(), argc, argv);
    crashIfIsolateIsDead(isolate);
    return result;
}

v8::MaybeLocal<v8::Object> V8ScriptRunner::instantiateObjectInDocument(v8::Isolate* isolate, v8::Local<v8::Function> function, ExecutionContext* context, int argc, v8::Local<v8::Value> argv[])
{
    TRACE_EVENT0("v8", "v8.newInstance");
    TRACE_EVENT_SCOPED_SAMPLING_STATE("v8", "V8Execution");
    if (ScriptForbiddenScope::isScriptForbidden()) {
        throwScriptForbiddenException(isolate);
        return v8::MaybeLocal<v8::Object>();
    }
    v8::MicrotasksScope microtasksScope(isolate, v8::MicrotasksScope::kRunMicrotasks);
    v8::MaybeLocal<v8::Object> result = function->NewInstance(isolate->GetCurrentContext(), argc, argv);
    crashIfIsolateIsDead(isolate);
    return result;
}

unsigned V8ScriptRunner::tagForParserCache(CachedMetadataHandler* cacheHandler)
{
    return cacheTag(CacheTagParser, cacheHandler);
}

unsigned V8ScriptRunner::tagForCodeCache(CachedMetadataHandler* cacheHandler)
{
    return cacheTag(CacheTagCode, cacheHandler);
}

// Store a timestamp to the cache as hint.
void V8ScriptRunner::setCacheTimeStamp(CachedMetadataHandler* cacheHandler)
{
    double now = WTF::currentTime();
    unsigned tag = cacheTag(CacheTagTimeStamp, cacheHandler);
    cacheHandler->clearCachedMetadata(CachedMetadataHandler::CacheLocally);
    cacheHandler->setCachedMetadata(tag, reinterpret_cast<char*>(&now), sizeof(now), CachedMetadataHandler::SendToPlatform);
}

} // namespace blink

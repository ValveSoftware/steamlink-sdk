// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "bindings/core/v8/ScriptStreamer.h"

#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/ScriptStreamerThread.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "bindings/core/v8/V8ScriptRunner.h"
#include "core/dom/Element.h"
#include "core/dom/PendingScript.h"
#include "core/frame/Settings.h"
#include "platform/heap/Handle.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/Platform.h"
#include "public/platform/WebScheduler.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>
#include <v8.h>

namespace blink {

namespace {

class ScriptStreamingTest : public ::testing::Test {
public:
    ScriptStreamingTest()
        : m_loadingTaskRunner(Platform::current()->currentThread()->scheduler()->loadingTaskRunner())
        , m_settings(Settings::create())
        , m_resourceRequest("http://www.streaming-test.com/")
        , m_resource(ScriptResource::create(m_resourceRequest, "UTF-8"))
        , m_pendingScript(PendingScript::create(0, m_resource.get()))
    {
        m_resource->setStatus(Resource::Pending);
        m_pendingScript = PendingScript::create(0, m_resource.get());
        ScriptStreamer::setSmallScriptThresholdForTesting(0);
    }

    ~ScriptStreamingTest()
    {
        if (m_pendingScript)
            m_pendingScript->dispose();
    }

    PendingScript* getPendingScript() const { return m_pendingScript.get(); }

protected:
    void appendData(const char* data)
    {
        m_resource->appendData(data, strlen(data));
        // Yield control to the background thread, so that V8 gets a chance to
        // process the data before the main thread adds more. Note that we
        // cannot fully control in what kind of chunks the data is passed to V8
        // (if V8 is not requesting more data between two appendData calls, it
        // will get both chunks together).
        testing::yieldCurrentThread();
    }

    void appendPadding()
    {
        for (int i = 0; i < 10; ++i) {
            appendData(" /* this is padding to make the script long enough, so "
                "that V8's buffer gets filled and it starts processing "
                "the data */ ");
        }
    }

    void finish()
    {
        m_resource->finish();
        m_resource->setStatus(Resource::Cached);
    }

    void processTasksUntilStreamingComplete()
    {
        while (ScriptStreamerThread::shared()->isRunningTask()) {
            testing::runPendingTasks();
        }
        // Once more, because the "streaming complete" notification might only
        // now be in the task queue.
        testing::runPendingTasks();
    }

    WebTaskRunner* m_loadingTaskRunner; // NOT OWNED
    std::unique_ptr<Settings> m_settings;
    // The Resource and PendingScript where we stream from. These don't really
    // fetch any data outside the test; the test controls the data by calling
    // ScriptResource::appendData.
    ResourceRequest m_resourceRequest;
    Persistent<ScriptResource> m_resource;
    Persistent<PendingScript> m_pendingScript;
};

class TestScriptResourceClient : public GarbageCollectedFinalized<TestScriptResourceClient>, public ScriptResourceClient {
    USING_GARBAGE_COLLECTED_MIXIN(TestScriptResourceClient);
public:
    TestScriptResourceClient()
        : m_finished(false) { }

    void notifyFinished(Resource*) override { m_finished = true; }
    String debugName() const override { return "TestScriptResourceClient"; }

    bool finished() const { return m_finished; }

private:
    bool m_finished;
};

#if OS(MACOSX) && defined(ADDRESS_SANITIZER)
// TODO(marja): Fix this test, http://crbug.com/572987
#define MAYBE_CompilingStreamedScript DISABLED_CompilingStreamedScript
#else
#define MAYBE_CompilingStreamedScript CompilingStreamedScript
#endif
TEST_F(ScriptStreamingTest, MAYBE_CompilingStreamedScript)
{
    // Test that we can successfully compile a streamed script.
    V8TestingScope scope;
    ScriptStreamer::startStreaming(getPendingScript(), ScriptStreamer::ParsingBlocking, m_settings.get(), scope.getScriptState(), m_loadingTaskRunner);
    TestScriptResourceClient* client = new TestScriptResourceClient;
    getPendingScript()->watchForLoad(client);

    appendData("function foo() {");
    appendPadding();
    appendData("return 5; }");
    appendPadding();
    appendData("foo();");
    EXPECT_FALSE(client->finished());
    finish();

    // Process tasks on the main thread until the streaming background thread
    // has completed its tasks.
    processTasksUntilStreamingComplete();
    EXPECT_TRUE(client->finished());
    bool errorOccurred = false;
    ScriptSourceCode sourceCode = getPendingScript()->getSource(KURL(), errorOccurred);
    EXPECT_FALSE(errorOccurred);
    EXPECT_TRUE(sourceCode.streamer());
    v8::TryCatch tryCatch(scope.isolate());
    v8::Local<v8::Script> script;
    EXPECT_TRUE(V8ScriptRunner::compileScript(sourceCode, scope.isolate()).ToLocal(&script));
    EXPECT_FALSE(tryCatch.HasCaught());
}

TEST_F(ScriptStreamingTest, CompilingStreamedScriptWithParseError)
{
    // Test that scripts with parse errors are handled properly. In those cases,
    // the V8 side typically finished before loading finishes: make sure we
    // handle it gracefully.
    V8TestingScope scope;
    ScriptStreamer::startStreaming(getPendingScript(), ScriptStreamer::ParsingBlocking, m_settings.get(), scope.getScriptState(), m_loadingTaskRunner);
    TestScriptResourceClient* client = new TestScriptResourceClient;
    getPendingScript()->watchForLoad(client);
    appendData("function foo() {");
    appendData("this is the part which will be a parse error");
    // V8 won't realize the parse error until it actually starts parsing the
    // script, and this happens only when its buffer is filled.
    appendPadding();

    EXPECT_FALSE(client->finished());

    // Force the V8 side to finish before the loading.
    processTasksUntilStreamingComplete();
    EXPECT_FALSE(client->finished());

    finish();
    EXPECT_TRUE(client->finished());

    bool errorOccurred = false;
    ScriptSourceCode sourceCode = getPendingScript()->getSource(KURL(), errorOccurred);
    EXPECT_FALSE(errorOccurred);
    EXPECT_TRUE(sourceCode.streamer());
    v8::TryCatch tryCatch(scope.isolate());
    v8::Local<v8::Script> script;
    EXPECT_FALSE(V8ScriptRunner::compileScript(sourceCode, scope.isolate()).ToLocal(&script));
    EXPECT_TRUE(tryCatch.HasCaught());
}

TEST_F(ScriptStreamingTest, CancellingStreaming)
{
    // Test that the upper layers (PendingScript and up) can be ramped down
    // while streaming is ongoing, and ScriptStreamer handles it gracefully.
    V8TestingScope scope;
    ScriptStreamer::startStreaming(getPendingScript(), ScriptStreamer::ParsingBlocking, m_settings.get(), scope.getScriptState(), m_loadingTaskRunner);
    TestScriptResourceClient* client = new TestScriptResourceClient;
    getPendingScript()->watchForLoad(client);
    appendData("function foo() {");

    // In general, we cannot control what the background thread is doing
    // (whether it's parsing or waiting for more data). In this test, we have
    // given it so little data that it's surely waiting for more.

    // Simulate cancelling the network load (e.g., because the user navigated
    // away).
    EXPECT_FALSE(client->finished());
    getPendingScript()->stopWatchingForLoad();
    getPendingScript()->releaseElementAndClear();
    m_pendingScript = nullptr; // This will destroy m_resource.
    m_resource = nullptr;

    // The V8 side will complete too. This should not crash. We don't receive
    // any results from the streaming and the client doesn't get notified.
    processTasksUntilStreamingComplete();
    EXPECT_FALSE(client->finished());
}

TEST_F(ScriptStreamingTest, SuppressingStreaming)
{
    // If we notice during streaming that there is a code cache, streaming
    // is suppressed (V8 doesn't parse while the script is loading), and the
    // upper layer (ScriptResourceClient) should get a notification when the
    // script is loaded.
    V8TestingScope scope;
    ScriptStreamer::startStreaming(getPendingScript(), ScriptStreamer::ParsingBlocking, m_settings.get(), scope.getScriptState(), m_loadingTaskRunner);
    TestScriptResourceClient* client = new TestScriptResourceClient;
    getPendingScript()->watchForLoad(client);
    appendData("function foo() {");
    appendPadding();

    CachedMetadataHandler* cacheHandler = m_resource->cacheHandler();
    EXPECT_TRUE(cacheHandler);
    cacheHandler->setCachedMetadata(V8ScriptRunner::tagForCodeCache(cacheHandler), "X", 1, CachedMetadataHandler::CacheLocally);

    appendPadding();
    finish();
    processTasksUntilStreamingComplete();
    EXPECT_TRUE(client->finished());

    bool errorOccurred = false;
    ScriptSourceCode sourceCode = getPendingScript()->getSource(KURL(), errorOccurred);
    EXPECT_FALSE(errorOccurred);
    // ScriptSourceCode doesn't refer to the streamer, since we have suppressed
    // the streaming and resumed the non-streaming code path for script
    // compilation.
    EXPECT_FALSE(sourceCode.streamer());
}

TEST_F(ScriptStreamingTest, EmptyScripts)
{
    // Empty scripts should also be streamed properly, that is, the upper layer
    // (ScriptResourceClient) should be notified when an empty script has been
    // loaded.
    V8TestingScope scope;
    ScriptStreamer::startStreaming(getPendingScript(), ScriptStreamer::ParsingBlocking, m_settings.get(), scope.getScriptState(), m_loadingTaskRunner);
    TestScriptResourceClient* client = new TestScriptResourceClient;
    getPendingScript()->watchForLoad(client);

    // Finish the script without sending any data.
    finish();
    // The finished notification should arrive immediately and not be cycled
    // through a background thread.
    EXPECT_TRUE(client->finished());

    bool errorOccurred = false;
    ScriptSourceCode sourceCode = getPendingScript()->getSource(KURL(), errorOccurred);
    EXPECT_FALSE(errorOccurred);
    EXPECT_FALSE(sourceCode.streamer());
}

TEST_F(ScriptStreamingTest, SmallScripts)
{
    // Small scripts shouldn't be streamed.
    V8TestingScope scope;
    ScriptStreamer::setSmallScriptThresholdForTesting(100);

    ScriptStreamer::startStreaming(getPendingScript(), ScriptStreamer::ParsingBlocking, m_settings.get(), scope.getScriptState(), m_loadingTaskRunner);
    TestScriptResourceClient* client = new TestScriptResourceClient;
    getPendingScript()->watchForLoad(client);

    appendData("function foo() { }");

    finish();

    // The finished notification should arrive immediately and not be cycled
    // through a background thread.
    EXPECT_TRUE(client->finished());

    bool errorOccurred = false;
    ScriptSourceCode sourceCode = getPendingScript()->getSource(KURL(), errorOccurred);
    EXPECT_FALSE(errorOccurred);
    EXPECT_FALSE(sourceCode.streamer());
}

#if OS(MACOSX) && defined(ADDRESS_SANITIZER)
// TODO(marja): Fix this test, http://crbug.com/572987
#define MAYBE_ScriptsWithSmallFirstChunk DISABLED_ScriptsWithSmallFirstChunk
#else
#define MAYBE_ScriptsWithSmallFirstChunk ScriptsWithSmallFirstChunk
#endif
TEST_F(ScriptStreamingTest, MAYBE_ScriptsWithSmallFirstChunk)
{
    // If a script is long enough, if should be streamed, even if the first data
    // chunk is small.
    V8TestingScope scope;
    ScriptStreamer::setSmallScriptThresholdForTesting(100);

    ScriptStreamer::startStreaming(getPendingScript(), ScriptStreamer::ParsingBlocking, m_settings.get(), scope.getScriptState(), m_loadingTaskRunner);
    TestScriptResourceClient* client = new TestScriptResourceClient;
    getPendingScript()->watchForLoad(client);

    // This is the first data chunk which is small.
    appendData("function foo() { }");
    appendPadding();
    appendPadding();
    appendPadding();

    finish();

    processTasksUntilStreamingComplete();
    EXPECT_TRUE(client->finished());
    bool errorOccurred = false;
    ScriptSourceCode sourceCode = getPendingScript()->getSource(KURL(), errorOccurred);
    EXPECT_FALSE(errorOccurred);
    EXPECT_TRUE(sourceCode.streamer());
    v8::TryCatch tryCatch(scope.isolate());
    v8::Local<v8::Script> script;
    EXPECT_TRUE(V8ScriptRunner::compileScript(sourceCode, scope.isolate()).ToLocal(&script));
    EXPECT_FALSE(tryCatch.HasCaught());
}

#if OS(MACOSX) && defined(ADDRESS_SANITIZER)
// TODO(marja): Fix this test, http://crbug.com/572987
#define MAYBE_EncodingChanges DISABLED_EncodingChanges
#else
#define MAYBE_EncodingChanges EncodingChanges
#endif
TEST_F(ScriptStreamingTest, MAYBE_EncodingChanges)
{
    // It's possible that the encoding of the Resource changes after we start
    // loading it.
    V8TestingScope scope;
    m_resource->setEncoding("windows-1252");

    ScriptStreamer::startStreaming(getPendingScript(), ScriptStreamer::ParsingBlocking, m_settings.get(), scope.getScriptState(), m_loadingTaskRunner);
    TestScriptResourceClient* client = new TestScriptResourceClient;
    getPendingScript()->watchForLoad(client);

    m_resource->setEncoding("UTF-8");
    // \xec\x92\x81 are the raw bytes for \uc481.
    appendData("function foo() { var foob\xec\x92\x81r = 13; return foob\xec\x92\x81r; } foo();");

    finish();

    processTasksUntilStreamingComplete();
    EXPECT_TRUE(client->finished());
    bool errorOccurred = false;
    ScriptSourceCode sourceCode = getPendingScript()->getSource(KURL(), errorOccurred);
    EXPECT_FALSE(errorOccurred);
    EXPECT_TRUE(sourceCode.streamer());
    v8::TryCatch tryCatch(scope.isolate());
    v8::Local<v8::Script> script;
    EXPECT_TRUE(V8ScriptRunner::compileScript(sourceCode, scope.isolate()).ToLocal(&script));
    EXPECT_FALSE(tryCatch.HasCaught());
}


#if OS(MACOSX) && defined(ADDRESS_SANITIZER)
// TODO(marja): Fix this test, http://crbug.com/572987
#define MAYBE_EncodingFromBOM DISABLED_EncodingFromBOM
#else
#define MAYBE_EncodingFromBOM EncodingFromBOM
#endif
TEST_F(ScriptStreamingTest, MAYBE_EncodingFromBOM)
{
    // Byte order marks should be removed before giving the data to V8. They
    // will also affect encoding detection.
    V8TestingScope scope;
    m_resource->setEncoding("windows-1252"); // This encoding is wrong on purpose.

    ScriptStreamer::startStreaming(getPendingScript(), ScriptStreamer::ParsingBlocking, m_settings.get(), scope.getScriptState(), m_loadingTaskRunner);
    TestScriptResourceClient* client = new TestScriptResourceClient;
    getPendingScript()->watchForLoad(client);

    // \xef\xbb\xbf is the UTF-8 byte order mark. \xec\x92\x81 are the raw bytes
    // for \uc481.
    appendData("\xef\xbb\xbf function foo() { var foob\xec\x92\x81r = 13; return foob\xec\x92\x81r; } foo();");

    finish();
    processTasksUntilStreamingComplete();
    EXPECT_TRUE(client->finished());
    bool errorOccurred = false;
    ScriptSourceCode sourceCode = getPendingScript()->getSource(KURL(), errorOccurred);
    EXPECT_FALSE(errorOccurred);
    EXPECT_TRUE(sourceCode.streamer());
    v8::TryCatch tryCatch(scope.isolate());
    v8::Local<v8::Script> script;
    EXPECT_TRUE(V8ScriptRunner::compileScript(sourceCode, scope.isolate()).ToLocal(&script));
    EXPECT_FALSE(tryCatch.HasCaught());
}

} // namespace

} // namespace blink

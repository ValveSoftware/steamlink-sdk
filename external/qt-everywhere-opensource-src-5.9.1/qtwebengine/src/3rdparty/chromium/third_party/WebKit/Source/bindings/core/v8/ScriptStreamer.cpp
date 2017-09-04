// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/ScriptStreamer.h"

#include "bindings/core/v8/ScriptStreamerThread.h"
#include "bindings/core/v8/V8ScriptRunner.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/PendingScript.h"
#include "core/fetch/CachedMetadata.h"
#include "core/frame/Settings.h"
#include "core/html/parser/TextResourceDecoder.h"
#include "core/loader/resource/ScriptResource.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/Histogram.h"
#include "platform/SharedBuffer.h"
#include "platform/tracing/TraceEvent.h"
#include "public/platform/WebScheduler.h"
#include "wtf/Deque.h"
#include "wtf/PtrUtil.h"
#include "wtf/text/TextEncodingRegistry.h"
#include <memory>

namespace blink {

namespace {

void recordStartedStreamingHistogram(ScriptStreamer::Type scriptType,
                                     int reason) {
  switch (scriptType) {
    case ScriptStreamer::ParsingBlocking: {
      DEFINE_STATIC_LOCAL(
          EnumerationHistogram, parseBlockingHistogram,
          ("WebCore.Scripts.ParsingBlocking.StartedStreaming", 2));
      parseBlockingHistogram.count(reason);
      break;
    }
    case ScriptStreamer::Deferred: {
      DEFINE_STATIC_LOCAL(EnumerationHistogram, deferredHistogram,
                          ("WebCore.Scripts.Deferred.StartedStreaming", 2));
      deferredHistogram.count(reason);
      break;
    }
    case ScriptStreamer::Async: {
      DEFINE_STATIC_LOCAL(EnumerationHistogram, asyncHistogram,
                          ("WebCore.Scripts.Async.StartedStreaming", 2));
      asyncHistogram.count(reason);
      break;
    }
    default:
      NOTREACHED();
      break;
  }
}

// For tracking why some scripts are not streamed. Not streaming is part of
// normal operation (e.g., script already loaded, script too small) and doesn't
// necessarily indicate a failure.
enum NotStreamingReason {
  AlreadyLoaded,
  NotHTTP,
  Reload,
  ContextNotValid,
  EncodingNotSupported,
  ThreadBusy,
  V8CannotStream,
  ScriptTooSmall,
  NotStreamingReasonEnd
};

void recordNotStreamingReasonHistogram(ScriptStreamer::Type scriptType,
                                       NotStreamingReason reason) {
  switch (scriptType) {
    case ScriptStreamer::ParsingBlocking: {
      DEFINE_STATIC_LOCAL(EnumerationHistogram, parseBlockingHistogram,
                          ("WebCore.Scripts.ParsingBlocking.NotStreamingReason",
                           NotStreamingReasonEnd));
      parseBlockingHistogram.count(reason);
      break;
    }
    case ScriptStreamer::Deferred: {
      DEFINE_STATIC_LOCAL(EnumerationHistogram, deferredHistogram,
                          ("WebCore.Scripts.Deferred.NotStreamingReason",
                           NotStreamingReasonEnd));
      deferredHistogram.count(reason);
      break;
    }
    case ScriptStreamer::Async: {
      DEFINE_STATIC_LOCAL(
          EnumerationHistogram, asyncHistogram,
          ("WebCore.Scripts.Async.NotStreamingReason", NotStreamingReasonEnd));
      asyncHistogram.count(reason);
      break;
    }
    default:
      NOTREACHED();
      break;
  }
}

}  // namespace

// For passing data between the main thread (producer) and the streamer thread
// (consumer). The main thread prepares the data (copies it from Resource) and
// the streamer thread feeds it to V8.
class SourceStreamDataQueue {
  WTF_MAKE_NONCOPYABLE(SourceStreamDataQueue);

 public:
  SourceStreamDataQueue() : m_finished(false) {}

  ~SourceStreamDataQueue() { discardQueuedData(); }

  void clear() {
    MutexLocker locker(m_mutex);
    m_finished = false;
    discardQueuedData();
  }

  void produce(const uint8_t* data, size_t length) {
    MutexLocker locker(m_mutex);
    DCHECK(!m_finished);
    m_data.append(std::make_pair(data, length));
    m_haveData.signal();
  }

  void finish() {
    MutexLocker locker(m_mutex);
    m_finished = true;
    m_haveData.signal();
  }

  void consume(const uint8_t** data, size_t* length) {
    MutexLocker locker(m_mutex);
    while (!tryGetData(data, length))
      m_haveData.wait(m_mutex);
  }

 private:
  bool tryGetData(const uint8_t** data, size_t* length) {
    ASSERT(m_mutex.locked());
    if (!m_data.isEmpty()) {
      std::pair<const uint8_t*, size_t> next_data = m_data.takeFirst();
      *data = next_data.first;
      *length = next_data.second;
      return true;
    }
    if (m_finished) {
      *length = 0;
      return true;
    }
    return false;
  }

  void discardQueuedData() {
    while (!m_data.isEmpty()) {
      std::pair<const uint8_t*, size_t> next_data = m_data.takeFirst();
      delete[] next_data.first;
    }
  }

  Deque<std::pair<const uint8_t*, size_t>> m_data;
  bool m_finished;
  Mutex m_mutex;
  ThreadCondition m_haveData;
};

// SourceStream implements the streaming interface towards V8. The main
// functionality is preparing the data to give to V8 on main thread, and
// actually giving the data (via GetMoreData which is called on a background
// thread).
class SourceStream : public v8::ScriptCompiler::ExternalSourceStream {
  WTF_MAKE_NONCOPYABLE(SourceStream);

 public:
  explicit SourceStream(WebTaskRunner* loadingTaskRunner)
      : v8::ScriptCompiler::ExternalSourceStream(),
        m_cancelled(false),
        m_finished(false),
        m_queueLeadPosition(0),
        m_queueTailPosition(0),
        m_loadingTaskRunner(loadingTaskRunner->clone()) {}

  virtual ~SourceStream() override {}

  // Called by V8 on a background thread. Should block until we can return
  // some data.
  size_t GetMoreData(const uint8_t** src) override {
    DCHECK(!isMainThread());
    {
      MutexLocker locker(m_mutex);
      if (m_cancelled)
        return 0;
    }
    size_t length = 0;
    // This will wait until there is data.
    m_dataQueue.consume(src, &length);
    {
      MutexLocker locker(m_mutex);
      if (m_cancelled)
        return 0;
    }
    m_queueLeadPosition += length;
    return length;
  }

  void didFinishLoading() {
    DCHECK(isMainThread());
    m_finished = true;
    m_dataQueue.finish();
  }

  void didReceiveData(ScriptStreamer* streamer) {
    DCHECK(isMainThread());
    prepareDataOnMainThread(streamer);
  }

  void cancel() {
    DCHECK(isMainThread());
    // The script is no longer needed by the upper layers. Stop streaming
    // it. The next time GetMoreData is called (or woken up), it will return
    // 0, which will be interpreted as EOS by V8 and the parsing will
    // fail. ScriptStreamer::streamingComplete will be called, and at that
    // point we will release the references to SourceStream.
    {
      MutexLocker locker(m_mutex);
      m_cancelled = true;
    }
    m_dataQueue.finish();
  }

 private:
  void prepareDataOnMainThread(ScriptStreamer* streamer) {
    DCHECK(isMainThread());

    // The Resource must still be alive; otherwise we should've cancelled
    // the streaming (if we have cancelled, the background thread is not
    // waiting).
    DCHECK(streamer->resource());

    if (!streamer->resource()->response().cacheStorageCacheName().isNull()) {
      streamer->suppressStreaming();
      cancel();
      return;
    }

    CachedMetadataHandler* cacheHandler = streamer->resource()->cacheHandler();
    RefPtr<CachedMetadata> codeCache(
        cacheHandler
            ? cacheHandler->cachedMetadata(
                  V8ScriptRunner::tagForCodeCache(cacheHandler))
            : nullptr);
    if (codeCache.get()) {
      // The resource has a code cache, so it's unnecessary to stream and
      // parse the code. Cancel the streaming and resume the non-streaming
      // code path.
      streamer->suppressStreaming();
      cancel();
      return;
    }

    if (!m_resourceBuffer) {
      // We don't have a buffer yet. Try to get it from the resource.
      m_resourceBuffer = streamer->resource()->resourceBuffer();
    }

    fetchDataFromResourceBuffer();
  }

  void fetchDataFromResourceBuffer() {
    DCHECK(isMainThread());
    MutexLocker locker(m_mutex);

    DCHECK(!m_finished);
    if (m_cancelled) {
      m_dataQueue.finish();
      return;
    }

    // Get as much data from the ResourceBuffer as we can.
    const char* data = nullptr;
    while (size_t length =
               m_resourceBuffer->getSomeData(data, m_queueTailPosition)) {
      // Copy the data chunks into a new buffer, since we're going to
      // give the data to a background thread.
      uint8_t* copiedData = new uint8_t[length];
      memcpy(copiedData, data, length);
      m_dataQueue.produce(copiedData, length);

      m_queueTailPosition += length;
    }
  }

  // For coordinating between the main thread and background thread tasks.
  // Guards m_cancelled and m_queueTailPosition.
  Mutex m_mutex;

  // The shared buffer containing the resource data + state variables.
  // Used by both threads, guarded by m_mutex.
  bool m_cancelled;
  bool m_finished;

  RefPtr<const SharedBuffer> m_resourceBuffer;  // Only used by the main thread.

  // The queue contains the data to be passed to the V8 thread.
  //   queueLeadPosition: data we have handed off to the V8 thread.
  //   queueTailPosition: end of data we have enqued in the queue.
  //   bookmarkPosition: position of the bookmark.
  SourceStreamDataQueue m_dataQueue;  // Thread safe.
  size_t m_queueLeadPosition;         // Only used by v8 thread.
  size_t m_queueTailPosition;  // Used by both threads; guarded by m_mutex.

  std::unique_ptr<WebTaskRunner> m_loadingTaskRunner;
};

size_t ScriptStreamer::s_smallScriptThreshold = 30 * 1024;

void ScriptStreamer::startStreaming(PendingScript* script,
                                    Type scriptType,
                                    Settings* settings,
                                    ScriptState* scriptState,
                                    WebTaskRunner* loadingTaskRunner) {
  // We don't yet know whether the script will really be streamed. E.g.,
  // suppressing streaming for short scripts is done later. Record only the
  // sure negative cases here.
  bool startedStreaming = startStreamingInternal(
      script, scriptType, settings, scriptState, loadingTaskRunner);
  if (!startedStreaming)
    recordStartedStreamingHistogram(scriptType, 0);
}

bool ScriptStreamer::convertEncoding(
    const char* encodingName,
    v8::ScriptCompiler::StreamedSource::Encoding* encoding) {
  // Here's a list of encodings we can use for streaming. These are
  // the canonical names.
  if (strcmp(encodingName, "windows-1252") == 0 ||
      strcmp(encodingName, "ISO-8859-1") == 0 ||
      strcmp(encodingName, "US-ASCII") == 0) {
    *encoding = v8::ScriptCompiler::StreamedSource::ONE_BYTE;
    return true;
  }
  if (strcmp(encodingName, "UTF-8") == 0) {
    *encoding = v8::ScriptCompiler::StreamedSource::UTF8;
    return true;
  }
  // We don't stream other encodings; especially we don't stream two
  // byte scripts to avoid the handling of endianness. Most scripts
  // are Latin1 or UTF-8 anyway, so this should be enough for most
  // real world purposes.
  return false;
}

bool ScriptStreamer::isFinished() const {
  MutexLocker locker(m_mutex);
  return m_loadingFinished && (m_parsingFinished || m_streamingSuppressed);
}

void ScriptStreamer::streamingCompleteOnBackgroundThread() {
  DCHECK(!isMainThread());
  {
    MutexLocker locker(m_mutex);
    m_parsingFinished = true;
  }

  // notifyFinished might already be called, or it might be called in the
  // future (if the parsing finishes earlier because of a parse error).
  m_loadingTaskRunner->postTask(
      BLINK_FROM_HERE, crossThreadBind(&ScriptStreamer::streamingComplete,
                                       wrapCrossThreadPersistent(this)));

  // The task might delete ScriptStreamer, so it's not safe to do anything
  // after posting it. Note that there's no way to guarantee that this
  // function has returned before the task is ran - however, we should not
  // access the "this" object after posting the task. (Especially, we should
  // not be holding the mutex at this point.)
}

void ScriptStreamer::cancel() {
  DCHECK(isMainThread());
  // The upper layer doesn't need the script any more, but streaming might
  // still be ongoing. Tell SourceStream to try to cancel it whenever it gets
  // the control the next time. It can also be that V8 has already completed
  // its operations and streamingComplete will be called soon.
  m_detached = true;
  m_resource = 0;
  if (m_stream)
    m_stream->cancel();
}

void ScriptStreamer::suppressStreaming() {
  MutexLocker locker(m_mutex);
  DCHECK(!m_loadingFinished);
  // It can be that the parsing task has already finished (e.g., if there was
  // a parse error).
  m_streamingSuppressed = true;
}

void ScriptStreamer::notifyAppendData(ScriptResource* resource) {
  DCHECK(isMainThread());
  CHECK_EQ(m_resource, resource);
  {
    MutexLocker locker(m_mutex);
    if (m_streamingSuppressed)
      return;
  }
  if (!m_haveEnoughDataForStreaming) {
    // Even if the first data chunk is small, the script can still be big
    // enough - wait until the next data chunk comes before deciding whether
    // to start the streaming.
    DCHECK(resource->resourceBuffer());
    if (resource->resourceBuffer()->size() < s_smallScriptThreshold)
      return;
    m_haveEnoughDataForStreaming = true;

    {
      // Check for BOM (byte order marks), because that might change our
      // understanding of the data encoding.
      constexpr size_t maximumLengthOfBOM = 4;
      char maybeBOM[maximumLengthOfBOM] = {};
      if (!resource->resourceBuffer()->getPartAsBytes(
              maybeBOM, static_cast<size_t>(0), maximumLengthOfBOM)) {
        NOTREACHED();
        return;
      }

      std::unique_ptr<TextResourceDecoder> decoder(TextResourceDecoder::create(
          "application/javascript", resource->encoding()));
      decoder->checkForBOM(maybeBOM, maximumLengthOfBOM);

      // The encoding may change when we see the BOM. Check for BOM now
      // and update the encoding from the decoder when necessary. Supress
      // streaming if the encoding is unsupported.
      //
      // Also note that have at least s_smallScriptThreshold worth of
      // data, which is more than enough for detecting a BOM.
      if (!convertEncoding(decoder->encoding().name(), &m_encoding)) {
        suppressStreaming();
        recordNotStreamingReasonHistogram(m_scriptType, EncodingNotSupported);
        recordStartedStreamingHistogram(m_scriptType, 0);
        return;
      }
    }

    if (ScriptStreamerThread::shared()->isRunningTask()) {
      // At the moment we only have one thread for running the tasks. A
      // new task shouldn't be queued before the running task completes,
      // because the running task can block and wait for data from the
      // network.
      suppressStreaming();
      recordNotStreamingReasonHistogram(m_scriptType, ThreadBusy);
      recordStartedStreamingHistogram(m_scriptType, 0);
      return;
    }

    if (!m_scriptState->contextIsValid()) {
      suppressStreaming();
      recordNotStreamingReasonHistogram(m_scriptType, ContextNotValid);
      recordStartedStreamingHistogram(m_scriptType, 0);
      return;
    }

    DCHECK(!m_stream);
    DCHECK(!m_source);
    m_stream = new SourceStream(m_loadingTaskRunner.get());
    // m_source takes ownership of m_stream.
    m_source = wrapUnique(
        new v8::ScriptCompiler::StreamedSource(m_stream, m_encoding));

    ScriptState::Scope scope(m_scriptState.get());
    std::unique_ptr<v8::ScriptCompiler::ScriptStreamingTask>
        scriptStreamingTask(wrapUnique(v8::ScriptCompiler::StartStreamingScript(
            m_scriptState->isolate(), m_source.get(), m_compileOptions)));
    if (!scriptStreamingTask) {
      // V8 cannot stream the script.
      suppressStreaming();
      m_stream = 0;
      m_source.reset();
      recordNotStreamingReasonHistogram(m_scriptType, V8CannotStream);
      recordStartedStreamingHistogram(m_scriptType, 0);
      return;
    }

    ScriptStreamerThread::shared()->postTask(
        crossThreadBind(&ScriptStreamerThread::runScriptStreamingTask,
                        passed(std::move(scriptStreamingTask)),
                        wrapCrossThreadPersistent(this)));
    recordStartedStreamingHistogram(m_scriptType, 1);
  }
  if (m_stream)
    m_stream->didReceiveData(this);
}

void ScriptStreamer::notifyFinished(Resource* resource) {
  DCHECK(isMainThread());
  CHECK_EQ(m_resource, resource);
  // A special case: empty and small scripts. We didn't receive enough data to
  // start the streaming before this notification. In that case, there won't
  // be a "parsing complete" notification either, and we should not wait for
  // it.
  if (!m_haveEnoughDataForStreaming) {
    recordNotStreamingReasonHistogram(m_scriptType, ScriptTooSmall);
    recordStartedStreamingHistogram(m_scriptType, 0);
    suppressStreaming();
  }
  if (m_stream)
    m_stream->didFinishLoading();
  m_loadingFinished = true;

  notifyFinishedToClient();
}

ScriptStreamer::ScriptStreamer(
    PendingScript* script,
    Type scriptType,
    ScriptState* scriptState,
    v8::ScriptCompiler::CompileOptions compileOptions,
    WebTaskRunner* loadingTaskRunner)
    : m_pendingScript(script),
      m_resource(script->resource()),
      m_detached(false),
      m_stream(0),
      m_loadingFinished(false),
      m_parsingFinished(false),
      m_haveEnoughDataForStreaming(false),
      m_streamingSuppressed(false),
      m_compileOptions(compileOptions),
      m_scriptState(scriptState),
      m_scriptType(scriptType),
      m_scriptURLString(m_resource->url().copy().getString()),
      m_scriptResourceIdentifier(m_resource->identifier()),
      // Unfortunately there's no dummy encoding value in the enum; let's use
      // one we don't stream.
      m_encoding(v8::ScriptCompiler::StreamedSource::TWO_BYTE),
      m_loadingTaskRunner(loadingTaskRunner->clone()) {}

ScriptStreamer::~ScriptStreamer() {}

DEFINE_TRACE(ScriptStreamer) {
  visitor->trace(m_pendingScript);
  visitor->trace(m_resource);
}

void ScriptStreamer::streamingComplete() {
  // The background task is completed; do the necessary ramp-down in the main
  // thread.
  DCHECK(isMainThread());

  // It's possible that the corresponding Resource was deleted before V8
  // finished streaming. In that case, the data or the notification is not
  // needed. In addition, if the streaming is suppressed, the non-streaming
  // code path will resume after the resource has loaded, before the
  // background task finishes.
  if (m_detached || m_streamingSuppressed)
    return;

  // We have now streamed the whole script to V8 and it has parsed the
  // script. We're ready for the next step: compiling and executing the
  // script.
  notifyFinishedToClient();
}

void ScriptStreamer::notifyFinishedToClient() {
  DCHECK(isMainThread());
  // Usually, the loading will be finished first, and V8 will still need some
  // time to catch up. But the other way is possible too: if V8 detects a
  // parse error, the V8 side can complete before loading has finished. Send
  // the notification after both loading and V8 side operations have
  // completed. Here we also check that we have a client: it can happen that a
  // function calling notifyFinishedToClient was already scheduled in the task
  // queue and the upper layer decided that it's not interested in the script
  // and called removeClient.
  if (!isFinished())
    return;

  m_pendingScript->streamingFinished();
}

bool ScriptStreamer::startStreamingInternal(PendingScript* script,
                                            Type scriptType,
                                            Settings* settings,
                                            ScriptState* scriptState,
                                            WebTaskRunner* loadingTaskRunner) {
  DCHECK(isMainThread());
  DCHECK(scriptState->contextIsValid());
  ScriptResource* resource = script->resource();
  if (resource->isLoaded()) {
    recordNotStreamingReasonHistogram(scriptType, AlreadyLoaded);
    return false;
  }
  if (!resource->url().protocolIsInHTTPFamily()) {
    recordNotStreamingReasonHistogram(scriptType, NotHTTP);
    return false;
  }
  if (resource->isCacheValidator()) {
    recordNotStreamingReasonHistogram(scriptType, Reload);
    // This happens e.g., during reloads. We're actually not going to load
    // the current Resource of the PendingScript but switch to another
    // Resource -> don't stream.
    return false;
  }
  // We cannot filter out short scripts, even if we wait for the HTTP headers
  // to arrive: the Content-Length HTTP header is not sent for chunked
  // downloads.

  // Decide what kind of cached data we should produce while streaming. Only
  // produce parser cache if the non-streaming compile takes advantage of it.
  v8::ScriptCompiler::CompileOptions compileOption =
      v8::ScriptCompiler::kNoCompileOptions;
  if (settings->v8CacheOptions() == V8CacheOptionsParse)
    compileOption = v8::ScriptCompiler::kProduceParserCache;

  // The Resource might go out of scope if the script is no longer
  // needed. This makes PendingScript notify the ScriptStreamer when it is
  // destroyed.
  script->setStreamer(ScriptStreamer::create(script, scriptType, scriptState,
                                             compileOption, loadingTaskRunner));

  return true;
}

}  // namespace blink

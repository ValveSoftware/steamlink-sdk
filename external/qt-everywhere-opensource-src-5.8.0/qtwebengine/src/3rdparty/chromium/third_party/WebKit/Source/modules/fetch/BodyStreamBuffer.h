// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BodyStreamBuffer_h
#define BodyStreamBuffer_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptValue.h"
#include "core/dom/DOMException.h"
#include "core/streams/ReadableByteStream.h"
#include "core/streams/ReadableByteStreamReader.h"
#include "core/streams/UnderlyingSource.h"
#include "core/streams/UnderlyingSourceBase.h"
#include "modules/ModulesExport.h"
#include "modules/fetch/FetchDataConsumerHandle.h"
#include "modules/fetch/FetchDataLoader.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebDataConsumerHandle.h"
#include <memory>

namespace blink {

class EncodedFormData;
class ScriptState;

class MODULES_EXPORT BodyStreamBuffer final : public UnderlyingSourceBase, public UnderlyingSource, public WebDataConsumerHandle::Client {
    WTF_MAKE_NONCOPYABLE(BodyStreamBuffer);
    USING_GARBAGE_COLLECTED_MIXIN(BodyStreamBuffer);
public:
    // Needed because we have to release |m_reader| promptly.
    EAGERLY_FINALIZE();
    // |handle| cannot be null and cannot be locked.
    // This function must be called with entering an appropriate V8 context.
    BodyStreamBuffer(ScriptState*, std::unique_ptr<FetchDataConsumerHandle> /* handle */);
    // |ReadableStreamOperations::isReadableStream(stream)| must hold.
    // This function must be called with entering an appropriate V8 context.
    BodyStreamBuffer(ScriptState*, ScriptValue stream);

    ScriptValue stream();

    // Callable only when neither locked nor disturbed.
    PassRefPtr<BlobDataHandle> drainAsBlobDataHandle(FetchDataConsumerHandle::Reader::BlobSizePolicy);
    PassRefPtr<EncodedFormData> drainAsFormData();
    void startLoading(FetchDataLoader*, FetchDataLoader::Client* /* client */);
    void tee(BodyStreamBuffer**, BodyStreamBuffer**);

    // UnderlyingSource
    void pullSource() override;
    ScriptPromise cancelSource(ScriptState*, ScriptValue reason) override;

    // UnderlyingSourceBase
    ScriptPromise pull(ScriptState*) override;
    ScriptPromise cancel(ScriptState*, ScriptValue reason) override;
    bool hasPendingActivity() const override;
    void stop() override;

    // WebDataConsumerHandle::Client
    void didGetReadable() override;

    bool isStreamReadable();
    bool isStreamClosed();
    bool isStreamErrored();
    bool isStreamLocked();
    bool isStreamDisturbed();
    void closeAndLockAndDisturb();
    ScriptState* scriptState() { return m_scriptState.get(); }

    DEFINE_INLINE_TRACE()
    {
        visitor->trace(m_stream);
        visitor->trace(m_loader);
        UnderlyingSourceBase::trace(visitor);
        UnderlyingSource::trace(visitor);
    }

private:
    class LoaderClient;

    void close();
    void error();
    void processData();
    void endLoading();
    void stopLoading();
    std::unique_ptr<FetchDataConsumerHandle> releaseHandle();

    RefPtr<ScriptState> m_scriptState;
    std::unique_ptr<FetchDataConsumerHandle> m_handle;
    std::unique_ptr<FetchDataConsumerHandle::Reader> m_reader;
    Member<ReadableByteStream> m_stream;
    // We need this member to keep it alive while loading.
    Member<FetchDataLoader> m_loader;
    bool m_streamNeedsMore = false;
    bool m_madeFromReadableStream;
};

} // namespace blink

#endif // BodyStreamBuffer_h

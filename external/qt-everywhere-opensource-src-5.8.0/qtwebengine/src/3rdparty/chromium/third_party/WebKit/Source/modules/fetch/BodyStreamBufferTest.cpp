// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/fetch/BodyStreamBuffer.h"

#include "bindings/core/v8/V8BindingForTesting.h"
#include "core/html/FormData.h"
#include "modules/fetch/DataConsumerHandleTestUtil.h"
#include "modules/fetch/FetchBlobDataConsumerHandle.h"
#include "modules/fetch/FetchFormDataConsumerHandle.h"
#include "platform/blob/BlobData.h"
#include "platform/blob/BlobURL.h"
#include "platform/network/EncodedFormData.h"
#include "platform/testing/UnitTestHelpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

namespace {

using ::testing::InSequence;
using ::testing::_;
using ::testing::SaveArg;
using Checkpoint = ::testing::StrictMock<::testing::MockFunction<void(int)>>;
using Command = DataConsumerHandleTestUtil::Command;
using ReplayingHandle = DataConsumerHandleTestUtil::ReplayingHandle;
using MockFetchDataLoaderClient = DataConsumerHandleTestUtil::MockFetchDataLoaderClient;

class FakeLoaderFactory : public FetchBlobDataConsumerHandle::LoaderFactory {
public:
    std::unique_ptr<ThreadableLoader> create(ExecutionContext&, ThreadableLoaderClient*, const ThreadableLoaderOptions&, const ResourceLoaderOptions&) override
    {
        ASSERT_NOT_REACHED();
        return nullptr;
    }
};

class BodyStreamBufferTest : public ::testing::Test {
protected:
    ScriptValue eval(ScriptState* scriptState, const char* s)
    {
        v8::Local<v8::String> source;
        v8::Local<v8::Script> script;
        v8::MicrotasksScope microtasks(scriptState->isolate(), v8::MicrotasksScope::kDoNotRunMicrotasks);
        if (!v8Call(v8::String::NewFromUtf8(scriptState->isolate(), s, v8::NewStringType::kNormal), source)) {
            ADD_FAILURE();
            return ScriptValue();
        }
        if (!v8Call(v8::Script::Compile(scriptState->context(), source), script)) {
            ADD_FAILURE() << "Compilation fails";
            return ScriptValue();
        }
        return ScriptValue(scriptState, script->Run(scriptState->context()));
    }
    ScriptValue evalWithPrintingError(ScriptState* scriptState, const char* s)
    {
        v8::TryCatch block(scriptState->isolate());
        ScriptValue r = eval(scriptState, s);
        if (block.HasCaught()) {
            ADD_FAILURE() << toCoreString(block.Exception()->ToString(scriptState->isolate())).utf8().data();
            block.ReThrow();
        }
        return r;
    }
};

TEST_F(BodyStreamBufferTest, Tee)
{
    V8TestingScope scope;
    Checkpoint checkpoint;
    MockFetchDataLoaderClient* client1 = MockFetchDataLoaderClient::create();
    MockFetchDataLoaderClient* client2 = MockFetchDataLoaderClient::create();

    InSequence s;
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*client1, didFetchDataLoadedString(String("hello, world")));
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(checkpoint, Call(3));
    EXPECT_CALL(*client2, didFetchDataLoadedString(String("hello, world")));
    EXPECT_CALL(checkpoint, Call(4));

    std::unique_ptr<DataConsumerHandleTestUtil::ReplayingHandle> handle = DataConsumerHandleTestUtil::ReplayingHandle::create();
    handle->add(DataConsumerHandleTestUtil::Command(DataConsumerHandleTestUtil::Command::Data, "hello, "));
    handle->add(DataConsumerHandleTestUtil::Command(DataConsumerHandleTestUtil::Command::Data, "world"));
    handle->add(DataConsumerHandleTestUtil::Command(DataConsumerHandleTestUtil::Command::Done));
    BodyStreamBuffer* buffer = new BodyStreamBuffer(scope.getScriptState(), createFetchDataConsumerHandleFromWebHandle(std::move(handle)));

    BodyStreamBuffer* new1;
    BodyStreamBuffer* new2;
    buffer->tee(&new1, &new2);

    EXPECT_TRUE(buffer->isStreamLocked());
    EXPECT_TRUE(buffer->isStreamDisturbed());
    EXPECT_FALSE(buffer->hasPendingActivity());

    new1->startLoading(FetchDataLoader::createLoaderAsString(), client1);
    checkpoint.Call(1);
    testing::runPendingTasks();
    checkpoint.Call(2);

    new2->startLoading(FetchDataLoader::createLoaderAsString(), client2);
    checkpoint.Call(3);
    testing::runPendingTasks();
    checkpoint.Call(4);
}

TEST_F(BodyStreamBufferTest, TeeFromHandleMadeFromStream)
{
    V8TestingScope scope;
    ScriptValue stream = evalWithPrintingError(
        scope.getScriptState(),
        "stream = new ReadableStream({start: c => controller = c});"
        "controller.enqueue(new Uint8Array([0x41, 0x42]));"
        "controller.enqueue(new Uint8Array([0x55, 0x58]));"
        "controller.close();"
        "stream");
    Checkpoint checkpoint;
    MockFetchDataLoaderClient* client1 = MockFetchDataLoaderClient::create();
    MockFetchDataLoaderClient* client2 = MockFetchDataLoaderClient::create();

    InSequence s;
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*client1, didFetchDataLoadedString(String("ABUX")));
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(checkpoint, Call(3));
    EXPECT_CALL(*client2, didFetchDataLoadedString(String("ABUX")));
    EXPECT_CALL(checkpoint, Call(4));

    BodyStreamBuffer* buffer = new BodyStreamBuffer(scope.getScriptState(), stream);

    BodyStreamBuffer* new1;
    BodyStreamBuffer* new2;
    buffer->tee(&new1, &new2);

    EXPECT_TRUE(buffer->isStreamLocked());
    // Note that this behavior is slightly different from for the behavior of
    // a BodyStreamBuffer made from a FetchDataConsumerHandle. See the above
    // test. In this test, the stream will get disturbed when the microtask
    // is performed.
    // TODO(yhirano): A uniformed behavior is preferred.
    EXPECT_FALSE(buffer->isStreamDisturbed());
    EXPECT_FALSE(buffer->hasPendingActivity());

    v8::MicrotasksScope::PerformCheckpoint(scope.getScriptState()->isolate());

    EXPECT_TRUE(buffer->isStreamLocked());
    EXPECT_TRUE(buffer->isStreamDisturbed());
    EXPECT_FALSE(buffer->hasPendingActivity());

    new1->startLoading(FetchDataLoader::createLoaderAsString(), client1);
    checkpoint.Call(1);
    testing::runPendingTasks();
    checkpoint.Call(2);

    new2->startLoading(FetchDataLoader::createLoaderAsString(), client2);
    checkpoint.Call(3);
    testing::runPendingTasks();
    checkpoint.Call(4);
}

TEST_F(BodyStreamBufferTest, DrainAsBlobDataHandle)
{
    V8TestingScope scope;
    std::unique_ptr<BlobData> data = BlobData::create();
    data->appendText("hello", false);
    auto size = data->length();
    RefPtr<BlobDataHandle> blobDataHandle = BlobDataHandle::create(std::move(data), size);
    BodyStreamBuffer* buffer = new BodyStreamBuffer(scope.getScriptState(), FetchBlobDataConsumerHandle::create(scope.getExecutionContext(), blobDataHandle, new FakeLoaderFactory));

    EXPECT_FALSE(buffer->isStreamLocked());
    EXPECT_FALSE(buffer->isStreamDisturbed());
    EXPECT_FALSE(buffer->hasPendingActivity());
    RefPtr<BlobDataHandle> outputBlobDataHandle = buffer->drainAsBlobDataHandle(FetchDataConsumerHandle::Reader::AllowBlobWithInvalidSize);

    EXPECT_TRUE(buffer->isStreamLocked());
    EXPECT_TRUE(buffer->isStreamDisturbed());
    EXPECT_FALSE(buffer->hasPendingActivity());
    EXPECT_EQ(blobDataHandle, outputBlobDataHandle);
}

TEST_F(BodyStreamBufferTest, DrainAsBlobDataHandleReturnsNull)
{
    V8TestingScope scope;
    // This handle is not drainable.
    std::unique_ptr<FetchDataConsumerHandle> handle = createFetchDataConsumerHandleFromWebHandle(createWaitingDataConsumerHandle());
    BodyStreamBuffer* buffer = new BodyStreamBuffer(scope.getScriptState(), std::move(handle));

    EXPECT_FALSE(buffer->isStreamLocked());
    EXPECT_FALSE(buffer->isStreamDisturbed());
    EXPECT_FALSE(buffer->hasPendingActivity());

    EXPECT_FALSE(buffer->drainAsBlobDataHandle(FetchDataConsumerHandle::Reader::AllowBlobWithInvalidSize));

    EXPECT_FALSE(buffer->isStreamLocked());
    EXPECT_FALSE(buffer->isStreamDisturbed());
    EXPECT_FALSE(buffer->hasPendingActivity());
}

TEST_F(BodyStreamBufferTest, DrainAsBlobFromBufferMadeFromBufferMadeFromStream)
{
    V8TestingScope scope;
    ScriptValue stream = evalWithPrintingError(scope.getScriptState(), "new ReadableStream()");
    BodyStreamBuffer* buffer = new BodyStreamBuffer(scope.getScriptState(), stream);

    EXPECT_FALSE(buffer->hasPendingActivity());
    EXPECT_FALSE(buffer->isStreamLocked());
    EXPECT_FALSE(buffer->isStreamDisturbed());
    EXPECT_TRUE(buffer->isStreamReadable());

    EXPECT_FALSE(buffer->drainAsBlobDataHandle(FetchDataConsumerHandle::Reader::AllowBlobWithInvalidSize));

    EXPECT_FALSE(buffer->hasPendingActivity());
    EXPECT_FALSE(buffer->isStreamLocked());
    EXPECT_FALSE(buffer->isStreamDisturbed());
    EXPECT_TRUE(buffer->isStreamReadable());
}

TEST_F(BodyStreamBufferTest, DrainAsFormData)
{
    V8TestingScope scope;
    FormData* data = FormData::create(UTF8Encoding());
    data->append("name1", "value1");
    data->append("name2", "value2");
    RefPtr<EncodedFormData> inputFormData = data->encodeMultiPartFormData();

    BodyStreamBuffer* buffer = new BodyStreamBuffer(scope.getScriptState(), FetchFormDataConsumerHandle::create(scope.getExecutionContext(), inputFormData));

    EXPECT_FALSE(buffer->isStreamLocked());
    EXPECT_FALSE(buffer->isStreamDisturbed());
    EXPECT_FALSE(buffer->hasPendingActivity());
    RefPtr<EncodedFormData> outputFormData = buffer->drainAsFormData();

    EXPECT_TRUE(buffer->isStreamLocked());
    EXPECT_TRUE(buffer->isStreamDisturbed());
    EXPECT_FALSE(buffer->hasPendingActivity());
    EXPECT_EQ(outputFormData->flattenToString(), inputFormData->flattenToString());
}

TEST_F(BodyStreamBufferTest, DrainAsFormDataReturnsNull)
{
    V8TestingScope scope;
    // This handle is not drainable.
    std::unique_ptr<FetchDataConsumerHandle> handle = createFetchDataConsumerHandleFromWebHandle(createWaitingDataConsumerHandle());
    BodyStreamBuffer* buffer = new BodyStreamBuffer(scope.getScriptState(), std::move(handle));

    EXPECT_FALSE(buffer->isStreamLocked());
    EXPECT_FALSE(buffer->isStreamDisturbed());
    EXPECT_FALSE(buffer->hasPendingActivity());

    EXPECT_FALSE(buffer->drainAsFormData());

    EXPECT_FALSE(buffer->isStreamLocked());
    EXPECT_FALSE(buffer->isStreamDisturbed());
    EXPECT_FALSE(buffer->hasPendingActivity());
}

TEST_F(BodyStreamBufferTest, DrainAsFormDataFromBufferMadeFromBufferMadeFromStream)
{
    V8TestingScope scope;
    ScriptValue stream = evalWithPrintingError(scope.getScriptState(), "new ReadableStream()");
    BodyStreamBuffer* buffer = new BodyStreamBuffer(scope.getScriptState(), stream);

    EXPECT_FALSE(buffer->hasPendingActivity());
    EXPECT_FALSE(buffer->isStreamLocked());
    EXPECT_FALSE(buffer->isStreamDisturbed());
    EXPECT_TRUE(buffer->isStreamReadable());

    EXPECT_FALSE(buffer->drainAsFormData());

    EXPECT_FALSE(buffer->hasPendingActivity());
    EXPECT_FALSE(buffer->isStreamLocked());
    EXPECT_FALSE(buffer->isStreamDisturbed());
    EXPECT_TRUE(buffer->isStreamReadable());
}

TEST_F(BodyStreamBufferTest, LoadBodyStreamBufferAsArrayBuffer)
{
    V8TestingScope scope;
    Checkpoint checkpoint;
    MockFetchDataLoaderClient* client = MockFetchDataLoaderClient::create();
    DOMArrayBuffer* arrayBuffer = nullptr;

    InSequence s;
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*client, didFetchDataLoadedArrayBufferMock(_)).WillOnce(SaveArg<0>(&arrayBuffer));
    EXPECT_CALL(checkpoint, Call(2));

    std::unique_ptr<ReplayingHandle> handle = ReplayingHandle::create();
    handle->add(Command(Command::Data, "hello"));
    handle->add(Command(Command::Done));
    BodyStreamBuffer* buffer = new BodyStreamBuffer(scope.getScriptState(), createFetchDataConsumerHandleFromWebHandle(std::move(handle)));
    buffer->startLoading(FetchDataLoader::createLoaderAsArrayBuffer(), client);

    EXPECT_TRUE(buffer->isStreamLocked());
    EXPECT_TRUE(buffer->isStreamDisturbed());
    EXPECT_TRUE(buffer->hasPendingActivity());

    checkpoint.Call(1);
    testing::runPendingTasks();
    checkpoint.Call(2);

    EXPECT_TRUE(buffer->isStreamLocked());
    EXPECT_TRUE(buffer->isStreamDisturbed());
    EXPECT_FALSE(buffer->hasPendingActivity());
    ASSERT_TRUE(arrayBuffer);
    EXPECT_EQ("hello", String(static_cast<const char*>(arrayBuffer->data()), arrayBuffer->byteLength()));
}

TEST_F(BodyStreamBufferTest, LoadBodyStreamBufferAsBlob)
{
    V8TestingScope scope;
    Checkpoint checkpoint;
    MockFetchDataLoaderClient* client = MockFetchDataLoaderClient::create();
    RefPtr<BlobDataHandle> blobDataHandle;

    InSequence s;
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*client, didFetchDataLoadedBlobHandleMock(_)).WillOnce(SaveArg<0>(&blobDataHandle));
    EXPECT_CALL(checkpoint, Call(2));

    std::unique_ptr<ReplayingHandle> handle = ReplayingHandle::create();
    handle->add(Command(Command::Data, "hello"));
    handle->add(Command(Command::Done));
    BodyStreamBuffer* buffer = new BodyStreamBuffer(scope.getScriptState(), createFetchDataConsumerHandleFromWebHandle(std::move(handle)));
    buffer->startLoading(FetchDataLoader::createLoaderAsBlobHandle("text/plain"), client);

    EXPECT_TRUE(buffer->isStreamLocked());
    EXPECT_TRUE(buffer->isStreamDisturbed());
    EXPECT_TRUE(buffer->hasPendingActivity());

    checkpoint.Call(1);
    testing::runPendingTasks();
    checkpoint.Call(2);

    EXPECT_TRUE(buffer->isStreamLocked());
    EXPECT_TRUE(buffer->isStreamDisturbed());
    EXPECT_FALSE(buffer->hasPendingActivity());
    EXPECT_EQ(5u, blobDataHandle->size());
}

TEST_F(BodyStreamBufferTest, LoadBodyStreamBufferAsString)
{
    V8TestingScope scope;
    Checkpoint checkpoint;
    MockFetchDataLoaderClient* client = MockFetchDataLoaderClient::create();

    InSequence s;
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*client, didFetchDataLoadedString(String("hello")));
    EXPECT_CALL(checkpoint, Call(2));

    std::unique_ptr<ReplayingHandle> handle = ReplayingHandle::create();
    handle->add(Command(Command::Data, "hello"));
    handle->add(Command(Command::Done));
    BodyStreamBuffer* buffer = new BodyStreamBuffer(scope.getScriptState(), createFetchDataConsumerHandleFromWebHandle(std::move(handle)));
    buffer->startLoading(FetchDataLoader::createLoaderAsString(), client);

    EXPECT_TRUE(buffer->isStreamLocked());
    EXPECT_TRUE(buffer->isStreamDisturbed());
    EXPECT_TRUE(buffer->hasPendingActivity());

    checkpoint.Call(1);
    testing::runPendingTasks();
    checkpoint.Call(2);

    EXPECT_TRUE(buffer->isStreamLocked());
    EXPECT_TRUE(buffer->isStreamDisturbed());
    EXPECT_FALSE(buffer->hasPendingActivity());
}

TEST_F(BodyStreamBufferTest, LoadClosedHandle)
{
    V8TestingScope scope;
    Checkpoint checkpoint;
    MockFetchDataLoaderClient* client = MockFetchDataLoaderClient::create();

    InSequence s;
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*client, didFetchDataLoadedString(String("")));
    EXPECT_CALL(checkpoint, Call(2));

    BodyStreamBuffer* buffer = new BodyStreamBuffer(scope.getScriptState(), createFetchDataConsumerHandleFromWebHandle(createDoneDataConsumerHandle()));

    EXPECT_TRUE(buffer->isStreamReadable());
    testing::runPendingTasks();
    EXPECT_TRUE(buffer->isStreamClosed());

    EXPECT_FALSE(buffer->isStreamLocked());
    EXPECT_FALSE(buffer->isStreamDisturbed());
    EXPECT_FALSE(buffer->hasPendingActivity());

    buffer->startLoading(FetchDataLoader::createLoaderAsString(), client);
    EXPECT_TRUE(buffer->isStreamLocked());
    EXPECT_TRUE(buffer->isStreamDisturbed());
    EXPECT_TRUE(buffer->hasPendingActivity());

    checkpoint.Call(1);
    testing::runPendingTasks();
    checkpoint.Call(2);

    EXPECT_TRUE(buffer->isStreamLocked());
    EXPECT_TRUE(buffer->isStreamDisturbed());
    EXPECT_FALSE(buffer->hasPendingActivity());
}

TEST_F(BodyStreamBufferTest, LoadErroredHandle)
{
    V8TestingScope scope;
    Checkpoint checkpoint;
    MockFetchDataLoaderClient* client = MockFetchDataLoaderClient::create();

    InSequence s;
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*client, didFetchDataLoadFailed());
    EXPECT_CALL(checkpoint, Call(2));

    BodyStreamBuffer* buffer = new BodyStreamBuffer(scope.getScriptState(), createFetchDataConsumerHandleFromWebHandle(createUnexpectedErrorDataConsumerHandle()));

    EXPECT_TRUE(buffer->isStreamReadable());
    testing::runPendingTasks();
    EXPECT_TRUE(buffer->isStreamErrored());

    EXPECT_FALSE(buffer->isStreamLocked());
    EXPECT_FALSE(buffer->isStreamDisturbed());
    EXPECT_FALSE(buffer->hasPendingActivity());
    buffer->startLoading(FetchDataLoader::createLoaderAsString(), client);
    EXPECT_TRUE(buffer->isStreamLocked());
    EXPECT_TRUE(buffer->isStreamDisturbed());
    EXPECT_TRUE(buffer->hasPendingActivity());

    checkpoint.Call(1);
    testing::runPendingTasks();
    checkpoint.Call(2);

    EXPECT_TRUE(buffer->isStreamLocked());
    EXPECT_TRUE(buffer->isStreamDisturbed());
    EXPECT_FALSE(buffer->hasPendingActivity());
}

TEST_F(BodyStreamBufferTest, LoaderShouldBeKeptAliveByBodyStreamBuffer)
{
    V8TestingScope scope;
    Checkpoint checkpoint;
    MockFetchDataLoaderClient* client = MockFetchDataLoaderClient::create();

    InSequence s;
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*client, didFetchDataLoadedString(String("hello")));
    EXPECT_CALL(checkpoint, Call(2));

    std::unique_ptr<ReplayingHandle> handle = ReplayingHandle::create();
    handle->add(Command(Command::Data, "hello"));
    handle->add(Command(Command::Done));
    Persistent<BodyStreamBuffer> buffer = new BodyStreamBuffer(scope.getScriptState(), createFetchDataConsumerHandleFromWebHandle(std::move(handle)));
    buffer->startLoading(FetchDataLoader::createLoaderAsString(), client);

    ThreadHeap::collectAllGarbage();
    checkpoint.Call(1);
    testing::runPendingTasks();
    checkpoint.Call(2);
}

// TODO(hiroshige): Merge this class into MockFetchDataConsumerHandle.
class MockFetchDataConsumerHandleWithMockDestructor : public DataConsumerHandleTestUtil::MockFetchDataConsumerHandle {
public:
    static std::unique_ptr<::testing::StrictMock<MockFetchDataConsumerHandleWithMockDestructor>> create() { return wrapUnique(new ::testing::StrictMock<MockFetchDataConsumerHandleWithMockDestructor>); }

    ~MockFetchDataConsumerHandleWithMockDestructor() override
    {
        destruct();
    }

    MOCK_METHOD0(destruct, void());
};

TEST_F(BodyStreamBufferTest, SourceHandleAndReaderShouldBeDestructedWhenCanceled)
{
    V8TestingScope scope;
    using MockHandle = MockFetchDataConsumerHandleWithMockDestructor;
    using MockReader = DataConsumerHandleTestUtil::MockFetchDataConsumerReader;
    std::unique_ptr<MockHandle> handle = MockHandle::create();
    std::unique_ptr<MockReader> reader = MockReader::create();

    Checkpoint checkpoint;
    InSequence s;

    EXPECT_CALL(*handle, obtainReaderInternal(_)).WillOnce(::testing::Return(reader.get()));
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*reader, destruct());
    EXPECT_CALL(*handle, destruct());
    EXPECT_CALL(checkpoint, Call(2));

    // |reader| is adopted by |obtainReader|.
    ASSERT_TRUE(reader.release());

    BodyStreamBuffer* buffer = new BodyStreamBuffer(scope.getScriptState(), std::move(handle));
    checkpoint.Call(1);
    ScriptValue reason(scope.getScriptState(), v8String(scope.getScriptState()->isolate(), "reason"));
    buffer->cancelSource(scope.getScriptState(), reason);
    checkpoint.Call(2);
}

} // namespace

} // namespace blink

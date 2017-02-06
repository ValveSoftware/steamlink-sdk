// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/fetch/FetchBlobDataConsumerHandle.h"

#include "core/dom/ExecutionContext.h"
#include "core/fetch/ResourceLoaderOptions.h"
#include "core/loader/MockThreadableLoader.h"
#include "core/loader/ThreadableLoaderClient.h"
#include "core/testing/DummyPageHolder.h"
#include "modules/fetch/DataConsumerHandleTestUtil.h"
#include "platform/blob/BlobData.h"
#include "platform/blob/BlobURL.h"
#include "platform/network/ResourceError.h"
#include "platform/network/ResourceRequest.h"
#include "platform/network/ResourceResponse.h"
#include "platform/testing/UnitTestHelpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/PassRefPtr.h"
#include "wtf/PtrUtil.h"
#include "wtf/RefPtr.h"
#include <memory>
#include <string.h>

namespace blink {
namespace {

using Result = WebDataConsumerHandle::Result;
const Result kShouldWait = WebDataConsumerHandle::ShouldWait;
const Result kUnexpectedError = WebDataConsumerHandle::UnexpectedError;
const Result kDone = WebDataConsumerHandle::Done;
using Flags = WebDataConsumerHandle::Flags;
const Flags kNone = WebDataConsumerHandle::FlagNone;
using Thread = DataConsumerHandleTestUtil::Thread;
using HandleReader = DataConsumerHandleTestUtil::HandleReader;
using HandleTwoPhaseReader = DataConsumerHandleTestUtil::HandleTwoPhaseReader;
using HandleReadResult = DataConsumerHandleTestUtil::HandleReadResult;
using ReplayingHandle = DataConsumerHandleTestUtil::ReplayingHandle;
using Command = DataConsumerHandleTestUtil::Command;
template <typename T>
using HandleReaderRunner = DataConsumerHandleTestUtil::HandleReaderRunner<T>;

using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Ref;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrictMock;
using Checkpoint = StrictMock<::testing::MockFunction<void(int)>>;

class MockLoaderFactory : public FetchBlobDataConsumerHandle::LoaderFactory {
public:
    std::unique_ptr<ThreadableLoader> create(ExecutionContext& executionContext, ThreadableLoaderClient* client, const ThreadableLoaderOptions& threadableLoaderOptions, const ResourceLoaderOptions& resourceLoaderOptions) override
    {
        return wrapUnique(createInternal(executionContext, client, threadableLoaderOptions, resourceLoaderOptions));
    }

    MOCK_METHOD4(createInternal, ThreadableLoader*(ExecutionContext&, ThreadableLoaderClient*, const ThreadableLoaderOptions&, const ResourceLoaderOptions&));
};

PassRefPtr<BlobDataHandle> createBlobDataHandle(const char* s)
{
    std::unique_ptr<BlobData> data = BlobData::create();
    data->appendText(s, false);
    auto size = data->length();
    return BlobDataHandle::create(std::move(data), size);
}

String toString(const Vector<char>& data)
{
    return String(data.data(), data.size());
}

class FetchBlobDataConsumerHandleTest : public ::testing::Test {
public:
    FetchBlobDataConsumerHandleTest()
        : m_dummyPageHolder(DummyPageHolder::create(IntSize(1, 1))) {}
    ~FetchBlobDataConsumerHandleTest() override
    {
        m_dummyPageHolder = nullptr;
        // We need this to collect garbage-collected mocks.
        ThreadHeap::collectAllGarbage();
    }

    Document& document() { return m_dummyPageHolder->document(); }

private:
    std::unique_ptr<DummyPageHolder> m_dummyPageHolder;
};

TEST_F(FetchBlobDataConsumerHandleTest, CreateLoader)
{
    auto factory = new StrictMock<MockLoaderFactory>;
    Checkpoint checkpoint;

    ResourceRequest request;
    ThreadableLoaderOptions options;
    ResourceLoaderOptions resourceLoaderOptions;

    std::unique_ptr<MockThreadableLoader> loader = MockThreadableLoader::create();
    MockThreadableLoader* loaderPtr = loader.get();

    InSequence s;
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*factory, createInternal(Ref(document()), _, _, _)).WillOnce(DoAll(
        SaveArg<2>(&options),
        SaveArg<3>(&resourceLoaderOptions),
        Return(loader.release())));
    EXPECT_CALL(*loaderPtr, start(_)).WillOnce(SaveArg<0>(&request));
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(*loaderPtr, cancel());

    RefPtr<BlobDataHandle> blobDataHandle = createBlobDataHandle("Once upon a time");
    std::unique_ptr<WebDataConsumerHandle> handle
        = FetchBlobDataConsumerHandle::create(&document(), blobDataHandle, factory);
    testing::runPendingTasks();

    size_t size = 0;
    handle->obtainReader(nullptr)->read(nullptr, 0, kNone, &size);
    checkpoint.Call(1);
    testing::runPendingTasks();
    checkpoint.Call(2);

    EXPECT_TRUE(request.url().getString().startsWith("blob:"));
    EXPECT_TRUE(request.useStreamOnResponse());

    EXPECT_EQ(ConsiderPreflight, options.preflightPolicy);
    EXPECT_EQ(DenyCrossOriginRequests, options.crossOriginRequestPolicy);
    EXPECT_EQ(DoNotEnforceContentSecurityPolicy, options.contentSecurityPolicyEnforcement);

    EXPECT_EQ(DoNotBufferData, resourceLoaderOptions.dataBufferingPolicy);
    EXPECT_EQ(DoNotAllowStoredCredentials, resourceLoaderOptions.allowCredentials);
    EXPECT_EQ(ClientDidNotRequestCredentials, resourceLoaderOptions.credentialsRequested);
    EXPECT_EQ(CheckContentSecurityPolicy, resourceLoaderOptions.contentSecurityPolicyOption);
    EXPECT_EQ(DocumentContext, resourceLoaderOptions.requestInitiatorContext);
    EXPECT_EQ(RequestAsynchronously, resourceLoaderOptions.synchronousPolicy);
    EXPECT_EQ(NotCORSEnabled, resourceLoaderOptions.corsEnabled);
}

TEST_F(FetchBlobDataConsumerHandleTest, CancelLoaderWhenStopped)
{
    auto factory = new StrictMock<MockLoaderFactory>;
    Checkpoint checkpoint;

    std::unique_ptr<MockThreadableLoader> loader = MockThreadableLoader::create();
    MockThreadableLoader* loaderPtr = loader.get();

    InSequence s;
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*factory, createInternal(Ref(document()), _, _, _)).WillOnce(Return(loader.release()));
    EXPECT_CALL(*loaderPtr, start(_));
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(*loaderPtr, cancel());
    EXPECT_CALL(checkpoint, Call(3));

    RefPtr<BlobDataHandle> blobDataHandle = createBlobDataHandle("Once upon a time");
    std::unique_ptr<WebDataConsumerHandle> handle
        = FetchBlobDataConsumerHandle::create(&document(), blobDataHandle, factory);
    testing::runPendingTasks();

    size_t size = 0;
    handle->obtainReader(nullptr)->read(nullptr, 0, kNone, &size);
    checkpoint.Call(1);
    testing::runPendingTasks();
    checkpoint.Call(2);
    document().stopActiveDOMObjects();
    checkpoint.Call(3);
}

TEST_F(FetchBlobDataConsumerHandleTest, CancelLoaderWhenDestinationDetached)
{
    auto factory = new StrictMock<MockLoaderFactory>;
    Checkpoint checkpoint;

    std::unique_ptr<MockThreadableLoader> loader = MockThreadableLoader::create();
    MockThreadableLoader* loaderPtr = loader.get();

    InSequence s;
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*factory, createInternal(Ref(document()), _, _, _)).WillOnce(Return(loader.release()));
    EXPECT_CALL(*loaderPtr, start(_));
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(checkpoint, Call(3));
    EXPECT_CALL(*loaderPtr, cancel());
    EXPECT_CALL(checkpoint, Call(4));

    RefPtr<BlobDataHandle> blobDataHandle = createBlobDataHandle("Once upon a time");
    std::unique_ptr<WebDataConsumerHandle> handle
        = FetchBlobDataConsumerHandle::create(&document(), blobDataHandle, factory);
    std::unique_ptr<WebDataConsumerHandle::Reader> reader = handle->obtainReader(nullptr);
    testing::runPendingTasks();

    size_t size = 0;
    reader->read(nullptr, 0, kNone, &size);
    checkpoint.Call(1);
    testing::runPendingTasks();
    checkpoint.Call(2);
    handle = nullptr;
    reader = nullptr;
    checkpoint.Call(3);
    ThreadHeap::collectAllGarbage();
    checkpoint.Call(4);
}

TEST_F(FetchBlobDataConsumerHandleTest, ReadTest)
{
    auto factory = new StrictMock<MockLoaderFactory>;
    Checkpoint checkpoint;

    std::unique_ptr<MockThreadableLoader> loader = MockThreadableLoader::create();
    MockThreadableLoader* loaderPtr = loader.get();
    ThreadableLoaderClient* client = nullptr;

    InSequence s;
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*factory, createInternal(Ref(document()), _, _, _)).WillOnce(DoAll(SaveArg<1>(&client), Return(loader.release())));
    EXPECT_CALL(*loaderPtr, start(_));
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(*loaderPtr, cancel());

    RefPtr<BlobDataHandle> blobDataHandle = createBlobDataHandle("Once upon a time");
    std::unique_ptr<WebDataConsumerHandle> handle
        = FetchBlobDataConsumerHandle::create(&document(), blobDataHandle, factory);

    std::unique_ptr<ReplayingHandle> src = ReplayingHandle::create();
    src->add(Command(Command::Wait));
    src->add(Command(Command::Data, "hello, "));
    src->add(Command(Command::Data, "world"));
    src->add(Command(Command::Wait));
    src->add(Command(Command::Done));

    size_t size = 0;
    handle->obtainReader(nullptr)->read(nullptr, 0, kNone, &size);
    checkpoint.Call(1);
    testing::runPendingTasks();
    checkpoint.Call(2);
    client->didReceiveResponse(0, ResourceResponse(), std::move(src));
    HandleReaderRunner<HandleReader> runner(std::move(handle));
    std::unique_ptr<HandleReadResult> r = runner.wait();
    EXPECT_EQ(kDone, r->result());
    EXPECT_EQ("hello, world", toString(r->data()));
}

TEST_F(FetchBlobDataConsumerHandleTest, TwoPhaseReadTest)
{
    auto factory = new StrictMock<MockLoaderFactory>;
    Checkpoint checkpoint;

    std::unique_ptr<MockThreadableLoader> loader = MockThreadableLoader::create();
    MockThreadableLoader* loaderPtr = loader.get();
    ThreadableLoaderClient* client = nullptr;

    InSequence s;
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*factory, createInternal(Ref(document()), _, _, _)).WillOnce(DoAll(SaveArg<1>(&client), Return(loader.release())));
    EXPECT_CALL(*loaderPtr, start(_));
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(*loaderPtr, cancel());

    RefPtr<BlobDataHandle> blobDataHandle = createBlobDataHandle("Once upon a time");
    std::unique_ptr<WebDataConsumerHandle> handle
        = FetchBlobDataConsumerHandle::create(&document(), blobDataHandle, factory);

    std::unique_ptr<ReplayingHandle> src = ReplayingHandle::create();
    src->add(Command(Command::Wait));
    src->add(Command(Command::Data, "hello, "));
    src->add(Command(Command::Data, "world"));
    src->add(Command(Command::Wait));
    src->add(Command(Command::Done));

    size_t size = 0;
    handle->obtainReader(nullptr)->read(nullptr, 0, kNone, &size);
    checkpoint.Call(1);
    testing::runPendingTasks();
    checkpoint.Call(2);
    client->didReceiveResponse(0, ResourceResponse(), std::move(src));
    HandleReaderRunner<HandleTwoPhaseReader> runner(std::move(handle));
    std::unique_ptr<HandleReadResult> r = runner.wait();
    EXPECT_EQ(kDone, r->result());
    EXPECT_EQ("hello, world", toString(r->data()));
}

TEST_F(FetchBlobDataConsumerHandleTest, LoadErrorTest)
{
    auto factory = new StrictMock<MockLoaderFactory>;
    Checkpoint checkpoint;

    std::unique_ptr<MockThreadableLoader> loader = MockThreadableLoader::create();
    MockThreadableLoader* loaderPtr = loader.get();
    ThreadableLoaderClient* client = nullptr;

    InSequence s;
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*factory, createInternal(Ref(document()), _, _, _)).WillOnce(DoAll(SaveArg<1>(&client), Return(loader.release())));
    EXPECT_CALL(*loaderPtr, start(_));
    EXPECT_CALL(checkpoint, Call(2));

    RefPtr<BlobDataHandle> blobDataHandle = createBlobDataHandle("Once upon a time");
    std::unique_ptr<WebDataConsumerHandle> handle
        = FetchBlobDataConsumerHandle::create(&document(), blobDataHandle, factory);

    size_t size = 0;
    handle->obtainReader(nullptr)->read(nullptr, 0, kNone, &size);
    checkpoint.Call(1);
    testing::runPendingTasks();
    checkpoint.Call(2);
    client->didFail(ResourceError());
    HandleReaderRunner<HandleReader> runner(std::move(handle));
    std::unique_ptr<HandleReadResult> r = runner.wait();
    EXPECT_EQ(kUnexpectedError, r->result());
}

TEST_F(FetchBlobDataConsumerHandleTest, BodyLoadErrorTest)
{
    auto factory = new StrictMock<MockLoaderFactory>;
    Checkpoint checkpoint;

    std::unique_ptr<MockThreadableLoader> loader = MockThreadableLoader::create();
    MockThreadableLoader* loaderPtr = loader.get();
    ThreadableLoaderClient* client = nullptr;

    InSequence s;
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*factory, createInternal(Ref(document()), _, _, _)).WillOnce(DoAll(SaveArg<1>(&client), Return(loader.release())));
    EXPECT_CALL(*loaderPtr, start(_));
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(*loaderPtr, cancel());

    RefPtr<BlobDataHandle> blobDataHandle = createBlobDataHandle("Once upon a time");
    std::unique_ptr<WebDataConsumerHandle> handle
        = FetchBlobDataConsumerHandle::create(&document(), blobDataHandle, factory);

    std::unique_ptr<ReplayingHandle> src = ReplayingHandle::create();
    src->add(Command(Command::Wait));
    src->add(Command(Command::Data, "hello, "));
    src->add(Command(Command::Error));

    size_t size = 0;
    handle->obtainReader(nullptr)->read(nullptr, 0, kNone, &size);
    checkpoint.Call(1);
    testing::runPendingTasks();
    checkpoint.Call(2);
    client->didReceiveResponse(0, ResourceResponse(), std::move(src));
    HandleReaderRunner<HandleReader> runner(std::move(handle));
    std::unique_ptr<HandleReadResult> r = runner.wait();
    EXPECT_EQ(kUnexpectedError, r->result());
}

TEST_F(FetchBlobDataConsumerHandleTest, DrainAsBlobDataHandle)
{
    auto factory = new StrictMock<MockLoaderFactory>;

    RefPtr<BlobDataHandle> blobDataHandle = createBlobDataHandle("Once upon a time");
    std::unique_ptr<FetchDataConsumerHandle> handle
        = FetchBlobDataConsumerHandle::create(&document(), blobDataHandle, factory);

    size_t size = 0;
    EXPECT_EQ(blobDataHandle, handle->obtainReader(nullptr)->drainAsBlobDataHandle());
    EXPECT_FALSE(handle->obtainReader(nullptr)->drainAsFormData());

    EXPECT_EQ(kDone, handle->obtainReader(nullptr)->read(nullptr, 0, kNone, &size));
}

TEST_F(FetchBlobDataConsumerHandleTest, DrainAsFormData)
{
    auto factory = new StrictMock<MockLoaderFactory>;

    RefPtr<BlobDataHandle> blobDataHandle = createBlobDataHandle("Once upon a time");
    std::unique_ptr<FetchDataConsumerHandle> handle
        = FetchBlobDataConsumerHandle::create(&document(), blobDataHandle, factory);

    RefPtr<EncodedFormData> formData = handle->obtainReader(nullptr)->drainAsFormData();
    ASSERT_TRUE(formData);
    EXPECT_TRUE(formData->isSafeToSendToAnotherThread());
    ASSERT_EQ(1u, formData->elements().size());
    EXPECT_EQ(FormDataElement::encodedBlob, formData->elements()[0].m_type);
    EXPECT_EQ(blobDataHandle->uuid(), formData->elements()[0].m_blobUUID);
    EXPECT_EQ(blobDataHandle, formData->elements()[0].m_optionalBlobDataHandle);

    EXPECT_FALSE(handle->obtainReader(nullptr)->drainAsBlobDataHandle());
    size_t size;
    EXPECT_EQ(kDone, handle->obtainReader(nullptr)->read(nullptr, 0, kNone, &size));
}

TEST_F(FetchBlobDataConsumerHandleTest, ZeroByteReadDoesNotAffectDraining)
{
    auto factory = new StrictMock<MockLoaderFactory>;

    RefPtr<BlobDataHandle> blobDataHandle = createBlobDataHandle("Once upon a time");
    std::unique_ptr<FetchDataConsumerHandle> handle
        = FetchBlobDataConsumerHandle::create(&document(), blobDataHandle, factory);
    std::unique_ptr<FetchDataConsumerHandle::Reader> reader = handle->obtainReader(nullptr);

    size_t readSize;
    EXPECT_EQ(kShouldWait, reader->read(nullptr, 0, kNone, &readSize));
    EXPECT_EQ(blobDataHandle, reader->drainAsBlobDataHandle());
}

TEST_F(FetchBlobDataConsumerHandleTest, OneByteReadAffectsDraining)
{
    auto factory = new StrictMock<MockLoaderFactory>;

    RefPtr<BlobDataHandle> blobDataHandle = createBlobDataHandle("Once upon a time");
    std::unique_ptr<FetchDataConsumerHandle> handle
        = FetchBlobDataConsumerHandle::create(&document(), blobDataHandle, factory);
    std::unique_ptr<FetchDataConsumerHandle::Reader> reader = handle->obtainReader(nullptr);

    size_t readSize;
    char c;
    EXPECT_EQ(kShouldWait, reader->read(&c, 1, kNone, &readSize));
    EXPECT_FALSE(reader->drainAsFormData());
}

TEST_F(FetchBlobDataConsumerHandleTest, BeginReadAffectsDraining)
{
    auto factory = new StrictMock<MockLoaderFactory>;

    RefPtr<BlobDataHandle> blobDataHandle = createBlobDataHandle("Once upon a time");
    std::unique_ptr<FetchDataConsumerHandle> handle
        = FetchBlobDataConsumerHandle::create(&document(), blobDataHandle, factory);
    std::unique_ptr<FetchDataConsumerHandle::Reader> reader = handle->obtainReader(nullptr);

    const void* buffer;
    size_t available;
    EXPECT_EQ(kShouldWait, reader->beginRead(&buffer, kNone, &available));
    EXPECT_FALSE(reader->drainAsBlobDataHandle());
}

} // namespace
} // namespace blink

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/fetch/FetchFormDataConsumerHandle.h"

#include "core/dom/DOMTypedArray.h"
#include "core/html/FormData.h"
#include "core/loader/MockThreadableLoader.h"
#include "core/loader/ThreadableLoaderClient.h"
#include "core/testing/DummyPageHolder.h"
#include "modules/fetch/DataConsumerHandleTestUtil.h"
#include "platform/network/ResourceResponse.h"
#include "platform/testing/UnitTestHelpers.h"
#include "platform/weborigin/KURL.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"
#include "wtf/text/TextEncoding.h"
#include "wtf/text/WTFString.h"
#include <memory>
#include <string.h>

namespace blink {
namespace {

using Result = WebDataConsumerHandle::Result;
const Result kOk = WebDataConsumerHandle::Ok;
const Result kDone = WebDataConsumerHandle::Done;
const Result kShouldWait = WebDataConsumerHandle::ShouldWait;
const WebDataConsumerHandle::Flags kNone = WebDataConsumerHandle::FlagNone;
using HandleReader = DataConsumerHandleTestUtil::HandleReader;
using HandleTwoPhaseReader = DataConsumerHandleTestUtil::HandleTwoPhaseReader;
using HandleReadResult = DataConsumerHandleTestUtil::HandleReadResult;
template <typename T>
using HandleReaderRunner = DataConsumerHandleTestUtil::HandleReaderRunner<T>;
using ReplayingHandle = DataConsumerHandleTestUtil::ReplayingHandle;
using Command = DataConsumerHandleTestUtil::Command;

using ::testing::_;
using ::testing::InvokeWithoutArgs;

String toString(const Vector<char>& data)
{
    return String(data.data(), data.size());
}

class LoaderFactory : public FetchBlobDataConsumerHandle::LoaderFactory {
public:
    explicit LoaderFactory(std::unique_ptr<WebDataConsumerHandle> handle)
        : m_client(nullptr)
        , m_handle(std::move(handle)) {}
    std::unique_ptr<ThreadableLoader> create(ExecutionContext&, ThreadableLoaderClient* client, const ThreadableLoaderOptions&, const ResourceLoaderOptions&) override
    {
        m_client = client;

        std::unique_ptr<MockThreadableLoader> loader = MockThreadableLoader::create();
        EXPECT_CALL(*loader, start(_)).WillOnce(InvokeWithoutArgs(this, &LoaderFactory::handleDidReceiveResponse));
        EXPECT_CALL(*loader, cancel()).Times(1);
        return std::move(loader);
    }

private:
    void handleDidReceiveResponse()
    {
        m_client->didReceiveResponse(0, ResourceResponse(), std::move(m_handle));
    }

    ThreadableLoaderClient* m_client;
    std::unique_ptr<WebDataConsumerHandle> m_handle;
};

class FetchFormDataConsumerHandleTest : public ::testing::Test {
public:
    FetchFormDataConsumerHandleTest() : m_page(DummyPageHolder::create(IntSize(1, 1))) {}

protected:
    Document* getDocument() { return &m_page->document(); }

    std::unique_ptr<DummyPageHolder> m_page;
};

PassRefPtr<EncodedFormData> complexFormData()
{
    RefPtr<EncodedFormData> data = EncodedFormData::create();

    data->appendData("foo", 3);
    data->appendFileRange("/foo/bar/baz", 3, 4, 5);
    data->appendFileSystemURLRange(KURL(KURL(), "file:///foo/bar/baz"), 6, 7, 8);
    std::unique_ptr<BlobData> blobData = BlobData::create();
    blobData->appendText("hello", false);
    auto size = blobData->length();
    RefPtr<BlobDataHandle> blobDataHandle = BlobDataHandle::create(std::move(blobData), size);
    data->appendBlob(blobDataHandle->uuid(), blobDataHandle);
    Vector<char> boundary;
    boundary.append("\0", 1);
    data->setBoundary(boundary);
    return data.release();
}

void verifyComplexFormData(EncodedFormData* data)
{
    const auto& elements = data->elements();
    if (4 != elements.size()) {
        FAIL() << "data->elements().size() should be 4, but is " << data->elements().size() << ".";
    }
    EXPECT_EQ(FormDataElement::data, elements[0].m_type);
    EXPECT_EQ("foo", String(elements[0].m_data.data(), elements[0].m_data.size()));

    EXPECT_EQ(FormDataElement::encodedFile, elements[1].m_type);
    EXPECT_EQ("/foo/bar/baz", elements[1].m_filename);
    EXPECT_EQ(3, elements[1].m_fileStart);
    EXPECT_EQ(4, elements[1].m_fileLength);
    EXPECT_EQ(5, elements[1].m_expectedFileModificationTime);

    EXPECT_EQ(FormDataElement::encodedFileSystemURL, elements[2].m_type);
    EXPECT_EQ(KURL(KURL(), "file:///foo/bar/baz"), elements[2].m_fileSystemURL);
    EXPECT_EQ(6, elements[2].m_fileStart);
    EXPECT_EQ(7, elements[2].m_fileLength);
    EXPECT_EQ(8, elements[2].m_expectedFileModificationTime);

    EXPECT_EQ(FormDataElement::encodedBlob, elements[3].m_type);
    if (!elements[3].m_optionalBlobDataHandle) {
        FAIL() << "optional BlobDataHandle must be set.";
    }
    EXPECT_EQ(elements[3].m_blobUUID, elements[3].m_optionalBlobDataHandle->uuid());
    EXPECT_EQ(5u, elements[3].m_optionalBlobDataHandle->size());
}

TEST_F(FetchFormDataConsumerHandleTest, ReadFromString)
{
    std::unique_ptr<FetchDataConsumerHandle> handle = FetchFormDataConsumerHandle::create(String("hello, world"));
    HandleReaderRunner<HandleReader> runner(std::move(handle));
    std::unique_ptr<HandleReadResult> r = runner.wait();
    EXPECT_EQ(kDone, r->result());
    EXPECT_EQ("hello, world", toString(r->data()));
}

TEST_F(FetchFormDataConsumerHandleTest, TwoPhaseReadFromString)
{
    std::unique_ptr<FetchDataConsumerHandle> handle = FetchFormDataConsumerHandle::create(String("hello, world"));
    HandleReaderRunner<HandleTwoPhaseReader> runner(std::move(handle));
    std::unique_ptr<HandleReadResult> r = runner.wait();
    EXPECT_EQ(kDone, r->result());
    EXPECT_EQ("hello, world", toString(r->data()));
}

TEST_F(FetchFormDataConsumerHandleTest, ReadFromStringNonLatin)
{
    UChar cs[] = {0x3042, 0};
    std::unique_ptr<FetchDataConsumerHandle> handle = FetchFormDataConsumerHandle::create(String(cs));
    HandleReaderRunner<HandleReader> runner(std::move(handle));
    std::unique_ptr<HandleReadResult> r = runner.wait();
    EXPECT_EQ(kDone, r->result());
    EXPECT_EQ("\xe3\x81\x82", toString(r->data()));
}

TEST_F(FetchFormDataConsumerHandleTest, ReadFromArrayBuffer)
{
    const unsigned char data[] = { 0x21, 0xfe, 0x00, 0x00, 0xff, 0xa3, 0x42, 0x30, 0x42, 0x99, 0x88 };
    DOMArrayBuffer* buffer = DOMArrayBuffer::create(data, WTF_ARRAY_LENGTH(data));
    std::unique_ptr<FetchDataConsumerHandle> handle = FetchFormDataConsumerHandle::create(buffer);
    HandleReaderRunner<HandleReader> runner(std::move(handle));
    std::unique_ptr<HandleReadResult> r = runner.wait();
    EXPECT_EQ(kDone, r->result());
    Vector<char> expected;
    expected.append(data, WTF_ARRAY_LENGTH(data));
    EXPECT_EQ(expected, r->data());
}

TEST_F(FetchFormDataConsumerHandleTest, ReadFromArrayBufferView)
{
    const unsigned char data[] = { 0x21, 0xfe, 0x00, 0x00, 0xff, 0xa3, 0x42, 0x30, 0x42, 0x99, 0x88 };
    const size_t offset = 1, size = 4;
    DOMArrayBuffer* buffer = DOMArrayBuffer::create(data, WTF_ARRAY_LENGTH(data));
    std::unique_ptr<FetchDataConsumerHandle> handle = FetchFormDataConsumerHandle::create(DOMUint8Array::create(buffer, offset, size));
    HandleReaderRunner<HandleReader> runner(std::move(handle));
    std::unique_ptr<HandleReadResult> r = runner.wait();
    EXPECT_EQ(kDone, r->result());
    Vector<char> expected;
    expected.append(data + offset, size);
    EXPECT_EQ(expected, r->data());
}

TEST_F(FetchFormDataConsumerHandleTest, ReadFromSimplFormData)
{
    RefPtr<EncodedFormData> data = EncodedFormData::create();
    data->appendData("foo", 3);
    data->appendData("hoge", 4);

    std::unique_ptr<FetchDataConsumerHandle> handle = FetchFormDataConsumerHandle::create(getDocument(), data);
    HandleReaderRunner<HandleReader> runner(std::move(handle));
    testing::runPendingTasks();
    std::unique_ptr<HandleReadResult> r = runner.wait();
    EXPECT_EQ(kDone, r->result());
    EXPECT_EQ("foohoge", toString(r->data()));
}

TEST_F(FetchFormDataConsumerHandleTest, ReadFromComplexFormData)
{
    RefPtr<EncodedFormData> data = complexFormData();
    std::unique_ptr<ReplayingHandle> src = ReplayingHandle::create();
    src->add(Command(Command::Data, "bar"));
    src->add(Command(Command::Done));
    std::unique_ptr<FetchDataConsumerHandle> handle = FetchFormDataConsumerHandle::createForTest(getDocument(), data, new LoaderFactory(std::move(src)));
    char c;
    size_t readSize;
    EXPECT_EQ(kShouldWait, handle->obtainReader(nullptr)->read(&c, 1, kNone, &readSize));

    HandleReaderRunner<HandleReader> runner(std::move(handle));
    testing::runPendingTasks();
    std::unique_ptr<HandleReadResult> r = runner.wait();
    EXPECT_EQ(kDone, r->result());
    EXPECT_EQ("bar", toString(r->data()));
}

TEST_F(FetchFormDataConsumerHandleTest, TwoPhaseReadFromComplexFormData)
{
    RefPtr<EncodedFormData> data = complexFormData();
    std::unique_ptr<ReplayingHandle> src = ReplayingHandle::create();
    src->add(Command(Command::Data, "bar"));
    src->add(Command(Command::Done));
    std::unique_ptr<FetchDataConsumerHandle> handle = FetchFormDataConsumerHandle::createForTest(getDocument(), data, new LoaderFactory(std::move(src)));
    char c;
    size_t readSize;
    EXPECT_EQ(kShouldWait, handle->obtainReader(nullptr)->read(&c, 1, kNone, &readSize));

    HandleReaderRunner<HandleTwoPhaseReader> runner(std::move(handle));
    testing::runPendingTasks();
    std::unique_ptr<HandleReadResult> r = runner.wait();
    EXPECT_EQ(kDone, r->result());
    EXPECT_EQ("bar", toString(r->data()));
}

TEST_F(FetchFormDataConsumerHandleTest, DrainAsBlobDataHandleFromString)
{
    std::unique_ptr<FetchDataConsumerHandle> handle = FetchFormDataConsumerHandle::create(String("hello, world"));
    std::unique_ptr<FetchDataConsumerHandle::Reader> reader = handle->obtainReader(nullptr);
    RefPtr<BlobDataHandle> blobDataHandle = reader->drainAsBlobDataHandle();
    ASSERT_TRUE(blobDataHandle);

    EXPECT_EQ(String(), blobDataHandle->type());
    EXPECT_EQ(12u, blobDataHandle->size());
    EXPECT_EQ(nullptr, reader->drainAsFormData());
    char c;
    size_t readSize;
    EXPECT_EQ(kDone, reader->read(&c, 1, kNone, &readSize));
}

TEST_F(FetchFormDataConsumerHandleTest, DrainAsBlobDataHandleFromArrayBuffer)
{
    std::unique_ptr<FetchDataConsumerHandle> handle = FetchFormDataConsumerHandle::create(DOMArrayBuffer::create("foo", 3));
    std::unique_ptr<FetchDataConsumerHandle::Reader> reader = handle->obtainReader(nullptr);
    RefPtr<BlobDataHandle> blobDataHandle = reader->drainAsBlobDataHandle();
    ASSERT_TRUE(blobDataHandle);

    EXPECT_EQ(String(), blobDataHandle->type());
    EXPECT_EQ(3u, blobDataHandle->size());
    EXPECT_EQ(nullptr, reader->drainAsFormData());
    char c;
    size_t readSize;
    EXPECT_EQ(kDone, reader->read(&c, 1, kNone, &readSize));
}

TEST_F(FetchFormDataConsumerHandleTest, DrainAsBlobDataHandleFromSimpleFormData)
{
    FormData* data = FormData::create(UTF8Encoding());
    data->append("name1", "value1");
    data->append("name2", "value2");
    RefPtr<EncodedFormData> inputFormData = data->encodeMultiPartFormData();

    std::unique_ptr<FetchDataConsumerHandle> handle = FetchFormDataConsumerHandle::create(getDocument(), inputFormData);
    std::unique_ptr<FetchDataConsumerHandle::Reader> reader = handle->obtainReader(nullptr);
    RefPtr<BlobDataHandle> blobDataHandle = reader->drainAsBlobDataHandle();
    ASSERT_TRUE(blobDataHandle);

    EXPECT_EQ(String(), blobDataHandle->type());
    EXPECT_EQ(inputFormData->flattenToString().utf8().length(), blobDataHandle->size());
    EXPECT_EQ(nullptr, reader->drainAsFormData());
    char c;
    size_t readSize;
    EXPECT_EQ(kDone, reader->read(&c, 1, kNone, &readSize));
}

TEST_F(FetchFormDataConsumerHandleTest, DrainAsBlobDataHandleFromComplexFormData)
{
    RefPtr<EncodedFormData> inputFormData = complexFormData();

    std::unique_ptr<FetchDataConsumerHandle> handle = FetchFormDataConsumerHandle::create(getDocument(), inputFormData);
    std::unique_ptr<FetchDataConsumerHandle::Reader> reader = handle->obtainReader(nullptr);
    RefPtr<BlobDataHandle> blobDataHandle = reader->drainAsBlobDataHandle();
    ASSERT_TRUE(blobDataHandle);

    EXPECT_EQ(nullptr, reader->drainAsFormData());
    char c;
    size_t readSize;
    EXPECT_EQ(kDone, reader->read(&c, 1, kNone, &readSize));
}

TEST_F(FetchFormDataConsumerHandleTest, DrainAsFormDataFromString)
{
    std::unique_ptr<FetchDataConsumerHandle> handle = FetchFormDataConsumerHandle::create(String("hello, world"));
    std::unique_ptr<FetchDataConsumerHandle::Reader> reader = handle->obtainReader(nullptr);
    RefPtr<EncodedFormData> formData = reader->drainAsFormData();
    ASSERT_TRUE(formData);
    EXPECT_TRUE(formData->isSafeToSendToAnotherThread());
    EXPECT_EQ("hello, world", formData->flattenToString());

    const void* buffer = nullptr;
    size_t size;
    EXPECT_EQ(kDone, reader->read(nullptr, 0, kNone, &size));
    EXPECT_EQ(kDone, reader->beginRead(&buffer, kNone, &size));
}

TEST_F(FetchFormDataConsumerHandleTest, DrainAsFormDataFromArrayBuffer)
{
    std::unique_ptr<FetchDataConsumerHandle> handle = FetchFormDataConsumerHandle::create(DOMArrayBuffer::create("foo", 3));
    std::unique_ptr<FetchDataConsumerHandle::Reader> reader = handle->obtainReader(nullptr);
    RefPtr<EncodedFormData> formData = reader->drainAsFormData();
    ASSERT_TRUE(formData);
    EXPECT_TRUE(formData->isSafeToSendToAnotherThread());
    EXPECT_EQ("foo", formData->flattenToString());
}

TEST_F(FetchFormDataConsumerHandleTest, DrainAsFormDataFromSimpleFormData)
{
    FormData* data = FormData::create(UTF8Encoding());
    data->append("name1", "value1");
    data->append("name2", "value2");
    RefPtr<EncodedFormData> inputFormData = data->encodeMultiPartFormData();

    std::unique_ptr<FetchDataConsumerHandle> handle = FetchFormDataConsumerHandle::create(getDocument(), inputFormData);
    std::unique_ptr<FetchDataConsumerHandle::Reader> reader = handle->obtainReader(nullptr);
    RefPtr<EncodedFormData> outputFormData = reader->drainAsFormData();
    ASSERT_TRUE(outputFormData);
    EXPECT_TRUE(outputFormData->isSafeToSendToAnotherThread());
    EXPECT_NE(outputFormData.get(), inputFormData.get());
    EXPECT_EQ(inputFormData->flattenToString(), outputFormData->flattenToString());
}

TEST_F(FetchFormDataConsumerHandleTest, DrainAsFormDataFromComplexFormData)
{
    RefPtr<EncodedFormData> inputFormData = complexFormData();

    std::unique_ptr<FetchDataConsumerHandle> handle = FetchFormDataConsumerHandle::create(getDocument(), inputFormData);
    std::unique_ptr<FetchDataConsumerHandle::Reader> reader = handle->obtainReader(nullptr);
    RefPtr<EncodedFormData> outputFormData = reader->drainAsFormData();
    ASSERT_TRUE(outputFormData);
    EXPECT_TRUE(outputFormData->isSafeToSendToAnotherThread());
    EXPECT_NE(outputFormData.get(), inputFormData.get());
    verifyComplexFormData(outputFormData.get());
}

TEST_F(FetchFormDataConsumerHandleTest, ZeroByteReadDoesNotAffectDraining)
{
    std::unique_ptr<FetchDataConsumerHandle> handle = FetchFormDataConsumerHandle::create(String("hello, world"));
    std::unique_ptr<FetchDataConsumerHandle::Reader> reader = handle->obtainReader(nullptr);
    size_t readSize;
    EXPECT_EQ(kOk, reader->read(nullptr, 0, kNone, &readSize));
    RefPtr<EncodedFormData> formData = reader->drainAsFormData();
    ASSERT_TRUE(formData);
    EXPECT_TRUE(formData->isSafeToSendToAnotherThread());
    EXPECT_EQ("hello, world", formData->flattenToString());
}

TEST_F(FetchFormDataConsumerHandleTest, OneByteReadAffectsDraining)
{
    char c;
    std::unique_ptr<FetchDataConsumerHandle> handle = FetchFormDataConsumerHandle::create(String("hello, world"));
    std::unique_ptr<FetchDataConsumerHandle::Reader> reader = handle->obtainReader(nullptr);
    size_t readSize;
    EXPECT_EQ(kOk, reader->read(&c, 1, kNone, &readSize));
    EXPECT_EQ(1u, readSize);
    EXPECT_EQ('h', c);
    EXPECT_FALSE(reader->drainAsFormData());
}

TEST_F(FetchFormDataConsumerHandleTest, BeginReadAffectsDraining)
{
    const void* buffer = nullptr;
    std::unique_ptr<FetchDataConsumerHandle> handle = FetchFormDataConsumerHandle::create(String("hello, world"));
    std::unique_ptr<FetchDataConsumerHandle::Reader> reader = handle->obtainReader(nullptr);
    size_t available;
    EXPECT_EQ(kOk, reader->beginRead(&buffer, kNone, &available));
    ASSERT_TRUE(buffer);
    EXPECT_EQ("hello, world", String(static_cast<const char*>(buffer), available));
    EXPECT_FALSE(reader->drainAsFormData());
    reader->endRead(0);
    EXPECT_FALSE(reader->drainAsFormData());
}

TEST_F(FetchFormDataConsumerHandleTest, ZeroByteReadDoesNotAffectDrainingForComplexFormData)
{
    std::unique_ptr<ReplayingHandle> src = ReplayingHandle::create();
    src->add(Command(Command::Data, "bar"));
    src->add(Command(Command::Done));
    std::unique_ptr<FetchDataConsumerHandle> handle = FetchFormDataConsumerHandle::createForTest(getDocument(), complexFormData(), new LoaderFactory(std::move(src)));
    std::unique_ptr<FetchDataConsumerHandle::Reader> reader = handle->obtainReader(nullptr);
    size_t readSize;
    EXPECT_EQ(kShouldWait, reader->read(nullptr, 0, kNone, &readSize));
    testing::runPendingTasks();
    EXPECT_EQ(kOk, reader->read(nullptr, 0, kNone, &readSize));
    RefPtr<EncodedFormData> formData = reader->drainAsFormData();
    ASSERT_TRUE(formData);
    EXPECT_TRUE(formData->isSafeToSendToAnotherThread());
    verifyComplexFormData(formData.get());
}

TEST_F(FetchFormDataConsumerHandleTest, OneByteReadAffectsDrainingForComplexFormData)
{
    std::unique_ptr<ReplayingHandle> src = ReplayingHandle::create();
    src->add(Command(Command::Data, "bar"));
    src->add(Command(Command::Done));
    std::unique_ptr<FetchDataConsumerHandle> handle = FetchFormDataConsumerHandle::createForTest(getDocument(), complexFormData(), new LoaderFactory(std::move(src)));
    std::unique_ptr<FetchDataConsumerHandle::Reader> reader = handle->obtainReader(nullptr);
    char c;
    size_t readSize;
    EXPECT_EQ(kShouldWait, reader->read(&c, 1, kNone, &readSize));
    testing::runPendingTasks();
    EXPECT_EQ(kOk, reader->read(&c, 1, kNone, &readSize));
    EXPECT_EQ(1u, readSize);
    EXPECT_EQ('b', c);
    EXPECT_FALSE(reader->drainAsFormData());
}

TEST_F(FetchFormDataConsumerHandleTest, BeginReadAffectsDrainingForComplexFormData)
{
    std::unique_ptr<ReplayingHandle> src = ReplayingHandle::create();
    src->add(Command(Command::Data, "bar"));
    src->add(Command(Command::Done));
    const void* buffer = nullptr;
    std::unique_ptr<FetchDataConsumerHandle> handle = FetchFormDataConsumerHandle::createForTest(getDocument(), complexFormData(), new LoaderFactory(std::move(src)));
    std::unique_ptr<FetchDataConsumerHandle::Reader> reader = handle->obtainReader(nullptr);
    size_t available;
    EXPECT_EQ(kShouldWait, reader->beginRead(&buffer, kNone, &available));
    testing::runPendingTasks();
    EXPECT_EQ(kOk, reader->beginRead(&buffer, kNone, &available));
    EXPECT_FALSE(reader->drainAsFormData());
    reader->endRead(0);
    EXPECT_FALSE(reader->drainAsFormData());
}

} // namespace
} // namespace blink

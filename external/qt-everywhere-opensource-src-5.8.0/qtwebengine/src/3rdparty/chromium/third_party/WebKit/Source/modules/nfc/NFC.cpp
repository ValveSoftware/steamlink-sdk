// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/nfc/NFC.h"

#include "bindings/core/v8/JSONValuesForV8.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/V8ArrayBuffer.h"
#include "core/dom/DOMArrayBuffer.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/frame/LocalDOMWindow.h"
#include "modules/nfc/NFCError.h"
#include "modules/nfc/NFCMessage.h"
#include "modules/nfc/NFCPushOptions.h"
#include "platform/mojo/MojoHelper.h"
#include "public/platform/ServiceRegistry.h"

namespace nfc = device::nfc::blink;

namespace {
const char kJsonMimePrefix[] = "application/";
const char kJsonMimeType[] = "application/json";
const char kOpaqueMimeType[] = "application/octet-stream";
const char kPlainTextMimeType[] = "text/plain";
const char kPlainTextMimePrefix[] = "text/";
const char kCharSetUTF8[] = ";charset=UTF-8";
} // anonymous namespace

// Mojo type converters
namespace mojo {

using nfc::NFCMessage;
using nfc::NFCMessagePtr;
using nfc::NFCRecord;
using nfc::NFCRecordPtr;
using nfc::NFCRecordType;
using nfc::NFCPushOptions;
using nfc::NFCPushOptionsPtr;
using nfc::NFCPushTarget;

NFCPushTarget toNFCPushTarget(const WTF::String& target)
{
    if (target == "tag")
        return NFCPushTarget::TAG;

    if (target == "peer")
        return NFCPushTarget::PEER;

    return NFCPushTarget::ANY;
}

NFCRecordType toNFCRecordType(const WTF::String& recordType)
{
    if (recordType == "empty")
        return NFCRecordType::EMPTY;

    if (recordType == "text")
        return NFCRecordType::TEXT;

    if (recordType == "url")
        return NFCRecordType::URL;

    if (recordType == "json")
        return NFCRecordType::JSON;

    if (recordType == "opaque")
        return NFCRecordType::OPAQUE_RECORD;

    NOTREACHED();
    return NFCRecordType::EMPTY;
}

// https://w3c.github.io/web-nfc/#creating-web-nfc-message Step 2.1
// If NFCRecord type is not provided, deduce NFCRecord type from JS data type:
// String or Number => 'text' record
// ArrayBuffer => 'opaque' record
// JSON serializable Object => 'json' record
NFCRecordType deduceRecordTypeFromDataType(const blink::NFCRecord& record)
{
    if (record.hasData()) {
        v8::Local<v8::Value> value = record.data().v8Value();

        if (value->IsString()
            || (value->IsNumber() && !std::isnan(value.As<v8::Number>()->Value()))) {
            return NFCRecordType::TEXT;
        }

        if (value->IsObject() && !value->IsArrayBuffer()) {
            return NFCRecordType::JSON;
        }

        if (value->IsArrayBuffer()) {
            return NFCRecordType::OPAQUE_RECORD;
        }
    }

    return NFCRecordType::EMPTY;
}

void setMediaType(NFCRecordPtr& recordPtr, const WTF::String& recordMediaType, const WTF::String& defaultMediaType)
{
    recordPtr->mediaType = recordMediaType.isEmpty() ? defaultMediaType : recordMediaType;
}

template <>
struct TypeConverter<mojo::WTFArray<uint8_t>, WTF::String> {
    static mojo::WTFArray<uint8_t> Convert(const WTF::String& string)
    {
        WTF::CString utf8String = string.utf8();
        WTF::Vector<uint8_t> array;
        array.append(utf8String.data(), utf8String.length());
        return mojo::WTFArray<uint8_t>(std::move(array));
    }
};

template <>
struct TypeConverter<mojo::WTFArray<uint8_t>, blink::DOMArrayBuffer*> {
    static mojo::WTFArray<uint8_t> Convert(blink::DOMArrayBuffer* buffer)
    {
        WTF::Vector<uint8_t> array;
        array.append(static_cast<uint8_t*>(buffer->data()), buffer->byteLength());
        return mojo::WTFArray<uint8_t>(std::move(array));
    }
};

template <>
struct TypeConverter<NFCRecordPtr, WTF::String> {
    static NFCRecordPtr Convert(const WTF::String& string)
    {
        NFCRecordPtr record = NFCRecord::New();
        record->recordType = NFCRecordType::TEXT;
        record->mediaType = kPlainTextMimeType;
        record->mediaType.append(kCharSetUTF8);
        record->data = mojo::WTFArray<uint8_t>::From(string);
        return record;
    }
};

template <>
struct TypeConverter<NFCRecordPtr, blink::DOMArrayBuffer*> {
    static NFCRecordPtr Convert(blink::DOMArrayBuffer* buffer)
    {
        NFCRecordPtr record = NFCRecord::New();
        record->recordType = NFCRecordType::OPAQUE_RECORD;
        record->mediaType = kOpaqueMimeType;
        record->data = mojo::WTFArray<uint8_t>::From(buffer);
        return record;
    }
};

template <>
struct TypeConverter<NFCMessagePtr, WTF::String> {
    static NFCMessagePtr Convert(const WTF::String& string)
    {
        NFCMessagePtr message = NFCMessage::New();
        message->data = mojo::WTFArray<NFCRecordPtr>::New(1);
        message->data[0] = NFCRecord::From(string);
        return message;
    }
};

template <>
struct TypeConverter<mojo::WTFArray<uint8_t>, blink::ScriptValue> {
    static mojo::WTFArray<uint8_t> Convert(const blink::ScriptValue& scriptValue)
    {
        v8::Local<v8::Value> value = scriptValue.v8Value();

        if (value->IsNumber())
            return mojo::WTFArray<uint8_t>::From(WTF::String::number(value.As<v8::Number>()->Value()));

        if (value->IsString()) {
            blink::V8StringResource<> stringResource = value;
            if (stringResource.prepare())
                return mojo::WTFArray<uint8_t>::From<WTF::String>(stringResource);
        }

        if (value->IsObject() && !value->IsArrayBuffer()) {
            RefPtr<blink::JSONValue> jsonResult = blink::toJSONValue(scriptValue.context(), value);
            if (jsonResult && (jsonResult->getType() == blink::JSONValue::TypeObject))
                return mojo::WTFArray<uint8_t>::From(jsonResult->toJSONString());
        }

        if (value->IsArrayBuffer())
            return mojo::WTFArray<uint8_t>::From(blink::V8ArrayBuffer::toImpl(value.As<v8::Object>()));

        return nullptr;
    }
};

template <>
struct TypeConverter<NFCRecordPtr, blink::NFCRecord> {
    static NFCRecordPtr Convert(const blink::NFCRecord& record)
    {
        NFCRecordPtr recordPtr = NFCRecord::New();

        if (record.hasRecordType())
            recordPtr->recordType = toNFCRecordType(record.recordType());
        else
            recordPtr->recordType = deduceRecordTypeFromDataType(record);

        // If record type is "empty", no need to set media type or data.
        // https://w3c.github.io/web-nfc/#creating-web-nfc-message
        if (recordPtr->recordType == NFCRecordType::EMPTY)
            return recordPtr;

        switch (recordPtr->recordType) {
        case NFCRecordType::TEXT:
        case NFCRecordType::URL:
            setMediaType(recordPtr, record.mediaType(), kPlainTextMimeType);
            recordPtr->mediaType.append(kCharSetUTF8);
            break;
        case NFCRecordType::JSON:
            setMediaType(recordPtr, record.mediaType(), kJsonMimeType);
            break;
        case NFCRecordType::OPAQUE_RECORD:
            setMediaType(recordPtr, record.mediaType(), kOpaqueMimeType);
            break;
        default:
            NOTREACHED();
            break;
        }

        recordPtr->data = mojo::WTFArray<uint8_t>::From(record.data());

        // If JS object cannot be converted to uint8_t array, return null,
        // interrupt NFCMessage conversion algorithm and reject promise with
        // SyntaxError exception.
        if (recordPtr->data.is_null())
            return nullptr;

        return recordPtr;
    }
};

template <>
struct TypeConverter<NFCMessagePtr, blink::NFCMessage> {
    static NFCMessagePtr Convert(const blink::NFCMessage& message)
    {
        NFCMessagePtr messagePtr = NFCMessage::New();
        messagePtr->url = message.url();
        messagePtr->data.resize(message.data().size());
        for (size_t i = 0; i < message.data().size(); ++i) {
            NFCRecordPtr record = NFCRecord::From(message.data()[i]);
            if (record.is_null())
                return nullptr;

            messagePtr->data[i] = std::move(record);
        }
        messagePtr->data = mojo::WTFArray<NFCRecordPtr>::From(message.data());
        return messagePtr;
    }
};

template <>
struct TypeConverter<NFCMessagePtr, blink::DOMArrayBuffer*> {
    static NFCMessagePtr Convert(blink::DOMArrayBuffer* buffer)
    {
        NFCMessagePtr message = NFCMessage::New();
        message->data = mojo::WTFArray<NFCRecordPtr>::New(1);
        message->data[0] = NFCRecord::From(buffer);
        return message;
    }
};

template <>
struct TypeConverter<NFCMessagePtr, blink::NFCPushMessage> {
    static NFCMessagePtr Convert(const blink::NFCPushMessage& message)
    {
        if (message.isString())
            return NFCMessage::From(message.getAsString());

        if (message.isNFCMessage())
            return NFCMessage::From(message.getAsNFCMessage());

        if (message.isArrayBuffer())
            return NFCMessage::From(message.getAsArrayBuffer());

        NOTREACHED();
        return nullptr;
    }
};

template <>
struct TypeConverter<NFCPushOptionsPtr, blink::NFCPushOptions> {
    static NFCPushOptionsPtr Convert(const blink::NFCPushOptions& pushOptions)
    {
        // https://w3c.github.io/web-nfc/#the-nfcpushoptions-dictionary
        // Default values for NFCPushOptions dictionary are:
        // target = 'any', timeout = Infinity, ignoreRead = true
        NFCPushOptionsPtr pushOptionsPtr = NFCPushOptions::New();

        if (pushOptions.hasTarget())
            pushOptionsPtr->target = toNFCPushTarget(pushOptions.target());
        else
            pushOptionsPtr->target = NFCPushTarget::ANY;

        if (pushOptions.hasTimeout())
            pushOptionsPtr->timeout = pushOptions.timeout();
        else
            pushOptionsPtr->timeout = std::numeric_limits<double>::infinity();

        if (pushOptions.hasIgnoreRead())
            pushOptionsPtr->ignoreRead = pushOptions.ignoreRead();
        else
            pushOptionsPtr->ignoreRead = true;

        return pushOptionsPtr;
    }
};

} // namespace mojo

namespace blink {
namespace {

bool isValidTextRecord(const NFCRecord& record)
{
    v8::Local<v8::Value> value = record.data().v8Value();
    if (!value->IsString() && !(value->IsNumber() && !std::isnan(value.As<v8::Number>()->Value())))
        return false;

    if (record.hasMediaType() && !record.mediaType().startsWith(kPlainTextMimePrefix, TextCaseInsensitive))
        return false;

    return true;
}

bool isValidURLRecord(const NFCRecord& record)
{
    if (!record.data().v8Value()->IsString())
        return false;

    blink::V8StringResource<> stringResource = record.data().v8Value();
    if (!stringResource.prepare())
        return false;

    return KURL(KURL(), stringResource).isValid();
}

bool isValidJSONRecord(const NFCRecord& record)
{
    v8::Local<v8::Value> value = record.data().v8Value();
    if (!value->IsObject() || value->IsArrayBuffer())
        return false;

    if (record.hasMediaType() && !record.mediaType().startsWith(kJsonMimePrefix, TextCaseInsensitive))
        return false;

    return true;
}

bool isValidOpaqueRecord(const NFCRecord& record)
{
    return record.data().v8Value()->IsArrayBuffer();
}

bool isValidNFCRecord(const NFCRecord& record)
{
    nfc::NFCRecordType type;
    if (record.hasRecordType()) {
        type = mojo::toNFCRecordType(record.recordType());
    } else {
        type = mojo::deduceRecordTypeFromDataType(record);

        // https://w3c.github.io/web-nfc/#creating-web-nfc-message
        // If NFCRecord.recordType is not set and record type cannot be deduced
        // from NFCRecord.data, reject promise with SyntaxError.
        if (type == nfc::NFCRecordType::EMPTY)
            return false;
    }

    // Non-empty records must have data.
    if (!record.hasData() && (type != nfc::NFCRecordType::EMPTY))
        return false;

    switch (type) {
    case nfc::NFCRecordType::TEXT:
        return isValidTextRecord(record);
    case nfc::NFCRecordType::URL:
        return isValidURLRecord(record);
    case nfc::NFCRecordType::JSON:
        return isValidJSONRecord(record);
    case nfc::NFCRecordType::OPAQUE_RECORD:
        return isValidOpaqueRecord(record);
    case nfc::NFCRecordType::EMPTY:
        return !record.hasData() && record.mediaType().isEmpty();
    }

    NOTREACHED();
    return false;
}

DOMException* isValidNFCRecordArray(const HeapVector<NFCRecord>& records)
{
    // https://w3c.github.io/web-nfc/#the-push-method
    // If NFCMessage.data is empty, reject promise with SyntaxError
    if (records.isEmpty())
        return DOMException::create(SyntaxError);

    for (const auto& record : records) {
        if (!isValidNFCRecord(record))
            return DOMException::create(SyntaxError);
    }

    return nullptr;
}

DOMException* isValidNFCPushMessage(const NFCPushMessage& message)
{
    if (!message.isNFCMessage() && !message.isString() && !message.isArrayBuffer())
        return DOMException::create(TypeMismatchError);

    if (message.isNFCMessage()) {
        if (!message.getAsNFCMessage().hasData())
            return DOMException::create(TypeMismatchError);

        return isValidNFCRecordArray(message.getAsNFCMessage().data());
    }

    return nullptr;
}

bool setURL(const String& origin, nfc::NFCMessagePtr& message)
{
    KURL originURL(ParsedURLString, origin);

    if (!message->url.isEmpty() && originURL.canSetPathname()) {
        originURL.setPath(message->url);
    }

    message->url = originURL;
    return originURL.isValid();
}

} // anonymous namespace

NFC::NFC(LocalFrame* frame)
    : PageLifecycleObserver(frame->page())
    , ContextLifecycleObserver(frame->document())
    , m_client(this)
{
    ThreadState::current()->registerPreFinalizer(this);
    frame->serviceRegistry()->connectToRemoteService(mojo::GetProxy(&m_nfc));
    m_nfc.set_connection_error_handler(createBaseCallback(WTF::bind(&NFC::OnConnectionError, wrapWeakPersistent(this))));
    m_nfc->SetClient(m_client.CreateInterfacePtrAndBind());
}

NFC* NFC::create(LocalFrame* frame)
{
    NFC* nfc = new NFC(frame);
    return nfc;
}

NFC::~NFC()
{
    // |m_nfc| may hold persistent handle to |this| object, therefore, there
    // should be no more outstanding requests when NFC object is destructed.
    DCHECK(m_requests.isEmpty());
}

void NFC::dispose()
{
    m_client.Close();
}

void NFC::contextDestroyed()
{
    m_nfc.reset();
    m_requests.clear();
}

// https://w3c.github.io/web-nfc/#writing-or-pushing-content
ScriptPromise NFC::push(ScriptState* scriptState, const NFCPushMessage& pushMessage, const NFCPushOptions& options)
{
    String errorMessage;
    if (!scriptState->getExecutionContext()->isSecureContext(errorMessage))
        return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(SecurityError, errorMessage));

    DOMException* exception = isValidNFCPushMessage(pushMessage);
    if (exception)
        return ScriptPromise::rejectWithDOMException(scriptState, exception);

    if (!m_nfc)
        return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(NotSupportedError));

    nfc::NFCMessagePtr message = nfc::NFCMessage::From(pushMessage);
    if (!message)
        return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(SyntaxError));

    if (!setURL(scriptState->getExecutionContext()->getSecurityOrigin()->toString(), message))
        return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(SyntaxError));

    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    m_requests.add(resolver);
    auto callback = createBaseCallback(WTF::bind(&NFC::OnRequestCompleted, wrapPersistent(this), wrapPersistent(resolver)));
    m_nfc->Push(std::move(message), nfc::NFCPushOptions::From(options), callback);

    return resolver->promise();
}

ScriptPromise NFC::cancelPush(ScriptState* scriptState, const String& target)
{
    String errorMessage;
    if (!scriptState->getExecutionContext()->isSecureContext(errorMessage))
        return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(SecurityError, errorMessage));

    if (!m_nfc)
        return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(NotSupportedError));

    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    m_requests.add(resolver);
    auto callback = createBaseCallback(WTF::bind(&NFC::OnRequestCompleted, wrapPersistent(this), wrapPersistent(resolver)));
    m_nfc->CancelPush(mojo::toNFCPushTarget(target), callback);

    return resolver->promise();
}

ScriptPromise NFC::watch(ScriptState* scriptState, MessageCallback* callback, const NFCWatchOptions& options)
{
    // TODO(shalamov): To be implemented.
    return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(NotSupportedError));
}

ScriptPromise NFC::cancelWatch(ScriptState* scriptState, long id)
{
    // TODO(shalamov): To be implemented.
    return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(NotSupportedError));
}

ScriptPromise NFC::cancelWatch(ScriptState* scriptState)
{
    // TODO(shalamov): To be implemented.
    return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(NotSupportedError));
}

void NFC::pageVisibilityChanged()
{
    // If service is not initialized, there cannot be any pending NFC activities
    if (!m_nfc)
        return;

    // NFC operations should be suspended.
    // https://w3c.github.io/web-nfc/#nfc-suspended
    if (page()->visibilityState() == PageVisibilityStateVisible)
        m_nfc->ResumeNFCOperations();
    else
        m_nfc->SuspendNFCOperations();
}

void NFC::OnRequestCompleted(ScriptPromiseResolver* resolver, nfc::NFCErrorPtr error)
{
    if (!m_requests.contains(resolver))
        return;

    m_requests.remove(resolver);
    if (error.is_null())
        resolver->resolve();
    else
        resolver->reject(NFCError::take(resolver, error->error_type));
}

void NFC::OnConnectionError()
{
    m_nfc.reset();

    // If NFCService is not available or disappears when NFC hardware is
    // disabled, reject promise with NotSupportedError exception.
    for (ScriptPromiseResolver* resolver : m_requests)
        resolver->reject(NFCError::take(resolver, nfc::NFCErrorType::NOT_SUPPORTED));

    m_requests.clear();
}

void NFC::OnWatch(mojo::WTFArray<uint32_t> ids, nfc::NFCMessagePtr)
{
    // TODO(shalamov): Not implemented.
}

DEFINE_TRACE(NFC)
{
    PageLifecycleObserver::trace(visitor);
    ContextLifecycleObserver::trace(visitor);
    visitor->trace(m_requests);
}

} // namespace blink

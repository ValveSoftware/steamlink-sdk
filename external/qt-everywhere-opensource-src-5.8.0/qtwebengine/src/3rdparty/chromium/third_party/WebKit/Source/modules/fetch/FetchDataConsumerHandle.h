// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FetchDataConsumerHandle_h
#define FetchDataConsumerHandle_h

#include "modules/ModulesExport.h"
#include "platform/blob/BlobData.h"
#include "platform/network/EncodedFormData.h"
#include "public/platform/WebDataConsumerHandle.h"
#include "wtf/Forward.h"
#include "wtf/PassRefPtr.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

// This is an interface class that adds Reader's blobDataHandle() to
// WebDataConsumerHandle.
// This class works not very well with Oilpan: As this class is not garbage
// collected while many clients or related objects may be, it is very easy
// to create a reference cycle. When an client is garbage collected, making
// the client own the handle is the right way.
class MODULES_EXPORT FetchDataConsumerHandle : public WebDataConsumerHandle {
    USING_FAST_MALLOC(FetchDataConsumerHandle);
public:
    class Reader : public WebDataConsumerHandle::Reader {
        USING_FAST_MALLOC(Reader);
    public:
        enum BlobSizePolicy {
            // The returned blob must have a valid size (i.e. != kuint64max).
            DisallowBlobWithInvalidSize,
            // The returned blob can have an invalid size.
            AllowBlobWithInvalidSize
        };

        // The following "draining" functions drain and return the data. They
        // can return null when it is impossible to drain, and in such cases the
        // operation is no-op.
        // When they return a non-null value, the data will be drained and
        // subsequent calls of read() / beginRead() will return |Done|.
        // Moreover subsequent calls of draining functions will return null.
        //
        // A draining function will return null when any data was read, i.e.
        // read() / bedingRead() was called or any draining functions returned
        // a non-null value. There is one exception: when read() is called
        // with zero |size| and its return value is Ok or ShouldWait, no data
        // is regarded as read.

        // Drains the data as a BlobDataHandle.
        // The returned non-null blob handle contains bytes that would be read
        // through WebDataConsumerHandle::Reader APIs without calling this
        // function.
        // When |policy| is DisallowBlobWithInvalidSize, this function doesn't
        // return a non-null blob handle with unspecified size.
        // The type of the returned handle may not be meaningful.
        virtual PassRefPtr<BlobDataHandle> drainAsBlobDataHandle(BlobSizePolicy = DisallowBlobWithInvalidSize) { return nullptr; }

        // Drains the data as an EncodedFormData.
        // This function returns a non-null form data when the handle is made
        // from an EncodedFormData-convertible value.
        virtual PassRefPtr<EncodedFormData> drainAsFormData() { return nullptr; }
    };

    // TODO(yhirano): obtainReader() is currently non-virtual override, and
    // will be changed into virtual override when we can use unique_ptr in
    // Blink.
    std::unique_ptr<Reader> obtainReader(Client* client) { return wrapUnique(obtainReaderInternal(client)); }

private:
    Reader* obtainReaderInternal(Client*) override = 0;
};

} // namespace blink

#endif // FetchDataConsumerHandle_h

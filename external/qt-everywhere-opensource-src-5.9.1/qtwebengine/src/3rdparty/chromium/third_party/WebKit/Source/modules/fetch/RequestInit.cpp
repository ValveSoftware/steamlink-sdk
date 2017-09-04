// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/fetch/RequestInit.h"

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/V8ArrayBuffer.h"
#include "bindings/core/v8/V8ArrayBufferView.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8Blob.h"
#include "bindings/core/v8/V8FormData.h"
#include "bindings/core/v8/V8URLSearchParams.h"
#include "bindings/modules/v8/V8PasswordCredential.h"
#include "core/dom/URLSearchParams.h"
#include "core/fileapi/Blob.h"
#include "core/html/FormData.h"
#include "modules/fetch/BlobBytesConsumer.h"
#include "modules/fetch/FormDataBytesConsumer.h"
#include "modules/fetch/Headers.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/blob/BlobData.h"
#include "platform/network/EncodedFormData.h"
#include "platform/weborigin/ReferrerPolicy.h"

namespace blink {

RequestInit::RequestInit(ExecutionContext* context,
                         const Dictionary& options,
                         ExceptionState& exceptionState)
    : areAnyMembersSet(false) {
  areAnyMembersSet |= DictionaryHelper::get(options, "method", method);
  areAnyMembersSet |= DictionaryHelper::get(options, "headers", headers);
  if (!headers) {
    Vector<Vector<String>> headersVector;
    if (DictionaryHelper::get(options, "headers", headersVector,
                              exceptionState)) {
      headers = Headers::create(headersVector, exceptionState);
      areAnyMembersSet = true;
    } else {
      areAnyMembersSet |=
          DictionaryHelper::get(options, "headers", headersDictionary);
    }
  }
  areAnyMembersSet |= DictionaryHelper::get(options, "mode", mode);
  areAnyMembersSet |= DictionaryHelper::get(options, "redirect", redirect);
  AtomicString referrerString;
  bool isReferrerStringSet =
      DictionaryHelper::get(options, "referrer", referrerString);
  areAnyMembersSet |= isReferrerStringSet;
  areAnyMembersSet |= DictionaryHelper::get(options, "integrity", integrity);
  AtomicString referrerPolicyString;
  bool isReferrerPolicySet =
      DictionaryHelper::get(options, "referrerPolicy", referrerPolicyString);
  areAnyMembersSet |= isReferrerPolicySet;

  v8::Local<v8::Value> v8Body;
  bool isBodySet = DictionaryHelper::get(options, "body", v8Body);
  areAnyMembersSet |= isBodySet;

  v8::Local<v8::Value> v8Credential;
  bool isCredentialSet =
      DictionaryHelper::get(options, "credentials", v8Credential);
  areAnyMembersSet |= isCredentialSet;

  if (areAnyMembersSet) {
    // A part of the Request constructor algorithm is performed here. See
    // the comments in the Request constructor code for the detail.

    // We need to use "about:client" instead of |clientReferrerString|,
    // because "about:client" => |clientReferrerString| conversion is done
    // in Request::createRequestWithRequestOrString.
    referrer = Referrer("about:client", ReferrerPolicyDefault);
    if (isReferrerStringSet)
      referrer.referrer = referrerString;
    if (isReferrerPolicySet) {
      if (referrerPolicyString == "") {
        referrer.referrerPolicy = ReferrerPolicyDefault;
      } else if (referrerPolicyString == "no-referrer") {
        referrer.referrerPolicy = ReferrerPolicyNever;
      } else if (referrerPolicyString == "no-referrer-when-downgrade") {
        referrer.referrerPolicy = ReferrerPolicyNoReferrerWhenDowngrade;
      } else if (referrerPolicyString == "origin") {
        referrer.referrerPolicy = ReferrerPolicyOrigin;
      } else if (referrerPolicyString == "origin-when-cross-origin") {
        referrer.referrerPolicy = ReferrerPolicyOriginWhenCrossOrigin;
      } else if (referrerPolicyString == "unsafe-url") {
        referrer.referrerPolicy = ReferrerPolicyAlways;
      } else if (referrerPolicyString ==
                     "no-referrer-when-downgrade-origin-when-cross-origin" &&
                 RuntimeEnabledFeatures::reducedReferrerGranularityEnabled()) {
        referrer.referrerPolicy =
            ReferrerPolicyNoReferrerWhenDowngradeOriginWhenCrossOrigin;
      } else {
        exceptionState.throwTypeError("Invalid referrer policy");
        return;
      }
    }
  }

  v8::Isolate* isolate = toIsolate(context);
  if (isCredentialSet) {
    if (V8PasswordCredential::hasInstance(v8Credential, isolate)) {
      // TODO(mkwst): According to the spec, we'd serialize this once we touch
      // the network. We're serializing it here, ahead of time, because lifetime
      // issues around ResourceRequest make it pretty difficult to pass a
      // PasswordCredential around at the platform level, and the hop between
      // the browser and renderer processes to deal with service workers is
      // equally painful. There should be no developer-visible difference in
      // behavior with this option, except that the `Content-Type` header will
      // be set early. That seems reasonable.
      PasswordCredential* credential =
          V8PasswordCredential::toImpl(v8Credential.As<v8::Object>());
      attachedCredential = credential->encodeFormData(contentType);
      credentials = "password";
    } else if (v8Credential->IsString()) {
      credentials = toUSVString(isolate, v8Credential, exceptionState);
    }
  }

  if (attachedCredential.get() || !isBodySet || v8Body->IsUndefined() ||
      v8Body->IsNull())
    return;

  if (v8Body->IsArrayBuffer()) {
    body = new FormDataBytesConsumer(
        V8ArrayBuffer::toImpl(v8Body.As<v8::Object>()));
  } else if (v8Body->IsArrayBufferView()) {
    body = new FormDataBytesConsumer(
        V8ArrayBufferView::toImpl(v8Body.As<v8::Object>()));
  } else if (V8Blob::hasInstance(v8Body, isolate)) {
    RefPtr<BlobDataHandle> blobDataHandle =
        V8Blob::toImpl(v8Body.As<v8::Object>())->blobDataHandle();
    contentType = blobDataHandle->type();
    body = new BlobBytesConsumer(context, blobDataHandle.release());
  } else if (V8FormData::hasInstance(v8Body, isolate)) {
    RefPtr<EncodedFormData> formData =
        V8FormData::toImpl(v8Body.As<v8::Object>())->encodeMultiPartFormData();
    // Here we handle formData->boundary() as a C-style string. See
    // FormDataEncoder::generateUniqueBoundaryString.
    contentType = AtomicString("multipart/form-data; boundary=") +
                  formData->boundary().data();
    body = new FormDataBytesConsumer(context, formData.release());
  } else if (V8URLSearchParams::hasInstance(v8Body, isolate)) {
    RefPtr<EncodedFormData> formData =
        V8URLSearchParams::toImpl(v8Body.As<v8::Object>())->toEncodedFormData();
    contentType =
        AtomicString("application/x-www-form-urlencoded;charset=UTF-8");
    body = new FormDataBytesConsumer(context, formData.release());
  } else if (v8Body->IsString()) {
    contentType = "text/plain;charset=UTF-8";
    body =
        new FormDataBytesConsumer(toUSVString(isolate, v8Body, exceptionState));
  }
}

}  // namespace blink

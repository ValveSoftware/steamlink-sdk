// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/cdm_promise.h"

#include "base/bind.h"
#include "base/logging.h"

namespace media {

CdmPromise::CdmPromise() : is_pending_(true) {
}

CdmPromise::CdmPromise(PromiseRejectedCB reject_cb)
    : reject_cb_(reject_cb), is_pending_(true) {
  DCHECK(!reject_cb_.is_null());
}

CdmPromise::~CdmPromise() {
  DCHECK(!is_pending_);
}

void CdmPromise::reject(MediaKeys::Exception exception_code,
                        uint32 system_code,
                        const std::string& error_message) {
  DCHECK(is_pending_);
  is_pending_ = false;
  reject_cb_.Run(exception_code, system_code, error_message);
}

template <typename T>
CdmPromiseTemplate<T>::CdmPromiseTemplate(
    base::Callback<void(const T&)> resolve_cb,
    PromiseRejectedCB reject_cb)
    : CdmPromise(reject_cb), resolve_cb_(resolve_cb) {
  DCHECK(!resolve_cb_.is_null());
}

template <typename T>
CdmPromiseTemplate<T>::~CdmPromiseTemplate() {
  DCHECK(!is_pending_);
}

template <typename T>
void CdmPromiseTemplate<T>::resolve(const T& result) {
  DCHECK(is_pending_);
  is_pending_ = false;
  resolve_cb_.Run(result);
}

CdmPromiseTemplate<void>::CdmPromiseTemplate(base::Callback<void()> resolve_cb,
                                             PromiseRejectedCB reject_cb)
    : CdmPromise(reject_cb), resolve_cb_(resolve_cb) {
  DCHECK(!resolve_cb_.is_null());
}

CdmPromiseTemplate<void>::CdmPromiseTemplate() {
}

CdmPromiseTemplate<void>::~CdmPromiseTemplate() {
  DCHECK(!is_pending_);
}

void CdmPromiseTemplate<void>::resolve() {
  DCHECK(is_pending_);
  is_pending_ = false;
  resolve_cb_.Run();
}

// Explicit template instantiation for the Promises needed.
template class MEDIA_EXPORT CdmPromiseTemplate<std::string>;

}  // namespace media

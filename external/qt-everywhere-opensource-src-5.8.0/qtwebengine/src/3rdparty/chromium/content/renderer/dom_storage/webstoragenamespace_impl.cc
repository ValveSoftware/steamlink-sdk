// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/dom_storage/webstoragenamespace_impl.h"

#include "base/logging.h"
#include "content/common/dom_storage/dom_storage_types.h"
#include "content/renderer/dom_storage/webstoragearea_impl.h"
#include "third_party/WebKit/public/platform/URLConversion.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "url/gurl.h"

using blink::WebStorageArea;
using blink::WebStorageNamespace;
using blink::WebString;

namespace content {

WebStorageNamespaceImpl::WebStorageNamespaceImpl()
    : namespace_id_(kLocalStorageNamespaceId) {
}

WebStorageNamespaceImpl::WebStorageNamespaceImpl(int64_t namespace_id)
    : namespace_id_(namespace_id) {
  DCHECK_NE(kInvalidSessionStorageNamespaceId, namespace_id);
}

WebStorageNamespaceImpl::~WebStorageNamespaceImpl() {
}

WebStorageArea* WebStorageNamespaceImpl::createStorageArea(
    const WebString& origin) {
  return new WebStorageAreaImpl(namespace_id_, blink::WebStringToGURL(origin));
}

WebStorageNamespace* WebStorageNamespaceImpl::copy() {
  // By returning NULL, we're telling WebKit to lazily fetch it the next time
  // session storage is used.  In the WebViewClient::createView, we do the
  // book-keeping necessary to make it a true copy-on-write despite not doing
  // anything here, now.
  return NULL;
}

bool WebStorageNamespaceImpl::isSameNamespace(
    const WebStorageNamespace& other) const {
  const WebStorageNamespaceImpl* other_impl =
      static_cast<const WebStorageNamespaceImpl*>(&other);
  return namespace_id_ == other_impl->namespace_id_;
}

}  // namespace content

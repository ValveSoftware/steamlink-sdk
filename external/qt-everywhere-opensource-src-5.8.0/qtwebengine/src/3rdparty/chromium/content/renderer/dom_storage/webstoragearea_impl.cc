// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/dom_storage/webstoragearea_impl.h"

#include "base/lazy_instance.h"
#include "base/metrics/histogram.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "content/common/dom_storage/dom_storage_messages.h"
#include "content/renderer/dom_storage/dom_storage_cached_area.h"
#include "content/renderer/dom_storage/dom_storage_dispatcher.h"
#include "content/renderer/render_thread_impl.h"
#include "third_party/WebKit/public/platform/WebURL.h"

using blink::WebString;
using blink::WebURL;

namespace content {

namespace {
typedef IDMap<WebStorageAreaImpl> AreaImplMap;
base::LazyInstance<AreaImplMap>::Leaky
    g_all_areas_map = LAZY_INSTANCE_INITIALIZER;

DomStorageDispatcher* dispatcher() {
  return RenderThreadImpl::current()->dom_storage_dispatcher();
}
}  // namespace

// static
WebStorageAreaImpl* WebStorageAreaImpl::FromConnectionId(int id) {
  return g_all_areas_map.Pointer()->Lookup(id);
}

WebStorageAreaImpl::WebStorageAreaImpl(int64_t namespace_id, const GURL& origin)
    : connection_id_(g_all_areas_map.Pointer()->Add(this)),
      cached_area_(dispatcher()->OpenCachedArea(connection_id_,
                                                namespace_id,
                                                origin)) {}

WebStorageAreaImpl::~WebStorageAreaImpl() {
  g_all_areas_map.Pointer()->Remove(connection_id_);
  if (dispatcher())
    dispatcher()->CloseCachedArea(connection_id_, cached_area_.get());
}

unsigned WebStorageAreaImpl::length() {
  return cached_area_->GetLength(connection_id_);
}

WebString WebStorageAreaImpl::key(unsigned index) {
  return cached_area_->GetKey(connection_id_, index);
}

WebString WebStorageAreaImpl::getItem(const WebString& key) {
  return cached_area_->GetItem(connection_id_, key);
}

void WebStorageAreaImpl::setItem(
    const WebString& key, const WebString& value, const WebURL& page_url,
    WebStorageArea::Result& result) {
  if (!cached_area_->SetItem(connection_id_, key, value, page_url))
    result = ResultBlockedByQuota;
  else
    result = ResultOK;
}

void WebStorageAreaImpl::removeItem(
    const WebString& key, const WebURL& page_url) {
  cached_area_->RemoveItem(connection_id_, key, page_url);
}

void WebStorageAreaImpl::clear(const WebURL& page_url) {
  cached_area_->Clear(connection_id_, page_url);
}

}  // namespace content

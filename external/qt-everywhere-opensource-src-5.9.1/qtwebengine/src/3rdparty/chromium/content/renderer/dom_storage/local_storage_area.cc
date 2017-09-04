// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/dom_storage/local_storage_area.h"

#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "third_party/WebKit/public/platform/WebURL.h"

using blink::WebString;
using blink::WebURL;

namespace content {

LocalStorageArea::LocalStorageArea(
    scoped_refptr<LocalStorageCachedArea> cached_area)
    : cached_area_(std::move(cached_area)),
      id_(base::Uint64ToString(base::RandUint64())) {
  cached_area_->AreaCreated(this);
}

LocalStorageArea::~LocalStorageArea() {
  cached_area_->AreaDestroyed(this);
}

unsigned LocalStorageArea::length() {
  return cached_area_->GetLength();
}

WebString LocalStorageArea::key(unsigned index) {
  return cached_area_->GetKey(index);
}

WebString LocalStorageArea::getItem(const WebString& key) {
  return cached_area_->GetItem(key);
}

void LocalStorageArea::setItem(
    const WebString& key, const WebString& value, const WebURL& page_url,
    WebStorageArea::Result& result) {
  if (!cached_area_->SetItem(key, value, page_url, id_))
    result = ResultBlockedByQuota;
  else
    result = ResultOK;
}

void LocalStorageArea::removeItem(
    const WebString& key, const WebURL& page_url) {
  cached_area_->RemoveItem(key, page_url, id_);
}

void LocalStorageArea::clear(const WebURL& page_url) {
  cached_area_->Clear(page_url, id_);
}

}  // namespace content

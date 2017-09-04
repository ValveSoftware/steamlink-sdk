// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_DOM_STORAGE_LOCAL_STORAGE_AREA_H_
#define CONTENT_RENDERER_DOM_STORAGE_LOCAL_STORAGE_AREA_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/renderer/dom_storage/local_storage_cached_area.h"
#include "third_party/WebKit/public/platform/WebStorageArea.h"

namespace content {

// There could be n instances of this class for the same origin in a renderer
// process. It delegates to the one LocalStorageCachedArea instance in a process
// for a given origin.
class LocalStorageArea : public blink::WebStorageArea {
 public:
  explicit LocalStorageArea(scoped_refptr<LocalStorageCachedArea> cached_area);
  ~LocalStorageArea() override;

  // blink::WebStorageArea:
  unsigned length() override;
  blink::WebString key(unsigned index) override;
  blink::WebString getItem(const blink::WebString& key) override;
  void setItem(const blink::WebString& key,
               const blink::WebString& value,
               const blink::WebURL& page_url,
               WebStorageArea::Result& result) override;
  void removeItem(const blink::WebString& key,
                  const blink::WebURL& page_url) override;
  void clear(const blink::WebURL& url) override;

  const std::string& id() const { return id_; }

 private:
  scoped_refptr<LocalStorageCachedArea> cached_area_;
  // A globally unique identifier for this storage area. It's used to pass the
  // source storage area, if any, in mutation events.
  std::string id_;

  DISALLOW_COPY_AND_ASSIGN(LocalStorageArea);
};

}  // namespace content

#endif  // CONTENT_RENDERER_DOM_STORAGE_LOCAL_STORAGE_AREA_H_

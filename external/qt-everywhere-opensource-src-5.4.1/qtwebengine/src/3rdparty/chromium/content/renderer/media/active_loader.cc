// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/active_loader.h"

#include "content/renderer/media/buffered_resource_loader.h"
#include "third_party/WebKit/public/platform/WebURLLoader.h"

namespace content {

ActiveLoader::ActiveLoader(scoped_ptr<blink::WebURLLoader> loader)
    : loader_(loader.Pass()),
      deferred_(false) {
}

ActiveLoader::~ActiveLoader() {
  loader_->cancel();
}

void ActiveLoader::SetDeferred(bool deferred) {
  deferred_ = deferred;
  loader_->setDefersLoading(deferred);
}

}  // namespace content

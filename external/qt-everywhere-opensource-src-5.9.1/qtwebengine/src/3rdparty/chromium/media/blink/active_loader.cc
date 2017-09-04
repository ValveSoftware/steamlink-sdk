// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/blink/active_loader.h"

#include <utility>

#include "third_party/WebKit/public/web/WebAssociatedURLLoader.h"

namespace media {

ActiveLoader::ActiveLoader(
    std::unique_ptr<blink::WebAssociatedURLLoader> loader)
    : loader_(std::move(loader)), deferred_(false) {}

ActiveLoader::~ActiveLoader() {
  loader_->cancel();
}

void ActiveLoader::SetDeferred(bool deferred) {
  deferred_ = deferred;
  loader_->setDefersLoading(deferred);
}

}  // namespace media

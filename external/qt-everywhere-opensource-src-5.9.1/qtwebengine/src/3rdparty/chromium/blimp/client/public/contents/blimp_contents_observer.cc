// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/public/contents/blimp_contents_observer.h"

#include "blimp/client/public/contents/blimp_contents.h"

namespace blimp {
namespace client {

BlimpContentsObserver::BlimpContentsObserver(BlimpContents* blimp_contents)
    : contents_(blimp_contents) {
  blimp_contents->AddObserver(this);
}

BlimpContentsObserver::~BlimpContentsObserver() {
  if (contents_)
    contents_->RemoveObserver(this);
}

void BlimpContentsObserver::BlimpContentsDying() {
  DCHECK(contents_);
  OnContentsDestroyed();
  contents_ = nullptr;
}

}  // namespace client
}  // namespace blimp

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/WebURLLoader.h"

#include "platform/WebTaskRunner.h"

namespace blink {

void WebURLLoader::setLoadingTaskRunner(WebTaskRunner* taskRunner) {
  setLoadingTaskRunner(taskRunner->toSingleThreadTaskRunner());
}

}  // namespace blink

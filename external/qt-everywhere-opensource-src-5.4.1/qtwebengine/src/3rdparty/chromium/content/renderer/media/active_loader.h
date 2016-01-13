// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_ACTIVE_LOADER_H_
#define CONTENT_RENDERER_MEDIA_ACTIVE_LOADER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

namespace blink {
class WebURLLoader;
}

namespace content {

// Wraps an active WebURLLoader with some additional state.
//
// Handles deferring and deletion of loaders.
class ActiveLoader {
 public:
  // Creates an ActiveLoader with the given loader. It is assumed that the
  // initial state of |loader| is loading and not deferred.
  explicit ActiveLoader(scoped_ptr<blink::WebURLLoader> loader);
  ~ActiveLoader();

  // Starts or stops deferring the resource load.
  void SetDeferred(bool deferred);
  bool deferred() { return deferred_; }

 private:
  friend class BufferedDataSourceTest;

  scoped_ptr<blink::WebURLLoader> loader_;
  bool deferred_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ActiveLoader);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_ACTIVE_LOADER_H_

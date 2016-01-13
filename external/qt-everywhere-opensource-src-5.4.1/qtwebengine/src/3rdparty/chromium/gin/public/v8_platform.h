// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_PUBLIC_V8_PLATFORM_H_
#define GIN_PUBLIC_V8_PLATFORM_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/lazy_instance.h"
#include "gin/gin_export.h"
#include "v8/include/v8-platform.h"

namespace gin {

// A v8::Platform implementation to use with gin.
class GIN_EXPORT V8Platform : public NON_EXPORTED_BASE(v8::Platform) {
 public:
  static V8Platform* Get();

  // v8::Platform implementation.
  virtual void CallOnBackgroundThread(
      v8::Task* task,
      v8::Platform::ExpectedRuntime expected_runtime) OVERRIDE;
  virtual void CallOnForegroundThread(v8::Isolate* isolate,
                                      v8::Task* task) OVERRIDE;
 private:
  friend struct base::DefaultLazyInstanceTraits<V8Platform>;

  V8Platform();
  virtual ~V8Platform();

  DISALLOW_COPY_AND_ASSIGN(V8Platform);
};

}  // namespace gin

#endif  // GIN_PUBLIC_V8_PLATFORM_H_

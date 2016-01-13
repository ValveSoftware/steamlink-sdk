// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_COMPOSITOR_BINDINGS_WEB_TRANSFORM_OPERATIONS_IMPL_H_
#define CONTENT_RENDERER_COMPOSITOR_BINDINGS_WEB_TRANSFORM_OPERATIONS_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "cc/animation/transform_operations.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebTransformOperations.h"

namespace content {

class WebTransformOperationsImpl : public blink::WebTransformOperations {
 public:
  CONTENT_EXPORT WebTransformOperationsImpl();
  virtual ~WebTransformOperationsImpl();

  const cc::TransformOperations& AsTransformOperations() const;

  // Implementation of blink::WebTransformOperations methods
  virtual bool canBlendWith(const blink::WebTransformOperations& other) const;
  virtual void appendTranslate(double x, double y, double z);
  virtual void appendRotate(double x, double y, double z, double degrees);
  virtual void appendScale(double x, double y, double z);
  virtual void appendSkew(double x, double y);
  virtual void appendPerspective(double depth);
  virtual void appendMatrix(const SkMatrix44&);
  virtual void appendIdentity();
  virtual bool isIdentity() const;

 private:
  cc::TransformOperations transform_operations_;

  DISALLOW_COPY_AND_ASSIGN(WebTransformOperationsImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_COMPOSITOR_BINDINGS_WEB_TRANSFORM_OPERATIONS_IMPL_H_


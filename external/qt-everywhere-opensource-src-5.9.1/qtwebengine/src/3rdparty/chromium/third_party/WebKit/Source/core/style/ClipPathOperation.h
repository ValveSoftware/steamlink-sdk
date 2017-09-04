/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef ClipPathOperation_h
#define ClipPathOperation_h

#include "core/style/BasicShapes.h"
#include "core/svg/SVGElementProxy.h"
#include "platform/graphics/Path.h"
#include "wtf/PtrUtil.h"
#include "wtf/RefCounted.h"
#include "wtf/text/WTFString.h"
#include <memory>

namespace blink {

class SVGElement;
class SVGResourceClient;
class TreeScope;

class ClipPathOperation : public RefCounted<ClipPathOperation> {
 public:
  enum OperationType { REFERENCE, SHAPE };

  virtual ~ClipPathOperation() {}

  virtual bool operator==(const ClipPathOperation&) const = 0;
  bool operator!=(const ClipPathOperation& o) const { return !(*this == o); }

  virtual OperationType type() const = 0;
  bool isSameType(const ClipPathOperation& o) const {
    return o.type() == type();
  }

 protected:
  ClipPathOperation() {}
};

class ReferenceClipPathOperation final : public ClipPathOperation {
 public:
  static PassRefPtr<ReferenceClipPathOperation> create(
      const String& url,
      SVGElementProxy& elementProxy) {
    return adoptRef(new ReferenceClipPathOperation(url, elementProxy));
  }

  void addClient(SVGResourceClient*);
  void removeClient(SVGResourceClient*);

  SVGElement* findElement(TreeScope&) const;

  const String& url() const { return m_url; }

 private:
  bool operator==(const ClipPathOperation&) const override;
  OperationType type() const override { return REFERENCE; }

  ReferenceClipPathOperation(const String& url, SVGElementProxy& elementProxy)
      : m_elementProxy(&elementProxy), m_url(url) {}

  Persistent<SVGElementProxy> m_elementProxy;
  String m_url;
};

DEFINE_TYPE_CASTS(ReferenceClipPathOperation,
                  ClipPathOperation,
                  op,
                  op->type() == ClipPathOperation::REFERENCE,
                  op.type() == ClipPathOperation::REFERENCE);

class ShapeClipPathOperation final : public ClipPathOperation {
 public:
  static PassRefPtr<ShapeClipPathOperation> create(
      PassRefPtr<BasicShape> shape) {
    return adoptRef(new ShapeClipPathOperation(std::move(shape)));
  }

  const BasicShape* basicShape() const { return m_shape.get(); }
  bool isValid() const { return m_shape.get(); }
  const Path& path(const FloatRect& boundingRect) {
    ASSERT(m_shape);
    m_path.reset();
    m_path = wrapUnique(new Path);
    m_shape->path(*m_path, boundingRect);
    m_path->setWindRule(m_shape->getWindRule());
    return *m_path;
  }

 private:
  bool operator==(const ClipPathOperation&) const override;
  OperationType type() const override { return SHAPE; }

  ShapeClipPathOperation(PassRefPtr<BasicShape> shape) : m_shape(shape) {}

  RefPtr<BasicShape> m_shape;
  std::unique_ptr<Path> m_path;
};

DEFINE_TYPE_CASTS(ShapeClipPathOperation,
                  ClipPathOperation,
                  op,
                  op->type() == ClipPathOperation::SHAPE,
                  op.type() == ClipPathOperation::SHAPE);

inline bool ShapeClipPathOperation::operator==(
    const ClipPathOperation& o) const {
  if (!isSameType(o))
    return false;
  BasicShape* otherShape = toShapeClipPathOperation(o).m_shape.get();
  if (!m_shape.get() || !otherShape)
    return static_cast<bool>(m_shape.get()) == static_cast<bool>(otherShape);
  return *m_shape == *otherShape;
}

}  // namespace blink

#endif  // ClipPathOperation_h

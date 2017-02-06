// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSTranslation_h
#define CSSTranslation_h

#include "core/css/cssom/CSSLengthValue.h"
#include "core/css/cssom/CSSTransformComponent.h"

namespace blink {

class ExceptionState;

class CORE_EXPORT CSSTranslation final : public CSSTransformComponent {
    WTF_MAKE_NONCOPYABLE(CSSTranslation);
    DEFINE_WRAPPERTYPEINFO();
public:
    static CSSTranslation* create(CSSLengthValue* x, CSSLengthValue* y, ExceptionState&)
    {
        return new CSSTranslation(x, y, nullptr);
    }
    static CSSTranslation* create(CSSLengthValue* x, CSSLengthValue* y, CSSLengthValue* z, ExceptionState&);

    static CSSTranslation* fromCSSValue(const CSSFunctionValue& value) { return nullptr; }

    CSSLengthValue* x() const { return m_x; }
    CSSLengthValue* y() const { return m_y; }
    CSSLengthValue* z() const { return m_z; }

    TransformComponentType type() const override { return is2D() ? TranslationType : Translation3DType; }

    // TODO: Implement asMatrix for CSSTranslation.
    CSSMatrixTransformComponent* asMatrix() const override { return nullptr; }

    CSSFunctionValue* toCSSValue() const override;

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_x);
        visitor->trace(m_y);
        visitor->trace(m_z);
        CSSTransformComponent::trace(visitor);
    }

private:
    CSSTranslation(CSSLengthValue* x, CSSLengthValue* y, CSSLengthValue* z)
        : CSSTransformComponent()
        , m_x(x)
        , m_y(y)
        , m_z(z)
    { }

    bool is2D() const { return !m_z; }

    Member<CSSLengthValue> m_x;
    Member<CSSLengthValue> m_y;
    Member<CSSLengthValue> m_z;
};

} // namespace blink

#endif

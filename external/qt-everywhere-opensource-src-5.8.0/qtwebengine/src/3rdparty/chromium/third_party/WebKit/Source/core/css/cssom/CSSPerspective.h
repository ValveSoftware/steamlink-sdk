// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPerspective_h
#define CSSPerspective_h

#include "core/CoreExport.h"
#include "core/css/cssom/CSSLengthValue.h"
#include "core/css/cssom/CSSTransformComponent.h"

namespace blink {

class ExceptionState;

class CORE_EXPORT CSSPerspective : public CSSTransformComponent {
    WTF_MAKE_NONCOPYABLE(CSSPerspective);
    DEFINE_WRAPPERTYPEINFO();
public:
    static CSSPerspective* create(const CSSLengthValue*, ExceptionState&);
    static CSSPerspective* fromCSSValue(const CSSFunctionValue& value) { return nullptr; }

    // Bindings require a non const return value.
    CSSLengthValue* length() const { return const_cast<CSSLengthValue*>(m_length.get()); }

    TransformComponentType type() const override { return PerspectiveType; }

    // TODO: Implement asMatrix for CSSPerspective.
    CSSMatrixTransformComponent* asMatrix() const override { return nullptr; }

    CSSFunctionValue* toCSSValue() const override;

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_length);
        CSSTransformComponent::trace(visitor);
    }

private:
    CSSPerspective(const CSSLengthValue* length) : m_length(length) {}

    Member<const CSSLengthValue> m_length;
};

} // namespace blink

#endif

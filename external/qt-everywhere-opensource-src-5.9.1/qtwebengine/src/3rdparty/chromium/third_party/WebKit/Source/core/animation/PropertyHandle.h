// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PropertyHandle_h
#define PropertyHandle_h

#include "core/CSSPropertyNames.h"
#include "core/CoreExport.h"
#include "core/dom/QualifiedName.h"
#include "wtf/Allocator.h"

namespace blink {

// Represents the property of a PropertySpecificKeyframe.
class CORE_EXPORT PropertyHandle {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  explicit PropertyHandle(CSSPropertyID property,
                          bool isPresentationAttribute = false)
      : m_handleType(isPresentationAttribute ? HandlePresentationAttribute
                                             : HandleCSSProperty),
        m_cssProperty(property) {
    DCHECK_NE(property, CSSPropertyInvalid);
    DCHECK_NE(property, CSSPropertyVariable);
  }

  explicit PropertyHandle(const AtomicString& propertyName)
      : m_handleType(HandleCSSCustomProperty),
        m_svgAttribute(nullptr),
        m_propertyName(propertyName) {}

  explicit PropertyHandle(const QualifiedName& attributeName)
      : m_handleType(HandleSVGAttribute), m_svgAttribute(&attributeName) {}

  bool operator==(const PropertyHandle&) const;
  bool operator!=(const PropertyHandle& other) const {
    return !(*this == other);
  }

  unsigned hash() const;

  bool isCSSProperty() const {
    return m_handleType == HandleCSSProperty || isCSSCustomProperty();
  }
  CSSPropertyID cssProperty() const {
    DCHECK(isCSSProperty());
    return m_handleType == HandleCSSProperty ? m_cssProperty
                                             : CSSPropertyVariable;
  }

  bool isCSSCustomProperty() const {
    return m_handleType == HandleCSSCustomProperty;
  }
  const AtomicString& customPropertyName() const {
    DCHECK(isCSSCustomProperty());
    return m_propertyName;
  }

  bool isPresentationAttribute() const {
    return m_handleType == HandlePresentationAttribute;
  }
  CSSPropertyID presentationAttribute() const {
    DCHECK(isPresentationAttribute());
    return m_cssProperty;
  }

  bool isSVGAttribute() const { return m_handleType == HandleSVGAttribute; }
  const QualifiedName& svgAttribute() const {
    DCHECK(isSVGAttribute());
    return *m_svgAttribute;
  }

 private:
  enum HandleType {
    HandleEmptyValueForHashTraits,
    HandleDeletedValueForHashTraits,
    HandleCSSProperty,
    HandleCSSCustomProperty,
    HandlePresentationAttribute,
    HandleSVGAttribute,
  };

  explicit PropertyHandle(HandleType handleType)
      : m_handleType(handleType), m_svgAttribute(nullptr) {}

  static PropertyHandle emptyValueForHashTraits() {
    return PropertyHandle(HandleEmptyValueForHashTraits);
  }

  static PropertyHandle deletedValueForHashTraits() {
    return PropertyHandle(HandleDeletedValueForHashTraits);
  }

  bool isDeletedValueForHashTraits() {
    return m_handleType == HandleDeletedValueForHashTraits;
  }

  HandleType m_handleType;
  union {
    CSSPropertyID m_cssProperty;
    const QualifiedName* m_svgAttribute;
  };
  AtomicString m_propertyName;

  friend struct ::WTF::HashTraits<blink::PropertyHandle>;
};

}  // namespace blink

namespace WTF {

template <>
struct DefaultHash<blink::PropertyHandle> {
  struct Hash {
    STATIC_ONLY(Hash);
    static unsigned hash(const blink::PropertyHandle& handle) {
      return handle.hash();
    }

    static bool equal(const blink::PropertyHandle& a,
                      const blink::PropertyHandle& b) {
      return a == b;
    }

    static const bool safeToCompareToEmptyOrDeleted = true;
  };
};

template <>
struct HashTraits<blink::PropertyHandle>
    : SimpleClassHashTraits<blink::PropertyHandle> {
  static const bool needsDestruction = true;
  static void constructDeletedValue(blink::PropertyHandle& slot, bool) {
    new (NotNull, &slot) blink::PropertyHandle(
        blink::PropertyHandle::deletedValueForHashTraits());
  }
  static bool isDeletedValue(blink::PropertyHandle value) {
    return value.isDeletedValueForHashTraits();
  }

  static blink::PropertyHandle emptyValue() {
    return blink::PropertyHandle::emptyValueForHashTraits();
  }
};

}  // namespace WTF

#endif  // PropertyHandle_h

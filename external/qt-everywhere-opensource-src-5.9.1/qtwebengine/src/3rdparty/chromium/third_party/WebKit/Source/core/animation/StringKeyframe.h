// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StringKeyframe_h
#define StringKeyframe_h

#include "core/animation/Keyframe.h"
#include "core/css/StylePropertySet.h"

#include "wtf/HashMap.h"

namespace blink {

class StyleSheetContents;

// A specialization of Keyframe that associates user specified keyframe
// properties with either CSS properties or SVG attributes.
class StringKeyframe : public Keyframe {
 public:
  static PassRefPtr<StringKeyframe> create() {
    return adoptRef(new StringKeyframe);
  }

  void setCSSPropertyValue(const AtomicString& propertyName,
                           const String& value,
                           StyleSheetContents*);
  void setCSSPropertyValue(CSSPropertyID,
                           const String& value,
                           StyleSheetContents*);
  void setCSSPropertyValue(CSSPropertyID, const CSSValue&);
  void setPresentationAttributeValue(CSSPropertyID,
                                     const String& value,
                                     StyleSheetContents*);
  void setSVGAttributeValue(const QualifiedName&, const String& value);

  const CSSValue& cssPropertyValue(const PropertyHandle& property) const {
    int index = -1;
    if (property.isCSSCustomProperty())
      index =
          m_cssPropertyMap->findPropertyIndex(property.customPropertyName());
    else
      index = m_cssPropertyMap->findPropertyIndex(property.cssProperty());
    CHECK_GE(index, 0);
    return m_cssPropertyMap->propertyAt(static_cast<unsigned>(index)).value();
  }

  const CSSValue& presentationAttributeValue(CSSPropertyID property) const {
    int index = m_presentationAttributeMap->findPropertyIndex(property);
    CHECK_GE(index, 0);
    return m_presentationAttributeMap->propertyAt(static_cast<unsigned>(index))
        .value();
  }

  String svgPropertyValue(const QualifiedName& attributeName) const {
    return m_svgAttributeMap.get(&attributeName);
  }

  PropertyHandleSet properties() const override;

  class CSSPropertySpecificKeyframe
      : public Keyframe::PropertySpecificKeyframe {
   public:
    static PassRefPtr<CSSPropertySpecificKeyframe> create(
        double offset,
        PassRefPtr<TimingFunction> easing,
        const CSSValue* value,
        EffectModel::CompositeOperation composite) {
      return adoptRef(new CSSPropertySpecificKeyframe(offset, std::move(easing),
                                                      value, composite));
    }

    const CSSValue* value() const { return m_value.get(); }

    bool populateAnimatableValue(CSSPropertyID,
                                 Element&,
                                 const ComputedStyle& baseStyle,
                                 const ComputedStyle* parentStyle) const final;
    PassRefPtr<AnimatableValue> getAnimatableValue() const final {
      return m_animatableValueCache.get();
    }

    bool isNeutral() const final { return !m_value; }
    PassRefPtr<Keyframe::PropertySpecificKeyframe> neutralKeyframe(
        double offset,
        PassRefPtr<TimingFunction> easing) const final;

   private:
    CSSPropertySpecificKeyframe(double offset,
                                PassRefPtr<TimingFunction> easing,
                                const CSSValue* value,
                                EffectModel::CompositeOperation composite)
        : Keyframe::PropertySpecificKeyframe(offset,
                                             std::move(easing),
                                             composite),
          m_value(const_cast<CSSValue*>(value)) {}

    virtual PassRefPtr<Keyframe::PropertySpecificKeyframe> cloneWithOffset(
        double offset) const;
    bool isCSSPropertySpecificKeyframe() const override { return true; }

    // TODO(sashab): Make this a const CSSValue.
    Persistent<CSSValue> m_value;
    mutable RefPtr<AnimatableValue> m_animatableValueCache;
  };

  class SVGPropertySpecificKeyframe
      : public Keyframe::PropertySpecificKeyframe {
   public:
    static PassRefPtr<SVGPropertySpecificKeyframe> create(
        double offset,
        PassRefPtr<TimingFunction> easing,
        const String& value,
        EffectModel::CompositeOperation composite) {
      return adoptRef(new SVGPropertySpecificKeyframe(offset, std::move(easing),
                                                      value, composite));
    }

    const String& value() const { return m_value; }

    PassRefPtr<PropertySpecificKeyframe> cloneWithOffset(
        double offset) const final;

    PassRefPtr<AnimatableValue> getAnimatableValue() const final {
      return nullptr;
    }

    bool isNeutral() const final { return m_value.isNull(); }
    PassRefPtr<PropertySpecificKeyframe> neutralKeyframe(
        double offset,
        PassRefPtr<TimingFunction> easing) const final;

   private:
    SVGPropertySpecificKeyframe(double offset,
                                PassRefPtr<TimingFunction> easing,
                                const String& value,
                                EffectModel::CompositeOperation composite)
        : Keyframe::PropertySpecificKeyframe(offset,
                                             std::move(easing),
                                             composite),
          m_value(value) {}

    bool isSVGPropertySpecificKeyframe() const override { return true; }

    String m_value;
  };

 private:
  StringKeyframe()
      : m_cssPropertyMap(MutableStylePropertySet::create(HTMLStandardMode)),
        m_presentationAttributeMap(
            MutableStylePropertySet::create(HTMLStandardMode)) {}

  StringKeyframe(const StringKeyframe& copyFrom);

  PassRefPtr<Keyframe> clone() const override;
  PassRefPtr<Keyframe::PropertySpecificKeyframe> createPropertySpecificKeyframe(
      PropertyHandle) const override;

  bool isStringKeyframe() const override { return true; }

  Persistent<MutableStylePropertySet> m_cssPropertyMap;
  Persistent<MutableStylePropertySet> m_presentationAttributeMap;
  HashMap<const QualifiedName*, String> m_svgAttributeMap;
};

using CSSPropertySpecificKeyframe = StringKeyframe::CSSPropertySpecificKeyframe;
using SVGPropertySpecificKeyframe = StringKeyframe::SVGPropertySpecificKeyframe;

DEFINE_TYPE_CASTS(StringKeyframe,
                  Keyframe,
                  value,
                  value->isStringKeyframe(),
                  value.isStringKeyframe());
DEFINE_TYPE_CASTS(CSSPropertySpecificKeyframe,
                  Keyframe::PropertySpecificKeyframe,
                  value,
                  value->isCSSPropertySpecificKeyframe(),
                  value.isCSSPropertySpecificKeyframe());
DEFINE_TYPE_CASTS(SVGPropertySpecificKeyframe,
                  Keyframe::PropertySpecificKeyframe,
                  value,
                  value->isSVGPropertySpecificKeyframe(),
                  value.isSVGPropertySpecificKeyframe());

}  // namespace blink

#endif

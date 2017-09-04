// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SVGAnimatedHref_h
#define SVGAnimatedHref_h

#include "core/svg/SVGAnimatedString.h"

namespace blink {

// This is an "access wrapper" for the 'href' attribute. The object
// itself holds the value for 'href' in the null/default NS and wraps
// one for 'href' in the XLink NS. Both objects are added to an
// SVGElement's property map and hence any updates/synchronization/etc
// via the "attribute DOM" (setAttribute and friends) will operate on
// the independent objects, while users of an 'href' value will be
// using this interface (which essentially just selects either itself
// or the wrapped object and forwards the operation to it.)
class SVGAnimatedHref final : public SVGAnimatedString {
 public:
  static SVGAnimatedHref* create(SVGElement* contextElement);

  SVGString* currentValue();
  const SVGString* currentValue() const;

  String baseVal() override;
  void setBaseVal(const String&, ExceptionState&) override;
  String animVal() override;

  bool isSpecified() const {
    return SVGAnimatedString::isSpecified() || m_xlinkHref->isSpecified();
  }

  static bool isKnownAttribute(const QualifiedName&);
  void addToPropertyMap(SVGElement*);

  DECLARE_VIRTUAL_TRACE();

 private:
  explicit SVGAnimatedHref(SVGElement* contextElement);

  SVGAnimatedString* backingString();
  const SVGAnimatedString* backingString() const;
  bool useXLink() const;

  Member<SVGAnimatedString> m_xlinkHref;
};

}  // namespace blink

#endif  // SVGAnimatedHref_h

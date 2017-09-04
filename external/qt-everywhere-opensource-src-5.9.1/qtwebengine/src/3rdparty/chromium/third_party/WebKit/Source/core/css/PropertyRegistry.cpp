// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/PropertyRegistry.h"

namespace blink {

void PropertyRegistry::registerProperty(
    const AtomicString& name,
    const CSSSyntaxDescriptor& syntax,
    bool inherits,
    const CSSValue* initial,
    PassRefPtr<CSSVariableData> initialVariableData) {
  DCHECK(!registration(name));
  m_registrations.set(name, new Registration(syntax, inherits, initial,
                                             std::move(initialVariableData)));
}

const PropertyRegistry::Registration* PropertyRegistry::registration(
    const AtomicString& name) const {
  return m_registrations.get(name);
}

}  // namespace blink

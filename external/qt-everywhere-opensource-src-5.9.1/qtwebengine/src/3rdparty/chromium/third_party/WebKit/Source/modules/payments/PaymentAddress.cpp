// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/payments/PaymentAddress.h"

#include "bindings/core/v8/V8ObjectBuilder.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

PaymentAddress::PaymentAddress(
    payments::mojom::blink::PaymentAddressPtr address)
    : m_country(address->country),
      m_addressLine(address->address_line),
      m_region(address->region),
      m_city(address->city),
      m_dependentLocality(address->dependent_locality),
      m_postalCode(address->postal_code),
      m_sortingCode(address->sorting_code),
      m_languageCode(address->language_code),
      m_organization(address->organization),
      m_recipient(address->recipient),
      m_phone(address->phone) {
  if (!m_languageCode.isEmpty() && !address->script_code.isEmpty()) {
    StringBuilder builder;
    builder.append(m_languageCode);
    builder.append('-');
    builder.append(address->script_code);
    m_languageCode = builder.toString();
  }
}

PaymentAddress::~PaymentAddress() {}

ScriptValue PaymentAddress::toJSONForBinding(ScriptState* scriptState) const {
  V8ObjectBuilder result(scriptState);
  result.addString("country", country());
  result.add("addressLine", addressLine());
  result.addString("region", region());
  result.addString("city", city());
  result.addString("dependentLocality", dependentLocality());
  result.addString("postalCode", postalCode());
  result.addString("sortingCode", sortingCode());
  result.addString("languageCode", languageCode());
  result.addString("organization", organization());
  result.addString("recipient", recipient());
  result.addString("phone", phone());
  return result.scriptValue();
}

}  // namespace blink

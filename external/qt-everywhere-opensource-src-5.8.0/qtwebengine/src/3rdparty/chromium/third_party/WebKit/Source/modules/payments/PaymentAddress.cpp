// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/payments/PaymentAddress.h"

namespace blink {

PaymentAddress::PaymentAddress(mojom::blink::PaymentAddressPtr address)
    : m_country(address->country)
    , m_addressLine(address->address_line.PassStorage())
    , m_region(address->region)
    , m_city(address->city)
    , m_dependentLocality(address->dependent_locality)
    , m_postalCode(address->postal_code)
    , m_sortingCode(address->sorting_code)
    , m_languageCode(address->language_code)
    , m_organization(address->organization)
    , m_recipient(address->recipient)
    , m_careOf(address->careOf)
    , m_phone(address->phone)
{
    if (!m_languageCode.isEmpty() && !address->script_code.isEmpty()) {
        m_languageCode.append("-");
        m_languageCode.append(address->script_code);
    }
}

PaymentAddress::~PaymentAddress() {}

} // namespace blink

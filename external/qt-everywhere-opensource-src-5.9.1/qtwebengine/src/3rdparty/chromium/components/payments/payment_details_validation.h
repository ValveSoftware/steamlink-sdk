// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_PAYMENT_DETAILS_VALIDATION_H_
#define COMPONENTS_PAYMENTS_PAYMENT_DETAILS_VALIDATION_H_

#include <string>

#include "components/payments/payment_request.mojom.h"

namespace payments {

bool validatePaymentDetails(const mojom::PaymentDetailsPtr& details,
                            std::string* error_message);

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_PAYMENT_DETAILS_VALIDATION_H_

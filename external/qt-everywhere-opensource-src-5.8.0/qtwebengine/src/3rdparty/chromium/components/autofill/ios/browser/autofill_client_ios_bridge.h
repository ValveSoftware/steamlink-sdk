// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_CLIENT_IOS_BRIDGE_H_
#define COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_CLIENT_IOS_BRIDGE_H_

#include <stdint.h>
#include <vector>

#include "base/memory/weak_ptr.h"

namespace autofill {
class AutofillPopupDelegate;
struct FormData;
class FormStructure;
struct Suggestion;
};

// Interface used to pipe events from AutoFillManangerDelegateIOS to the
// embedder.
@protocol AutofillClientIOSBridge

- (void)showAutofillPopup:(const std::vector<autofill::Suggestion>&)suggestions
            popupDelegate:
                (const base::WeakPtr<autofill::AutofillPopupDelegate>&)delegate;

- (void)hideAutofillPopup;

- (void)onFormDataFilled:(uint16_t)query_id
                  result:(const autofill::FormData&)result;

- (void)sendAutofillTypePredictionsToRenderer:
        (const std::vector<autofill::FormStructure*>&)forms;

@end

#endif  // COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_CLIENT_IOS_BRIDGE_H_

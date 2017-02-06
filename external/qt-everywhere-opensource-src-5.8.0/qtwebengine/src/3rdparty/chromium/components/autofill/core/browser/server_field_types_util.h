// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_SERVER_FIELD_TYPES_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_SERVER_FIELD_TYPES_UTIL_H_

#include <stddef.h>

#include <vector>

#include "components/autofill/core/browser/detail_input.h"
#include "components/autofill/core/browser/dialog_section.h"
#include "components/autofill/core/browser/field_types.h"

namespace autofill {

class AutofillField;
class AutofillType;

// Returns true if |type| should be shown when |field_type| has been requested.
// This filters the types that we fill into the page to match the ones the
// dialog actually cares about, preventing rAc from giving away data that an
// AutofillProfile or other data source might know about the user which isn't
// represented in the dialog.
bool ServerTypeEncompassesFieldType(ServerFieldType type,
                                    const AutofillType& field_type);

// Returns true if |type| in the given |section| should be used for a
// site-requested |field|.
bool ServerTypeMatchesField(DialogSection section,
                            ServerFieldType type,
                            const AutofillField& field);

// Returns true if the |type| belongs to the CREDIT_CARD field type group.
bool IsCreditCardType(ServerFieldType type);

// Constructs |inputs| from the array of inputs in |input_template|.
void BuildInputs(const DetailInput input_template[],
                 size_t template_size,
                 DetailInputs* inputs);

// Gets just the |type| attributes from each DetailInput.
std::vector<ServerFieldType> TypesFromInputs(const DetailInputs& inputs);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_SERVER_FIELD_TYPES_UTIL_H_

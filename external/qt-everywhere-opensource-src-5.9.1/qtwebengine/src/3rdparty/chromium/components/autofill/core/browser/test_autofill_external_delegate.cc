// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/test_autofill_external_delegate.h"

#include "ui/gfx/geometry/rect.h"

namespace autofill {

void GenerateTestAutofillPopup(
    AutofillExternalDelegate* autofill_external_delegate) {
  int query_id = 1;
  FormData form;
  FormFieldData field;
  field.is_focusable = true;
  field.should_autocomplete = true;
  gfx::RectF bounds(100.f, 100.f);
  autofill_external_delegate->OnQuery(query_id, form, field, bounds);

  std::vector<Suggestion> suggestions;
  suggestions.push_back(Suggestion());
  autofill_external_delegate->OnSuggestionsReturned(query_id, suggestions);
}

}  // namespace autofill

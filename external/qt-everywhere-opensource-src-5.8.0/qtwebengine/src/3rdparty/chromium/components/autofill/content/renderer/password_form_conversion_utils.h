// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_PASSWORD_FORM_CONVERSION_UTILS_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_PASSWORD_FORM_CONVERSION_UTILS_H_

#include <map>
#include <memory>
#include <vector>

#include "components/autofill/core/common/password_form_field_prediction_map.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "url/gurl.h"

namespace blink {
class WebDocument;
class WebFormElement;
class WebFormControlElement;
class WebFrame;
class WebInputElement;
}

namespace autofill {

struct FormData;
struct FormFieldData;
struct PasswordForm;

// Tests whether the given form is a GAIA reauthentication form. The form is
// not passed directly as WebFormElement, but by specifying its |origin| and
// |control_elements|. This is for better performance and easier testing.
// TODO(msramek): Move this logic to the browser.
bool IsGaiaReauthenticationForm(
    const GURL& origin,
    const std::vector<blink::WebFormControlElement>& control_elements);

typedef std::map<const blink::WebInputElement,
                 blink::WebString> ModifiedValues;

// Create a PasswordForm from DOM form. Webkit doesn't allow storing
// custom metadata to DOM nodes, so we have to do this every time an event
// happens with a given form and compare against previously Create'd forms
// to identify..which sucks.
// If an element of |form| has an entry in |nonscript_modified_values|, the
// associated string is used instead of the element's value to create
// the PasswordForm.
// |form_predictions| is Autofill server response, if present it's used for
// overwriting default username element selection.
std::unique_ptr<PasswordForm> CreatePasswordFormFromWebForm(
    const blink::WebFormElement& form,
    const ModifiedValues* nonscript_modified_values,
    const FormsPredictionsMap* form_predictions);

// Same as CreatePasswordFormFromWebForm() but for input elements that are not
// enclosed in <form> element.
std::unique_ptr<PasswordForm> CreatePasswordFormFromUnownedInputElements(
    const blink::WebFrame& frame,
    const ModifiedValues* nonscript_modified_values,
    const FormsPredictionsMap* form_predictions);

// Checks in a case-insensitive way if the autocomplete attribute for the given
// |element| is present and has the specified |value_in_lowercase|.
bool HasAutocompleteAttributeValue(const blink::WebInputElement& element,
                                   const char* value_in_lowercase);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_PASSWORD_FORM_CONVERSION_UTILS_H__

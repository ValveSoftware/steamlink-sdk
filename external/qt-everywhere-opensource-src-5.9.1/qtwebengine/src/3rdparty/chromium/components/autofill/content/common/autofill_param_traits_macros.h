// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Singly or multiply-included shared traits file depending on circumstances.
// This allows the use of Autofill IPC serialization macros in more than one IPC
// message file.
#ifndef COMPONENTS_AUTOFILL_CONTENT_COMMON_AUTOFILL_PARAM_TRAITS_MACROS_H_
#define COMPONENTS_AUTOFILL_CONTENT_COMMON_AUTOFILL_PARAM_TRAITS_MACROS_H_

#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/password_form.h"
#include "ipc/ipc_message_macros.h"
#include "url/ipc/url_param_traits.h"
#include "url/origin.h"

IPC_ENUM_TRAITS_MAX_VALUE(autofill::FormFieldData::CheckStatus,
                          autofill::FormFieldData::CheckStatus::CHECKED)

IPC_ENUM_TRAITS_MAX_VALUE(autofill::FormFieldData::RoleAttribute,
                          autofill::FormFieldData::ROLE_ATTRIBUTE_OTHER)

IPC_ENUM_TRAITS_MAX_VALUE(base::i18n::TextDirection,
                          base::i18n::TEXT_DIRECTION_NUM_DIRECTIONS - 1)

IPC_STRUCT_TRAITS_BEGIN(autofill::FormFieldData)
  IPC_STRUCT_TRAITS_MEMBER(label)
  IPC_STRUCT_TRAITS_MEMBER(name)
  IPC_STRUCT_TRAITS_MEMBER(value)
  IPC_STRUCT_TRAITS_MEMBER(form_control_type)
  IPC_STRUCT_TRAITS_MEMBER(autocomplete_attribute)
  IPC_STRUCT_TRAITS_MEMBER(placeholder)
  IPC_STRUCT_TRAITS_MEMBER(role)
  IPC_STRUCT_TRAITS_MEMBER(max_length)
  IPC_STRUCT_TRAITS_MEMBER(is_autofilled)
  IPC_STRUCT_TRAITS_MEMBER(check_status)
  IPC_STRUCT_TRAITS_MEMBER(is_focusable)
  IPC_STRUCT_TRAITS_MEMBER(should_autocomplete)
  IPC_STRUCT_TRAITS_MEMBER(text_direction)
  IPC_STRUCT_TRAITS_MEMBER(option_values)
  IPC_STRUCT_TRAITS_MEMBER(option_contents)
  IPC_STRUCT_TRAITS_MEMBER(css_classes)
  IPC_STRUCT_TRAITS_MEMBER(properties_mask)
IPC_STRUCT_TRAITS_END()

IPC_ENUM_TRAITS_MAX_VALUE(autofill::PasswordForm::Type,
                          autofill::PasswordForm::TYPE_LAST)

IPC_STRUCT_TRAITS_BEGIN(autofill::FormData)
  IPC_STRUCT_TRAITS_MEMBER(name)
  IPC_STRUCT_TRAITS_MEMBER(origin)
  IPC_STRUCT_TRAITS_MEMBER(action)
  IPC_STRUCT_TRAITS_MEMBER(is_form_tag)
  IPC_STRUCT_TRAITS_MEMBER(is_formless_checkout)
  IPC_STRUCT_TRAITS_MEMBER(fields)
IPC_STRUCT_TRAITS_END()

IPC_ENUM_TRAITS_MAX_VALUE(autofill::PasswordForm::Scheme,
                          autofill::PasswordForm::SCHEME_LAST)

IPC_ENUM_TRAITS_MAX_VALUE(autofill::PasswordForm::Layout,
                          autofill::PasswordForm::Layout::LAYOUT_LAST)

IPC_STRUCT_TRAITS_BEGIN(autofill::PasswordForm)
  IPC_STRUCT_TRAITS_MEMBER(scheme)
  IPC_STRUCT_TRAITS_MEMBER(signon_realm)
  IPC_STRUCT_TRAITS_MEMBER(origin)
  IPC_STRUCT_TRAITS_MEMBER(action)
  IPC_STRUCT_TRAITS_MEMBER(submit_element)
  IPC_STRUCT_TRAITS_MEMBER(username_element)
  IPC_STRUCT_TRAITS_MEMBER(username_marked_by_site)
  IPC_STRUCT_TRAITS_MEMBER(username_value)
  IPC_STRUCT_TRAITS_MEMBER(other_possible_usernames)
  IPC_STRUCT_TRAITS_MEMBER(password_element)
  IPC_STRUCT_TRAITS_MEMBER(password_value)
  IPC_STRUCT_TRAITS_MEMBER(new_password_element)
  IPC_STRUCT_TRAITS_MEMBER(new_password_value)
  IPC_STRUCT_TRAITS_MEMBER(preferred)
  IPC_STRUCT_TRAITS_MEMBER(blacklisted_by_user)
  IPC_STRUCT_TRAITS_MEMBER(type)
  IPC_STRUCT_TRAITS_MEMBER(times_used)
  IPC_STRUCT_TRAITS_MEMBER(form_data)
  IPC_STRUCT_TRAITS_MEMBER(layout)
  IPC_STRUCT_TRAITS_MEMBER(was_parsed_using_autofill_predictions)
  IPC_STRUCT_TRAITS_MEMBER(does_look_like_signup_form)
IPC_STRUCT_TRAITS_END()

#endif  // COMPONENTS_AUTOFILL_CONTENT_COMMON_AUTOFILL_PARAM_TRAITS_MACROS_H_

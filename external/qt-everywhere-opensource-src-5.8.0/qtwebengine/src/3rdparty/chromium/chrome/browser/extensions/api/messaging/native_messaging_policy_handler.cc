// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/messaging/native_messaging_policy_handler.h"

#include <utility>

#include "base/logging.h"
#include "chrome/browser/extensions/api/messaging/native_messaging_host_manifest.h"
#include "chrome/browser/extensions/external_policy_loader.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/prefs/pref_value_map.h"
#include "grit/components_strings.h"
#include "policy/policy_constants.h"

namespace extensions {

NativeMessagingHostListPolicyHandler::NativeMessagingHostListPolicyHandler(
    const char* policy_name,
    const char* pref_path,
    bool allow_wildcards)
    : policy::TypeCheckingPolicyHandler(policy_name, base::Value::TYPE_LIST),
      pref_path_(pref_path),
      allow_wildcards_(allow_wildcards) {}

NativeMessagingHostListPolicyHandler::~NativeMessagingHostListPolicyHandler() {}

bool NativeMessagingHostListPolicyHandler::CheckPolicySettings(
    const policy::PolicyMap& policies,
    policy::PolicyErrorMap* errors) {
  return CheckAndGetList(policies, errors, NULL);
}

void NativeMessagingHostListPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
  std::unique_ptr<base::ListValue> list;
  policy::PolicyErrorMap errors;
  if (CheckAndGetList(policies, &errors, &list) && list)
    prefs->SetValue(pref_path(), std::move(list));
}

const char* NativeMessagingHostListPolicyHandler::pref_path() const {
  return pref_path_;
}

bool NativeMessagingHostListPolicyHandler::CheckAndGetList(
    const policy::PolicyMap& policies,
    policy::PolicyErrorMap* errors,
    std::unique_ptr<base::ListValue>* names) {
  const base::Value* value = NULL;
  if (!CheckAndGetValue(policies, errors, &value))
    return false;

  if (!value)
    return true;

  const base::ListValue* list_value = NULL;
  if (!value->GetAsList(&list_value)) {
    NOTREACHED();
    return false;
  }

  // Filter the list, rejecting any invalid native messaging host names.
  std::unique_ptr<base::ListValue> filtered_list(new base::ListValue());
  for (base::ListValue::const_iterator entry(list_value->begin());
       entry != list_value->end(); ++entry) {
    std::string name;
    if (!(*entry)->GetAsString(&name)) {
      errors->AddError(policy_name(),
                       entry - list_value->begin(),
                       IDS_POLICY_TYPE_ERROR,
                       ValueTypeToString(base::Value::TYPE_STRING));
      continue;
    }
    if (!(allow_wildcards_ && name == "*") &&
        !NativeMessagingHostManifest::IsValidName(name)) {
      errors->AddError(policy_name(),
                       entry - list_value->begin(),
                       IDS_POLICY_VALUE_FORMAT_ERROR);
      continue;
    }
    filtered_list->AppendString(name);
  }

  if (names)
    *names = std::move(filtered_list);

  return true;
}

}  // namespace extensions

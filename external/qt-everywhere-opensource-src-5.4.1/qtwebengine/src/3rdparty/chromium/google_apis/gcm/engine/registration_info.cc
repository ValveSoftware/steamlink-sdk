// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/engine/registration_info.h"

#include "base/strings/string_util.h"

namespace gcm {

RegistrationInfo::RegistrationInfo() {
}

RegistrationInfo::~RegistrationInfo() {
}

std::string RegistrationInfo::SerializeAsString() const {
  if (sender_ids.empty() || registration_id.empty())
    return std::string();

  // Serialize as:
  //    sender1,sender2,...=reg_id
  std::string value;
  for (std::vector<std::string>::const_iterator iter = sender_ids.begin();
       iter != sender_ids.end(); ++iter) {
    DCHECK(!iter->empty() &&
           iter->find(',') == std::string::npos &&
           iter->find('=') == std::string::npos);
    if (!value.empty())
      value += ",";
    value += *iter;
  }

  DCHECK(registration_id.find('=') == std::string::npos);
  value += '=';
  value += registration_id;
  return value;
}

bool RegistrationInfo::ParseFromString(const std::string& value) {
  if (value.empty())
    return true;

  size_t pos = value.find('=');
  if (pos == std::string::npos)
    return false;

  std::string senders = value.substr(0, pos);
  registration_id = value.substr(pos + 1);

  Tokenize(senders, ",", &sender_ids);

  if (sender_ids.empty() || registration_id.empty()) {
    sender_ids.clear();
    registration_id.clear();
    return false;
  }

  return true;
}

}  // namespace gcm

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GCM_ENGINE_REGISTRATION_INFO_H_
#define GOOGLE_APIS_GCM_ENGINE_REGISTRATION_INFO_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/linked_ptr.h"
#include "google_apis/gcm/base/gcm_export.h"

namespace gcm {

struct GCM_EXPORT RegistrationInfo {
  RegistrationInfo();
  ~RegistrationInfo();

  std::string SerializeAsString() const;
  bool ParseFromString(const std::string& value);

  std::vector<std::string> sender_ids;
  std::string registration_id;
};

// Map of app id to registration info.
typedef std::map<std::string, linked_ptr<RegistrationInfo> >
RegistrationInfoMap;

}  // namespace gcm

#endif  // GOOGLE_APIS_GCM_ENGINE_REGISTRATION_INFO_H_

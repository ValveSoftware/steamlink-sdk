// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/identity.h"

#include "base/guid.h"

namespace service_manager {

Identity::Identity() {}

Identity::Identity(const std::string& name, const std::string& user_id)
    : Identity(name, user_id, "") {}

Identity::Identity(const std::string& name, const std::string& user_id,
                   const std::string& instance)
    : name_(name),
      user_id_(user_id),
      instance_(instance) {
  CHECK(!user_id.empty());
  CHECK(base::IsValidGUID(user_id));
}

Identity::Identity(const Identity& other) = default;

Identity::~Identity() {}

bool Identity::operator<(const Identity& other) const {
  if (name_ != other.name_)
    return name_ < other.name_;
  if (instance_ != other.instance_)
    return instance_ < other.instance_;
  return user_id_ < other.user_id_;
}

bool Identity::operator==(const Identity& other) const {
  return other.name_ == name_ && other.instance_ == instance_ &&
         other.user_id_ == user_id_;
}

bool Identity::IsValid() const {
  return !name_.empty() && base::IsValidGUID(user_id_);
}

}  // namespace service_manager

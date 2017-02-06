// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHELL_PUBLIC_CPP_CAPABILITIES_H_
#define SERVICES_SHELL_PUBLIC_CPP_CAPABILITIES_H_

#include <map>
#include <set>
#include <string>

#include "services/shell/public/interfaces/shell_resolver.mojom.h"

namespace shell {

using Class = std::string;
using Classes = std::set<std::string>;
using Interface = std::string;
using Interfaces = std::set<std::string>;
using Name = std::string;

// See comments in services/shell/public/interfaces/capabilities.mojom for a
// description of CapabilityRequest and CapabilitySpec.

struct CapabilityRequest {
  CapabilityRequest();
  CapabilityRequest(const CapabilityRequest& other);
  ~CapabilityRequest();
  bool operator==(const CapabilityRequest& other) const;
  bool operator<(const CapabilityRequest& other) const;
  Classes classes;
  Interfaces interfaces;
};

struct CapabilitySpec {
  CapabilitySpec();
  CapabilitySpec(const CapabilitySpec& other);
  ~CapabilitySpec();
  bool operator==(const CapabilitySpec& other) const;
  bool operator<(const CapabilitySpec& other) const;
  std::map<Class, Interfaces> provided;
  std::map<Name, CapabilityRequest> required;
};

}  // namespace shell

namespace mojo {

template <>
struct TypeConverter<shell::mojom::CapabilitySpecPtr, shell::CapabilitySpec> {
  static shell::mojom::CapabilitySpecPtr Convert(
      const shell::CapabilitySpec& input);
};

template <>
struct TypeConverter<shell::CapabilitySpec, shell::mojom::CapabilitySpecPtr> {
  static shell::CapabilitySpec Convert(
      const shell::mojom::CapabilitySpecPtr& input);
};

template <>
struct TypeConverter<shell::mojom::CapabilityRequestPtr,
                     shell::CapabilityRequest> {
  static shell::mojom::CapabilityRequestPtr Convert(
      const shell::CapabilityRequest& input);
};

template <>
struct TypeConverter<shell::CapabilityRequest,
                     shell::mojom::CapabilityRequestPtr> {
  static shell::CapabilityRequest Convert(
      const shell::mojom::CapabilityRequestPtr& input);
};

}  // namespace mojo

#endif  // SERVICES_SHELL_PUBLIC_CPP_CAPABILITIES_H_

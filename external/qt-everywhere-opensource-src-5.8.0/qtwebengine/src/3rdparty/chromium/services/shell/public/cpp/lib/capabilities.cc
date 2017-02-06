// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shell/public/cpp/capabilities.h"

namespace shell {

CapabilityRequest::CapabilityRequest() {}
CapabilityRequest::CapabilityRequest(const CapabilityRequest& other) = default;
CapabilityRequest::~CapabilityRequest() {}

bool CapabilityRequest::operator==(const CapabilityRequest& other) const {
  return other.classes == classes && other.interfaces == interfaces;
}

bool CapabilityRequest::operator<(const CapabilityRequest& other) const {
  return std::tie(classes, interfaces) <
      std::tie(other.classes, other.interfaces);
}

CapabilitySpec::CapabilitySpec() {}
CapabilitySpec::CapabilitySpec(const CapabilitySpec& other) = default;
CapabilitySpec::~CapabilitySpec() {}

bool CapabilitySpec::operator==(const CapabilitySpec& other) const {
  return other.provided == provided && other.required == required;
}

bool CapabilitySpec::operator<(const CapabilitySpec& other) const {
  return std::tie(provided, required) <
      std::tie(other.provided, other.required);
}

}  // namespace shell

namespace mojo {

// static
shell::mojom::CapabilitySpecPtr
TypeConverter<shell::mojom::CapabilitySpecPtr, shell::CapabilitySpec>::Convert(
    const shell::CapabilitySpec& input) {
  shell::mojom::CapabilitySpecPtr spec(shell::mojom::CapabilitySpec::New());
  spec->provided =
      mojo::Map<mojo::String, mojo::Array<mojo::String>>::From(input.provided);
  spec->required =
      mojo::Map<mojo::String, shell::mojom::CapabilityRequestPtr>::From(
          input.required);
  return spec;
}

// static
shell::CapabilitySpec
TypeConverter<shell::CapabilitySpec, shell::mojom::CapabilitySpecPtr>::Convert(
    const shell::mojom::CapabilitySpecPtr& input) {
  shell::CapabilitySpec spec;
  spec.provided =
      input->provided.To<std::map<shell::Class, shell::Interfaces>>();
  spec.required =
      input->required.To<std::map<shell::Name, shell::CapabilityRequest>>();
  return spec;
}

// static
shell::mojom::CapabilityRequestPtr TypeConverter<
    shell::mojom::CapabilityRequestPtr,
    shell::CapabilityRequest>::Convert(const shell::CapabilityRequest& input) {
  shell::mojom::CapabilityRequestPtr request(
      shell::mojom::CapabilityRequest::New());
  request->classes = mojo::Array<mojo::String>::From(input.classes);
  request->interfaces = mojo::Array<mojo::String>::From(input.interfaces);
  return request;
}

// static
shell::CapabilityRequest
TypeConverter<shell::CapabilityRequest, shell::mojom::CapabilityRequestPtr>::
    Convert(const shell::mojom::CapabilityRequestPtr& input) {
  shell::CapabilityRequest request;
  request.classes = input->classes.To<std::set<std::string>>();
  request.interfaces = input->interfaces.To<std::set<std::string>>();
  return request;
}

}  // namespace mojo

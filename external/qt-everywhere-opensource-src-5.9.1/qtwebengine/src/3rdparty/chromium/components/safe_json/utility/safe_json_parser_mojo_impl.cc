// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_json/utility/safe_json_parser_mojo_impl.h"

#include <memory>
#include <utility>

#include "base/json/json_reader.h"
#include "base/values.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace safe_json {

SafeJsonParserMojoImpl::SafeJsonParserMojoImpl() = default;

SafeJsonParserMojoImpl::~SafeJsonParserMojoImpl() = default;

// static
void SafeJsonParserMojoImpl::Create(
    mojo::InterfaceRequest<mojom::SafeJsonParser> request) {
  mojo::MakeStrongBinding(base::MakeUnique<SafeJsonParserMojoImpl>(),
                          std::move(request));
}

void SafeJsonParserMojoImpl::Parse(const std::string& json,
                                   const ParseCallback& callback) {
  int error_code;
  std::string error;
  std::unique_ptr<base::Value> value = base::JSONReader::ReadAndReturnError(
      json, base::JSON_PARSE_RFC, &error_code, &error);
  base::ListValue wrapper;
  if (value) {
    wrapper.Append(std::move(value));
    callback.Run(wrapper, base::nullopt);
  } else {
    callback.Run(wrapper, base::make_optional(std::move(error)));
  }
}

}  // namespace safe_json

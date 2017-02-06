// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_json/utility/safe_json_parser_mojo_impl.h"

#include <memory>
#include <utility>

#include "base/json/json_reader.h"
#include "base/values.h"

namespace safe_json {

// static
void SafeJsonParserMojoImpl::Create(
    mojo::InterfaceRequest<mojom::SafeJsonParser> request) {
  new SafeJsonParserMojoImpl(std::move(request));
}

SafeJsonParserMojoImpl::SafeJsonParserMojoImpl(
    mojo::InterfaceRequest<mojom::SafeJsonParser> request)
    : binding_(this, std::move(request)) {}

SafeJsonParserMojoImpl::~SafeJsonParserMojoImpl() {
}

void SafeJsonParserMojoImpl::Parse(const mojo::String& json,
                                   const ParseCallback& callback) {
  int error_code;
  std::string error;
  std::unique_ptr<base::Value> value = base::JSONReader::ReadAndReturnError(
      json.get(), base::JSON_PARSE_RFC, &error_code, &error);
  base::ListValue wrapper;
  if (value) {
    wrapper.Append(std::move(value));
    callback.Run(wrapper, nullptr);
  } else {
    callback.Run(wrapper, error);
  }
}

}  // namespace safe_json

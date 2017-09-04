// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_JSON_SAFE_JSON_PARSER_MOJO_IMPL_H_
#define COMPONENTS_SAFE_JSON_SAFE_JSON_PARSER_MOJO_IMPL_H_

#include <string>

#include "base/macros.h"
#include "components/safe_json/public/interfaces/safe_json.mojom.h"

namespace safe_json {

class SafeJsonParserMojoImpl : public mojom::SafeJsonParser {
 public:
  SafeJsonParserMojoImpl();
  ~SafeJsonParserMojoImpl() override;

  static void Create(mojo::InterfaceRequest<mojom::SafeJsonParser> request);

 private:
  // mojom::SafeJsonParser implementation.
  void Parse(const std::string& json, const ParseCallback& callback) override;

  DISALLOW_COPY_AND_ASSIGN(SafeJsonParserMojoImpl);
};

}  // namespace safe_json

#endif  // COMPONENTS_SAFE_JSON_SAFE_JSON_PARSER_MOJO_IMPL_H_

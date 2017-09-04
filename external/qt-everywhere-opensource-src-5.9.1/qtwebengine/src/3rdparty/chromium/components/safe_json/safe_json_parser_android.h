// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_JSON_SAFE_JSON_PARSER_ANDROID_H_
#define COMPONENTS_SAFE_JSON_SAFE_JSON_PARSER_ANDROID_H_

#include <memory>

#include "base/macros.h"
#include "components/safe_json/safe_json_parser.h"

namespace safe_json {

class JsonSanitizer;

class SafeJsonParserAndroid : public SafeJsonParser {
 public:
  SafeJsonParserAndroid(const std::string& unsafe_json,
                        const SuccessCallback& success_callback,
                        const ErrorCallback& error_callback);

 private:
  friend std::default_delete<SafeJsonParserAndroid>;

  ~SafeJsonParserAndroid() override;

  void OnSanitizationSuccess(const std::string& sanitized_json);
  void OnSanitizationError(const std::string& error);

  // SafeJsonParser implementation.
  void Start() override;

  const std::string unsafe_json_;
  SuccessCallback success_callback_;
  ErrorCallback error_callback_;

  DISALLOW_COPY_AND_ASSIGN(SafeJsonParserAndroid);
};

}  // namespace safe_json

#endif   // COMPONENTS_SAFE_JSON_SAFE_JSON_PARSER_ANDROID_H_

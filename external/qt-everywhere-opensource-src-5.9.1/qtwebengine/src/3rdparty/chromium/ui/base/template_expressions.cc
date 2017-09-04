// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/template_expressions.h"

#include <stddef.h>

#include "base/logging.h"
#include "net/base/escape.h"

namespace {
const char kLeader[] = "$i18n";
const size_t kLeaderSize = arraysize(kLeader) - 1;
const char kKeyOpen = '{';
const char kKeyClose = '}';
}  // namespace

namespace ui {

std::string ReplaceTemplateExpressions(
    base::StringPiece source,
    const TemplateReplacements& replacements) {
  std::string formatted;
  const size_t kValueLengthGuess = 16;
  formatted.reserve(source.length() + replacements.size() * kValueLengthGuess);
  // Two position markers are used as cursors through the |source|.
  // The |current_pos| will follow behind |next_pos|.
  size_t current_pos = 0;
  while (true) {
    size_t next_pos = source.find(kLeader, current_pos);

    if (next_pos == std::string::npos) {
      source.substr(current_pos).AppendToString(&formatted);
      break;
    }

    source.substr(current_pos, next_pos - current_pos)
        .AppendToString(&formatted);
    current_pos = next_pos + kLeaderSize;

    size_t context_end = source.find(kKeyOpen, current_pos);
    CHECK_NE(context_end, std::string::npos);
    std::string context;
    source.substr(current_pos, context_end - current_pos)
        .AppendToString(&context);
    current_pos = context_end + sizeof(kKeyOpen);

    size_t key_end = source.find(kKeyClose, current_pos);
    CHECK_NE(key_end, std::string::npos);

    std::string key =
        source.substr(current_pos, key_end - current_pos).as_string();
    CHECK(!key.empty());

    TemplateReplacements::const_iterator value = replacements.find(key);
    CHECK(value != replacements.end()) << "$i18n replacement key \"" << key
                                       << "\" not found";

    std::string replacement = value->second;
    if (context.empty()) {
      // Make the replacement HTML safe.
      replacement = net::EscapeForHTML(replacement);
    } else if (context == "Raw") {
      // Pass the replacement through unchanged.
    } else {
      CHECK(false) << "Unknown context " << context;
    }

    formatted.append(replacement);

    current_pos = key_end + sizeof(kKeyClose);
  }
  return formatted;
}

}  // namespace ui

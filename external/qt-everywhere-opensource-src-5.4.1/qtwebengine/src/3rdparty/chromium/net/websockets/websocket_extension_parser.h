// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_WEBSOCKETS_WEBSOCKET_EXTENSION_PARSER_H_
#define NET_WEBSOCKETS_WEBSOCKET_EXTENSION_PARSER_H_

#include <string>

#include "base/strings/string_piece.h"
#include "net/base/net_export.h"
#include "net/websockets/websocket_extension.h"

namespace net {

class NET_EXPORT_PRIVATE WebSocketExtensionParser {
 public:
  WebSocketExtensionParser();
  ~WebSocketExtensionParser();

  // Parses the given string as a WebSocket extension header value.
  // This parser assumes some preprocesses are made.
  //  - The parser parses single extension at a time. This means that
  //    the parser parses |extension| in RFC6455 9.1, not |extension-list|.
  //  - There is no newline characters in the input. LWS-concatenation must
  //    have already been done.
  void Parse(const char* data, size_t size);
  void Parse(const std::string& data) {
    Parse(data.data(), data.size());
  }

  bool has_error() const { return has_error_; }
  const WebSocketExtension& extension() const { return extension_; }

 private:
  void Consume(char c);
  void ConsumeExtension(WebSocketExtension* extension);
  void ConsumeExtensionParameter(WebSocketExtension::Parameter* parameter);
  void ConsumeToken(base::StringPiece* token);
  void ConsumeQuotedToken(std::string* token);
  void ConsumeSpaces();
  bool Lookahead(char c);
  bool ConsumeIfMatch(char c);
  size_t UnconsumedBytes() const { return end_ - current_; }

  static bool IsControl(char c);
  static bool IsSeparator(char c);

  const char* current_;
  const char* end_;
  bool has_error_;
  WebSocketExtension extension_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketExtensionParser);
};

}  // namespace net

#endif  // NET_WEBSOCKETS_WEBSOCKET_EXTENSION_PARSER_H_

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_extension_parser.h"

#include "base/strings/string_util.h"

namespace net {

WebSocketExtensionParser::WebSocketExtensionParser() {}

WebSocketExtensionParser::~WebSocketExtensionParser() {}

void WebSocketExtensionParser::Parse(const char* data, size_t size) {
  current_ = data;
  end_ = data + size;
  has_error_ = false;

  ConsumeExtension(&extension_);
  if (has_error_) return;
  ConsumeSpaces();
  has_error_ = has_error_ || (current_ != end_);
}

void WebSocketExtensionParser::Consume(char c) {
  DCHECK(!has_error_);
  ConsumeSpaces();
  DCHECK(!has_error_);
  if (current_ == end_ || c != current_[0]) {
    has_error_ = true;
    return;
  }
  ++current_;
}

void WebSocketExtensionParser::ConsumeExtension(WebSocketExtension* extension) {
  DCHECK(!has_error_);
  base::StringPiece name;
  ConsumeToken(&name);
  if (has_error_) return;
  *extension = WebSocketExtension(name.as_string());

  while (ConsumeIfMatch(';')) {
    WebSocketExtension::Parameter parameter((std::string()));
    ConsumeExtensionParameter(&parameter);
    if (has_error_) return;
    extension->Add(parameter);
  }
}

void WebSocketExtensionParser::ConsumeExtensionParameter(
    WebSocketExtension::Parameter* parameter) {
  DCHECK(!has_error_);
  base::StringPiece name, value;
  std::string value_string;

  ConsumeToken(&name);
  if (has_error_) return;
  if (!ConsumeIfMatch('=')) {
    *parameter = WebSocketExtension::Parameter(name.as_string());
    return;
  }

  if (Lookahead('\"')) {
    ConsumeQuotedToken(&value_string);
  } else {
    ConsumeToken(&value);
    value_string = value.as_string();
  }
  if (has_error_) return;
  *parameter = WebSocketExtension::Parameter(name.as_string(), value_string);
}

void WebSocketExtensionParser::ConsumeToken(base::StringPiece* token) {
  DCHECK(!has_error_);
  ConsumeSpaces();
  DCHECK(!has_error_);
  const char* head = current_;
  while (current_ < end_ &&
         !IsControl(current_[0]) && !IsSeparator(current_[0]))
    ++current_;
  if (current_ == head) {
    has_error_ = true;
    return;
  }
  *token = base::StringPiece(head, current_ - head);
}

void WebSocketExtensionParser::ConsumeQuotedToken(std::string* token) {
  DCHECK(!has_error_);
  Consume('"');
  if (has_error_) return;
  *token = "";
  while (current_ < end_ && !IsControl(current_[0])) {
    if (UnconsumedBytes() >= 2 && current_[0] == '\\') {
      char next = current_[1];
      if (IsControl(next) || IsSeparator(next)) break;
      *token += next;
      current_ += 2;
    } else if (IsSeparator(current_[0])) {
      break;
    } else {
      *token += current_[0];
      ++current_;
    }
  }
  // We can't use Consume here because we don't want to consume spaces.
  if (current_ < end_ && current_[0] == '"')
    ++current_;
  else
    has_error_ = true;
  has_error_ = has_error_ || token->empty();
}

void WebSocketExtensionParser::ConsumeSpaces() {
  DCHECK(!has_error_);
  while (current_ < end_ && (current_[0] == ' ' || current_[0] == '\t'))
    ++current_;
  return;
}

bool WebSocketExtensionParser::Lookahead(char c) {
  DCHECK(!has_error_);
  const char* head = current_;

  Consume(c);
  bool result = !has_error_;
  current_ = head;
  has_error_ = false;
  return result;
}

bool WebSocketExtensionParser::ConsumeIfMatch(char c) {
  DCHECK(!has_error_);
  const char* head = current_;

  Consume(c);
  if (has_error_) {
    current_ = head;
    has_error_ = false;
    return false;
  }
  return true;
}

// static
bool WebSocketExtensionParser::IsControl(char c) {
  return (0 <= c && c <= 31) || c == 127;
}

// static
bool WebSocketExtensionParser::IsSeparator(char c) {
  const char separators[] = "()<>@,;:\\\"/[]?={} \t";
  return strchr(separators, c) != NULL;
}

}  // namespace net

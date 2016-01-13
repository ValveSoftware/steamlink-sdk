// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_net_log_params.h"

#include "base/strings/stringprintf.h"
#include "base/values.h"

namespace net {

base::Value* NetLogWebSocketHandshakeCallback(
    const std::string* headers,
    NetLog::LogLevel /* log_level */) {
  base::DictionaryValue* dict = new base::DictionaryValue();
  base::ListValue* header_list = new base::ListValue();

  size_t last = 0;
  size_t headers_size = headers->size();
  size_t pos = 0;
  while (pos <= headers_size) {
    if (pos == headers_size ||
        ((*headers)[pos] == '\r' &&
         pos + 1 < headers_size && (*headers)[pos + 1] == '\n')) {
      std::string entry = headers->substr(last, pos - last);
      pos += 2;
      last = pos;

      header_list->Append(new base::StringValue(entry));

      if (entry.empty()) {
        // Dump WebSocket key3.
        std::string key;
        for (; pos < headers_size; ++pos) {
          key += base::StringPrintf("\\x%02x", (*headers)[pos] & 0xff);
        }
        header_list->Append(new base::StringValue(key));
        break;
      }
    } else {
      ++pos;
    }
  }

  dict->Set("headers", header_list);
  return dict;
}

}  // namespace net

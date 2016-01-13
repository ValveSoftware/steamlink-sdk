// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_header_block.h"

#include "base/values.h"
#include "net/http/http_log_util.h"

namespace net {

base::Value* SpdyHeaderBlockNetLogCallback(
    const SpdyHeaderBlock* headers,
    NetLog::LogLevel log_level) {
  base::DictionaryValue* dict = new base::DictionaryValue();
  base::DictionaryValue* headers_dict = new base::DictionaryValue();
  for (SpdyHeaderBlock::const_iterator it = headers->begin();
       it != headers->end(); ++it) {
    headers_dict->SetWithoutPathExpansion(
        it->first,
        new base::StringValue(
            ElideHeaderValueForNetLog(log_level, it->first, it->second)));
  }
  dict->Set("headers", headers_dict);
  return dict;
}

bool SpdyHeaderBlockFromNetLogParam(
    const base::Value* event_param,
    SpdyHeaderBlock* headers) {
  headers->clear();

  const base::DictionaryValue* dict = NULL;
  const base::DictionaryValue* header_dict = NULL;

  if (!event_param ||
      !event_param->GetAsDictionary(&dict) ||
      !dict->GetDictionary("headers", &header_dict)) {
    return false;
  }

  for (base::DictionaryValue::Iterator it(*header_dict); !it.IsAtEnd();
       it.Advance()) {
    if (!it.value().GetAsString(&(*headers)[it.key()])) {
      headers->clear();
      return false;
    }
  }
  return true;
}

}  // namespace net

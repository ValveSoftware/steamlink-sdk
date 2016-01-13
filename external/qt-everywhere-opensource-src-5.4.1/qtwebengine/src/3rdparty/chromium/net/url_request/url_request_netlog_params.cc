// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_netlog_params.h"

#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "url/gurl.h"

namespace net {

base::Value* NetLogURLRequestStartCallback(const GURL* url,
                                           const std::string* method,
                                           int load_flags,
                                           RequestPriority priority,
                                           int64 upload_id,
                                           NetLog::LogLevel /* log_level */) {
  base::DictionaryValue* dict = new base::DictionaryValue();
  dict->SetString("url", url->possibly_invalid_spec());
  dict->SetString("method", *method);
  dict->SetInteger("load_flags", load_flags);
  dict->SetString("priority", RequestPriorityToString(priority));
  if (upload_id > -1)
    dict->SetString("upload_id", base::Int64ToString(upload_id));
  return dict;
}

bool StartEventLoadFlagsFromEventParams(const base::Value* event_params,
                                        int* load_flags) {
  const base::DictionaryValue* dict;
  if (!event_params->GetAsDictionary(&dict) ||
      !dict->GetInteger("load_flags", load_flags)) {
    *load_flags = 0;
    return false;
  }
  return true;
}

}  // namespace net

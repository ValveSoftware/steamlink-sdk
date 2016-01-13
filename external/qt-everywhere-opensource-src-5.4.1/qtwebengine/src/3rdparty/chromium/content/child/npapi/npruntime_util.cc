// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/npapi/npruntime_util.h"

#include "base/pickle.h"
#include "third_party/WebKit/public/web/WebBindings.h"

using blink::WebBindings;

namespace content {

bool SerializeNPIdentifier(NPIdentifier identifier, Pickle* pickle) {
  const NPUTF8* string;
  int32_t number;
  bool is_string;
  WebBindings::extractIdentifierData(identifier, string, number, is_string);

  if (!pickle->WriteBool(is_string))
    return false;
  if (is_string) {
    // Write the null byte for efficiency on the other end.
    return pickle->WriteData(string, strlen(string) + 1);
  }
  return pickle->WriteInt(number);
}

bool DeserializeNPIdentifier(PickleIterator* pickle_iter,
                             NPIdentifier* identifier) {
  bool is_string;
  if (!pickle_iter->ReadBool(&is_string))
    return false;

  if (is_string) {
    const char* data;
    int data_len;
    if (!pickle_iter->ReadData(&data, &data_len))
      return false;
    DCHECK_EQ((static_cast<size_t>(data_len)), strlen(data) + 1);
    *identifier = WebBindings::getStringIdentifier(data);
  } else {
    int number;
    if (!pickle_iter->ReadInt(&number))
      return false;
    *identifier = WebBindings::getIntIdentifier(number);
  }
  return true;
}

}  // namespace content

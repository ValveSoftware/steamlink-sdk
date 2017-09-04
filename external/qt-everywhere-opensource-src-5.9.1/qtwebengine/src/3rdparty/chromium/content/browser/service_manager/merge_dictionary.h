// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_MANAGER_MERGE_DICTIONARY_H_
#define CONTENT_BROWSER_SERVICE_MANAGER_MERGE_DICTIONARY_H_

#include "base/values.h"
#include "content/common/content_export.h"

namespace content {

// Similar to base::DictionaryValue::MergeDictionary(), except concatenates
// ListValue contents.
// This is explicitly not part of base::DictionaryValue at brettw's request.
void CONTENT_EXPORT MergeDictionary(base::DictionaryValue* target,
                                    const base::DictionaryValue* source);

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_MANAGER_MERGE_DICTIONARY_H_

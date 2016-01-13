// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_NPAPI_NPRUNTIME_UTIL_H_
#define CONTENT_CHILD_NPAPI_NPRUNTIME_UTIL_H_

#include "third_party/npapi/bindings/npruntime.h"

class Pickle;
class PickleIterator;

namespace content {

// Efficiently serialize/deserialize a NPIdentifier
bool SerializeNPIdentifier(NPIdentifier identifier, Pickle* pickle);
bool DeserializeNPIdentifier(PickleIterator* pickle_iter,
                             NPIdentifier* identifier);

}  // namespace content

#endif  // CONTENT_CHILD_NPAPI_NPRUNTIME_UTIL_H_

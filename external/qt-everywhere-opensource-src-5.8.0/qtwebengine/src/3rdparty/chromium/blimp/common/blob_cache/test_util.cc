// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/common/blob_cache/test_util.h"

namespace blimp {

BlobDataPtr CreateBlobDataPtr(const std::string& data) {
  return new BlobData(data);
}

}  // namespace blimp

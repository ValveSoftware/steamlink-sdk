// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_CHILD_FIXED_RECEIVED_DATA_H_
#define CONTENT_PUBLIC_CHILD_FIXED_RECEIVED_DATA_H_

#include <stddef.h>

#include <vector>

#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/public/child/request_peer.h"

namespace content {

class CONTENT_EXPORT FixedReceivedData final
    : public RequestPeer::ThreadSafeReceivedData {
 public:
  FixedReceivedData(const char* data, size_t length, int encoded_length);
  explicit FixedReceivedData(ReceivedData* data);
  FixedReceivedData(const std::vector<char>& data, int encoded_length);
  ~FixedReceivedData() override;

  const char* payload() const override;
  int length() const override;
  int encoded_length() const override;

 private:
  std::vector<char> data_;
  int encoded_length_;

  DISALLOW_COPY_AND_ASSIGN(FixedReceivedData);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_CHILD_FIXED_RECEIVED_DATA_H_

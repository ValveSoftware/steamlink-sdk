// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SOCKET_STREAM_HANDLE_DATA_H_
#define CONTENT_RENDERER_SOCKET_STREAM_HANDLE_DATA_H_

#include "base/supports_user_data.h"
#include "content/common/content_export.h"

namespace content {

// User data stored in each WebSocketStreamHandleImpl.
class SocketStreamHandleData : public base::SupportsUserData::Data {
 public:
  explicit SocketStreamHandleData(int render_frame_id)
      : render_frame_id_(render_frame_id) {}
  virtual ~SocketStreamHandleData() {}

  int render_frame_id() const { return render_frame_id_; }

 private:
  int render_frame_id_;

  DISALLOW_COPY_AND_ASSIGN(SocketStreamHandleData);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SOCKET_STREAM_HANDLE_DATA_H_

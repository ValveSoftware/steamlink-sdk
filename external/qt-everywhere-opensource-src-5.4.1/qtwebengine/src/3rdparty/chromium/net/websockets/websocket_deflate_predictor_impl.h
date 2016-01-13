// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_WEBSOCKETS_WEBSOCKET_DEFLATE_PREDICTOR_IMPL_H_
#define NET_WEBSOCKETS_WEBSOCKET_DEFLATE_PREDICTOR_IMPL_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_vector.h"
#include "net/base/net_export.h"
#include "net/websockets/websocket_deflate_predictor.h"

namespace net {

struct WebSocketFrame;

class NET_EXPORT_PRIVATE WebSocketDeflatePredictorImpl
    : public WebSocketDeflatePredictor {
 public:
  virtual ~WebSocketDeflatePredictorImpl() {}

  virtual Result Predict(const ScopedVector<WebSocketFrame>& frames,
                         size_t frame_index) OVERRIDE;
  virtual void RecordInputDataFrame(const WebSocketFrame* frame) OVERRIDE;
  virtual void RecordWrittenDataFrame(const WebSocketFrame* frame) OVERRIDE;
};

}  // namespace net

#endif  // NET_WEBSOCKETS_WEBSOCKET_DEFLATE_PREDICTOR_IMPL_H_

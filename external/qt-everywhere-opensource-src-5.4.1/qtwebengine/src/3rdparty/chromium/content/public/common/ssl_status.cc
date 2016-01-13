// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/ssl_status.h"

namespace content {

SSLStatus::SSLStatus()
    : security_style(SECURITY_STYLE_UNKNOWN),
      cert_id(0),
      cert_status(0),
      security_bits(-1),
      connection_status(0),
      content_status(NORMAL_CONTENT) {
}

SSLStatus::~SSLStatus() {}

}  // namespace content

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PRESENTATION_SESSION_MESSAGE_H_
#define CONTENT_PUBLIC_BROWSER_PRESENTATION_SESSION_MESSAGE_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "content/common/content_export.h"

namespace content {

enum PresentationMessageType {
  TEXT,
  ARRAY_BUFFER,
  BLOB,
};

// Represents a presentation session message.
// If this is a text message, |data| is null; otherwise, |message| is null.
// Empty messages are allowed.
struct CONTENT_EXPORT PresentationSessionMessage {
 public:
  explicit PresentationSessionMessage(PresentationMessageType type);
  ~PresentationSessionMessage();

  bool is_binary() const;
  const PresentationMessageType type;
  std::string message;
  std::unique_ptr<std::vector<uint8_t>> data;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PRESENTATION_SESSION_H_

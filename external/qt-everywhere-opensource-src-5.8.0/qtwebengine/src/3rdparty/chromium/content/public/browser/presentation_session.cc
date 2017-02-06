// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/presentation_session.h"

namespace content {

PresentationSessionInfo::PresentationSessionInfo(
    const std::string& presentation_url,
    const std::string& presentation_id)
    : presentation_url(presentation_url),
      presentation_id(presentation_id) {
}

PresentationSessionInfo::~PresentationSessionInfo() {
}

PresentationError::PresentationError(
    PresentationErrorType error_type,
    const std::string& message)
    : error_type(error_type),
      message(message) {
}

PresentationError::~PresentationError() {
}

}  // namespace content


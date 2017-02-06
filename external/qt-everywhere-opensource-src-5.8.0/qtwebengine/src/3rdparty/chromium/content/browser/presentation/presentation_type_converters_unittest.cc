// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "content/browser/presentation/presentation_type_converters.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

TEST(PresentationTypeConvertersTest, PresentationSessionInfo) {
  std::string presentation_url("http://fooUrl");
  std::string presentation_id("presentationId");
  PresentationSessionInfo session(presentation_url, presentation_id);
  blink::mojom::PresentationSessionInfoPtr session_mojo(
      blink::mojom::PresentationSessionInfo::From(session));
  EXPECT_FALSE(session_mojo.is_null());
  EXPECT_EQ(presentation_url, session_mojo->url);
  EXPECT_EQ(presentation_id, session_mojo->id);
}

TEST(PresentationTypeConvertersTest, PresentationError) {
  std::string message("Error message");
  PresentationError error(PRESENTATION_ERROR_NO_AVAILABLE_SCREENS, message);
  blink::mojom::PresentationErrorPtr error_mojo(
      blink::mojom::PresentationError::From(error));
  EXPECT_FALSE(error_mojo.is_null());
  EXPECT_EQ(blink::mojom::PresentationErrorType::NO_AVAILABLE_SCREENS,
            error_mojo->error_type);
  EXPECT_EQ(message, error_mojo->message);
}

}  // namespace content

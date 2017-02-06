// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRESENTATION_PRESENTATION_TYPE_CONVERTERS_H_
#define CONTENT_BROWSER_PRESENTATION_PRESENTATION_TYPE_CONVERTERS_H_

#include "content/common/content_export.h"
#include "content/public/browser/presentation_session.h"
#include "third_party/WebKit/public/platform/modules/presentation/presentation.mojom.h"

namespace content {

CONTENT_EXPORT blink::mojom::PresentationErrorType PresentationErrorTypeToMojo(
    PresentationErrorType input);

CONTENT_EXPORT blink::mojom::PresentationConnectionState
PresentationConnectionStateToMojo(PresentationConnectionState state);

CONTENT_EXPORT blink::mojom::PresentationConnectionCloseReason
PresentationConnectionCloseReasonToMojo(
    PresentationConnectionCloseReason reason);
}  // namespace content

namespace mojo {

template <>
struct TypeConverter<blink::mojom::PresentationSessionInfoPtr,
                     content::PresentationSessionInfo> {
  static blink::mojom::PresentationSessionInfoPtr Convert(
      const content::PresentationSessionInfo& input) {
    blink::mojom::PresentationSessionInfoPtr output(
        blink::mojom::PresentationSessionInfo::New());
    output->url = input.presentation_url;
    output->id = input.presentation_id;
    return output;
  }
};

template <>
struct TypeConverter<content::PresentationSessionInfo,
                     blink::mojom::PresentationSessionInfoPtr> {
  static content::PresentationSessionInfo Convert(
      const blink::mojom::PresentationSessionInfoPtr& input) {
    return content::PresentationSessionInfo(input->url, input->id);
  }
};

template <>
struct TypeConverter<blink::mojom::PresentationErrorPtr,
                     content::PresentationError> {
  static blink::mojom::PresentationErrorPtr Convert(
      const content::PresentationError& input) {
    blink::mojom::PresentationErrorPtr output(
        blink::mojom::PresentationError::New());
    output->error_type = PresentationErrorTypeToMojo(input.error_type);
    output->message = input.message;
    return output;
  }
};

}  // namespace mojo

#endif  // CONTENT_BROWSER_PRESENTATION_PRESENTATION_TYPE_CONVERTERS_H_

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_PAGE_STATE_SERIALIZATION_H_
#define CONTENT_COMMON_PAGE_STATE_SERIALIZATION_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/strings/nullable_string16.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/common/resource_request_body_impl.h"
#include "third_party/WebKit/public/platform/WebHTTPBody.h"
#include "third_party/WebKit/public/platform/WebHistoryScrollRestorationType.h"
#include "third_party/WebKit/public/platform/WebReferrerPolicy.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "url/gurl.h"

namespace content {

struct CONTENT_EXPORT ExplodedHttpBody {
  base::NullableString16 http_content_type;
  scoped_refptr<ResourceRequestBodyImpl> request_body;
  bool contains_passwords;

  ExplodedHttpBody();
  ~ExplodedHttpBody();
};

struct CONTENT_EXPORT ExplodedFrameState {
  base::NullableString16 url_string;
  base::NullableString16 referrer;
  base::NullableString16 target;
  base::NullableString16 state_object;
  std::vector<base::NullableString16> document_state;
  blink::WebHistoryScrollRestorationType scroll_restoration_type;
  gfx::PointF visual_viewport_scroll_offset;
  gfx::Point scroll_offset;
  int64_t item_sequence_number;
  int64_t document_sequence_number;
  double page_scale_factor;
  blink::WebReferrerPolicy referrer_policy;
  ExplodedHttpBody http_body;
  std::vector<ExplodedFrameState> children;

  ExplodedFrameState();
  ExplodedFrameState(const ExplodedFrameState& other);
  ~ExplodedFrameState();
  void operator=(const ExplodedFrameState& other);

private:
  void assign(const ExplodedFrameState& other);
};

struct CONTENT_EXPORT ExplodedPageState {
  // TODO(creis, lukasza): Instead of storing them in |referenced_files|,
  // extract referenced files from ExplodedHttpBody.  |referenced_files|
  // currently contains a list from all frames, but cannot be deserialized into
  // the files referenced by each frame.  See http://crbug.com/441966.
  std::vector<base::NullableString16> referenced_files;
  ExplodedFrameState top;

  ExplodedPageState();
  ~ExplodedPageState();
};

CONTENT_EXPORT bool DecodePageState(const std::string& encoded,
                                    ExplodedPageState* exploded);
CONTENT_EXPORT bool EncodePageState(const ExplodedPageState& exploded,
                                    std::string* encoded);

#if defined(OS_ANDROID)
CONTENT_EXPORT bool DecodePageStateWithDeviceScaleFactorForTesting(
    const std::string& encoded,
    float device_scale_factor,
    ExplodedPageState* exploded);

// Converts results of EncodeResourceRequestBody (passed in as a pair of |data|
// + |size|) back into a ResourceRequestBodyImpl.  Returns nullptr if the
// decoding fails (e.g. if |data| is malformed).
scoped_refptr<ResourceRequestBodyImpl> DecodeResourceRequestBody(
    const char* data,
    size_t size);

// Encodes |resource_request_body| into |encoded|.
std::string EncodeResourceRequestBody(
    const ResourceRequestBodyImpl& resource_request_body);
#endif

}  // namespace content

#endif  // CONTENT_COMMON_PAGE_STATE_SERIALIZATION_H_

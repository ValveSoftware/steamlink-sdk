// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/internal_document_state_data.h"

#include "content/public/renderer/document_state.h"
#include "third_party/WebKit/public/web/WebDataSource.h"

namespace content {

namespace {

// Key InternalDocumentStateData is stored under in DocumentState.
const char kUserDataKey[] = "InternalDocumentStateData";

}

InternalDocumentStateData::InternalDocumentStateData()
    : http_status_code_(0),
      is_overriding_user_agent_(false),
      must_reset_scroll_and_scale_state_(false),
      cache_policy_override_set_(false),
      cache_policy_override_(blink::WebCachePolicy::UseProtocolCachePolicy) {}

// static
InternalDocumentStateData* InternalDocumentStateData::FromDataSource(
    blink::WebDataSource* ds) {
  return FromDocumentState(static_cast<DocumentState*>(ds->getExtraData()));
}

// static
InternalDocumentStateData* InternalDocumentStateData::FromDocumentState(
    DocumentState* ds) {
  if (!ds)
    return NULL;
  InternalDocumentStateData* data = static_cast<InternalDocumentStateData*>(
      ds->GetUserData(&kUserDataKey));
  if (!data) {
    data = new InternalDocumentStateData;
    ds->SetUserData(&kUserDataKey, data);
  }
  return data;
}

InternalDocumentStateData::~InternalDocumentStateData() {
}

}  // namespace content

// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visitedlink/renderer/visitedlink_slave.h"

#include <stddef.h>
#include <stdint.h>

#include "base/logging.h"
#include "third_party/WebKit/public/web/WebView.h"

using blink::WebView;

namespace visitedlink {

VisitedLinkSlave::VisitedLinkSlave() : binding_(this), weak_factory_(this) {}

VisitedLinkSlave::~VisitedLinkSlave() {
  FreeTable();
}

base::Callback<void(mojom::VisitedLinkNotificationSinkRequest)>
VisitedLinkSlave::GetBindCallback() {
  return base::Bind(&VisitedLinkSlave::Bind, weak_factory_.GetWeakPtr());
}

// Initializes the table with the given shared memory handle. This memory is
// mapped into the process.
void VisitedLinkSlave::UpdateVisitedLinks(
    mojo::ScopedSharedBufferHandle table) {
  DCHECK(table.is_valid()) << "Bad table handle";
  // Since this function may be called again to change the table, we may need
  // to free old objects.
  FreeTable();
  DCHECK(hash_table_ == NULL);

  int32_t table_len = 0;
  {
    // Map the header into our process so we can see how long the rest is,
    // and set the salt.
    mojo::ScopedSharedBufferMapping header_memory =
        table->Map(sizeof(SharedHeader));
    if (!header_memory)
      return;

    SharedHeader* header = static_cast<SharedHeader*>(header_memory.get());
    table_len = header->length;
    memcpy(salt_, header->salt, sizeof(salt_));
  }

  // Now we know the length, so map the table contents.
  table_mapping_ =
      table->MapAtOffset(table_len * sizeof(Fingerprint), sizeof(SharedHeader));
  if (!table_mapping_)
    return;

  // Commit the data.
  hash_table_ = reinterpret_cast<Fingerprint*>(table_mapping_.get());
  table_length_ = table_len;
}

void VisitedLinkSlave::AddVisitedLinks(
    const std::vector<VisitedLinkSlave::Fingerprint>& fingerprints) {
  for (size_t i = 0; i < fingerprints.size(); ++i)
    WebView::updateVisitedLinkState(fingerprints[i]);
}

void VisitedLinkSlave::ResetVisitedLinks(bool invalidate_hashes) {
  WebView::resetVisitedLinkState(invalidate_hashes);
}

void VisitedLinkSlave::FreeTable() {
  if (!hash_table_)
    return;

  table_mapping_.reset();
  hash_table_ = NULL;
  table_length_ = 0;
}

void VisitedLinkSlave::Bind(mojom::VisitedLinkNotificationSinkRequest request) {
  binding_.Bind(std::move(request));
}

}  // namespace visitedlink

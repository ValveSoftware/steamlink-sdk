// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/revisit/bookmarks_by_url_provider_impl.h"

#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "url/gurl.h"

namespace sync_sessions {

BookmarksByUrlProviderImpl::BookmarksByUrlProviderImpl(
    bookmarks::BookmarkModel* model)
    : model_(model) {}

void BookmarksByUrlProviderImpl::GetNodesByURL(
    const GURL& url,
    std::vector<const bookmarks::BookmarkNode*>* nodes) {
  model_->GetNodesByURL(url, nodes);
}

}  // namespace sync_sessions

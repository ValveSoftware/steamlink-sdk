// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_REVISIT_BOOKMARKS_BY_URL_PROVIDER_IMPL_H_
#define COMPONENTS_SYNC_SESSIONS_REVISIT_BOOKMARKS_BY_URL_PROVIDER_IMPL_H_

#include <vector>

#include "base/macros.h"
#include "components/sync_sessions/revisit/bookmarks_page_revisit_observer.h"

class GURL;

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

namespace sync_sessions {

// Simple implementation of BookmarksByUrlProvider that delegates to a
// BookmarkModel. It holds a non-owning pointer, with the assumption that this
// object is destroyed before the BookmarkModel.
class BookmarksByUrlProviderImpl : public BookmarksByUrlProvider {
 public:
  explicit BookmarksByUrlProviderImpl(bookmarks::BookmarkModel* model);
  void GetNodesByURL(
      const GURL& url,
      std::vector<const bookmarks::BookmarkNode*>* nodes) override;

 private:
  bookmarks::BookmarkModel* model_;
  DISALLOW_COPY_AND_ASSIGN(BookmarksByUrlProviderImpl);
};

}  // namespace sync_sessions

#endif  // COMPONENTS_SYNC_SESSIONS_REVISIT_BOOKMARKS_BY_URL_PROVIDER_IMPL_H_

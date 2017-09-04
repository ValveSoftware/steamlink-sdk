// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_MANAGED_MANAGED_BOOKMARKS_TRACKER_H_
#define COMPONENTS_BOOKMARKS_MANAGED_MANAGED_BOOKMARKS_TRACKER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/prefs/pref_change_registrar.h"

class GURL;
class PrefService;

namespace base {
class ListValue;
}

namespace bookmarks {

class BookmarkModel;
class BookmarkNode;
class BookmarkPermanentNode;

// Tracks either the Managed Bookmarks pref (set by policy) or the Supervised
// Bookmarks pref (set for a supervised user by their custodian) and makes the
// managed_node()/supervised_node() in the BookmarkModel follow the
// policy/custodian-defined bookmark tree.
class ManagedBookmarksTracker {
 public:
  typedef base::Callback<std::string()> GetManagementDomainCallback;

  // Shared constants used in the policy configuration.
  static const char kName[];
  static const char kUrl[];
  static const char kChildren[];
  static const char kFolderName[];

  // If |is_supervised| is true, this will track supervised bookmarks rather
  // than managed bookmarks.
  ManagedBookmarksTracker(BookmarkModel* model,
                          PrefService* prefs,
                          bool is_supervised,
                          const GetManagementDomainCallback& callback);
  ~ManagedBookmarksTracker();

  // Returns the initial list of managed bookmarks, which can be passed to
  // LoadInitial() to do the initial load.
  std::unique_ptr<base::ListValue> GetInitialManagedBookmarks();

  // Loads the initial managed/supervised bookmarks in |list| into |folder|.
  // New nodes will be assigned IDs starting at |next_node_id|.
  // Returns the next node ID to use.
  static int64_t LoadInitial(BookmarkNode* folder,
                             const base::ListValue* list,
                             int64_t next_node_id);

  // Starts tracking the pref for updates to the managed/supervised bookmarks.
  // Should be called after loading the initial bookmarks.
  void Init(BookmarkPermanentNode* managed_node);

  bool is_supervised() const { return is_supervised_; }

  // Public for testing.
  static const char* GetPrefName(bool is_supervised);

 private:
  const char* GetPrefName() const;
  base::string16 GetBookmarksFolderTitle() const;

  void ReloadManagedBookmarks();

  void UpdateBookmarks(const BookmarkNode* folder, const base::ListValue* list);
  static bool LoadBookmark(const base::ListValue* list,
                           size_t index,
                           base::string16* title,
                           GURL* url,
                           const base::ListValue** children);

  BookmarkModel* model_;
  const bool is_supervised_;
  BookmarkPermanentNode* managed_node_;
  PrefService* prefs_;
  PrefChangeRegistrar registrar_;
  GetManagementDomainCallback get_management_domain_callback_;

  DISALLOW_COPY_AND_ASSIGN(ManagedBookmarksTracker);
};

}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_MANAGED_MANAGED_BOOKMARKS_TRACKER_H_


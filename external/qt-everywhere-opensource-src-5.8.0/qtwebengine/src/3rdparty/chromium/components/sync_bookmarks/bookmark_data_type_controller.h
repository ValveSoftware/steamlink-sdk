// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_DATA_TYPE_CONTROLLER_H__
#define COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_DATA_TYPE_CONTROLLER_H__

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/sync_driver/frontend_data_type_controller.h"

namespace browser_sync {

// A class that manages the startup and shutdown of bookmark sync.
class BookmarkDataTypeController : public FrontendDataTypeController,
                                   public bookmarks::BaseBookmarkModelObserver,
                                   public history::HistoryServiceObserver {
 public:
  BookmarkDataTypeController(
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_thread,
      const base::Closure& error_callback,
      sync_driver::SyncClient* sync_client);

  // FrontendDataTypeController:
  syncer::ModelType type() const override;

 private:
  ~BookmarkDataTypeController() override;

  // FrontendDataTypeController:
  bool StartModels() override;
  void CleanUpState() override;
  void CreateSyncComponents() override;

  // bookmarks::BaseBookmarkModelObserver:
  void BookmarkModelChanged() override;
  void BookmarkModelLoaded(bookmarks::BookmarkModel* model,
                           bool ids_reassigned) override;
  void BookmarkModelBeingDeleted(bookmarks::BookmarkModel* model) override;

  // Helper that returns true iff both the bookmark model and the history
  // service have finished loading.
  bool DependentsLoaded();

  // history::HistoryServiceObserver:
  void OnHistoryServiceLoaded(history::HistoryService* service) override;
  void HistoryServiceBeingDeleted(
      history::HistoryService* history_service) override;

  ScopedObserver<history::HistoryService, history::HistoryServiceObserver>
      history_service_observer_;
  ScopedObserver<bookmarks::BookmarkModel, BaseBookmarkModelObserver>
      bookmark_model_observer_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkDataTypeController);
};

}  // namespace browser_sync

#endif  // COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_DATA_TYPE_CONTROLLER_H__

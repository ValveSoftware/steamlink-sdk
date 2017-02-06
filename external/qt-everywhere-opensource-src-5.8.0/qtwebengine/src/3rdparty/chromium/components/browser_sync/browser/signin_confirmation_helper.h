// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_SYNC_BROWSER_SIGNIN_CONFIRMATION_HELPER_
#define COMPONENTS_BROWSER_SYNC_BROWSER_SIGNIN_CONFIRMATION_HELPER_

#include <stddef.h>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/task/cancelable_task_tracker.h"

namespace history {
class HistoryService;
class QueryResults;
}

namespace sync_driver {

// Helper class for sync signin to check some asynchronous conditions. Call
// either CheckHasHistory or CheckHasTypedUrls or both, and |return_result|
// will be called with true if either returns true, otherwise false.
class SigninConfirmationHelper {
 public:
  SigninConfirmationHelper(history::HistoryService* history_service,
                           const base::Callback<void(bool)>& return_result);

  // This helper checks if there are history entries in the history service.
  void CheckHasHistory(int max_entries);

  // This helper checks if there are typed URLs in the history service.
  void CheckHasTypedURLs();

 private:
  // Deletes itself.
  ~SigninConfirmationHelper();

  // Callback helper function for CheckHasHistory.
  void OnHistoryQueryResults(size_t max_entries,
                             history::QueryResults* results);

  // Posts the given result to the origin thread.
  void PostResult(bool result);

  // Calls |return_result_| if |result| == true or if it's the result of the
  // last pending check.
  void ReturnResult(bool result);

  // The task runner for the thread this object was constructed on.
  const scoped_refptr<base::SingleThreadTaskRunner> origin_thread_;

  // Pointer to the history service.
  history::HistoryService* history_service_;

  // Used for async tasks.
  base::CancelableTaskTracker task_tracker_;

  // Keep track of how many async requests are pending.
  int pending_requests_;

  // Callback to pass the result back to the caller.
  const base::Callback<void(bool)> return_result_;

  DISALLOW_COPY_AND_ASSIGN(SigninConfirmationHelper);
};

}  // namespace sync_driver

#endif  // COMPONENTS_BROWSER_SYNC_BROWSER_SIGNIN_CONFIRMATION_HELPER_

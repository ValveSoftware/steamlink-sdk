// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREVIEWS_CORE_PREVIEWS_UI_SERVICE_H_
#define COMPONENTS_PREVIEWS_CORE_PREVIEWS_UI_SERVICE_H_

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "components/previews/core/previews_experiments.h"
#include "components/previews/core/previews_opt_out_store.h"

class GURL;

namespace base {
class SingleThreadTaskRunner;
}

namespace previews {
class PreviewsIOData;

// A class to manage the UI portion of inter-thread communication between
// previews/ objects. Created and used on the UI thread.
class PreviewsUIService {
 public:
  PreviewsUIService(
      PreviewsIOData* previews_io_data,
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
      std::unique_ptr<PreviewsOptOutStore> previews_opt_out_store);
  virtual ~PreviewsUIService();

  // Sets |io_data_| to |io_data| to allow calls from the UI thread to the IO
  // thread. Virtualized in testing.
  virtual void SetIOData(base::WeakPtr<PreviewsIOData> io_data);

  // Adds a navigation to |url| to the black list with result |opt_out|.
  void AddPreviewNavigation(const GURL& url, PreviewsType type, bool opt_out);

  // Clears the history of the black list between |begin_time| and |end_time|.
  void ClearBlackList(base::Time begin_time, base::Time end_time);

 private:
  // The IO thread portion of the inter-thread communication for previews/.
  base::WeakPtr<previews::PreviewsIOData> io_data_;

  base::ThreadChecker thread_checker_;

  // The IO thread task runner. Used to post tasks to |io_data_|.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  base::WeakPtrFactory<PreviewsUIService> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PreviewsUIService);
};

}  // namespace previews

#endif  // COMPONENTS_PREVIEWS_CORE_PREVIEWS_UI_SERVICE_H_

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WORKER_SERVICE_H_
#define CONTENT_PUBLIC_BROWSER_WORKER_SERVICE_H_

#include <vector>

#include "base/process/process.h"
#include "content/common/content_export.h"
#include "url/gurl.h"

namespace content {

class WorkerServiceObserver;

// A singleton for managing HTML5 shared web workers. These are run in a
// separate process, since multiple renderer processes can be talking to a
// single shared worker. All the methods below can only be called on the IO
// thread.
class WorkerService {
 public:
  virtual ~WorkerService() {}

  // Returns the WorkerService singleton.
  CONTENT_EXPORT static WorkerService* GetInstance();

  // Determines whether embedded SharedWorker is enabled.
  CONTENT_EXPORT static bool EmbeddedSharedWorkerEnabled();

  // Terminates the given worker. Returns true if the process was found.
  virtual bool TerminateWorker(int process_id, int route_id) = 0;

  struct WorkerInfo {
    GURL url;
    base::string16 name;
    int process_id;
    int route_id;
    base::ProcessHandle handle;
  };

  // Return information about all the currently running workers.
  virtual std::vector<WorkerInfo> GetWorkers() = 0;

  virtual void AddObserver(WorkerServiceObserver* observer) = 0;
  virtual void RemoveObserver(WorkerServiceObserver* observer) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PLUGIN_SERVICE_H_

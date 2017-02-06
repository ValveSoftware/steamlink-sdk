// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COPRESENCE_PUBLIC_COPRESENCE_MANAGER_H_
#define COMPONENTS_COPRESENCE_PUBLIC_COPRESENCE_MANAGER_H_

#include <string>

#include "base/macros.h"
#include "components/copresence/public/copresence_delegate.h"

namespace copresence {

class CopresenceState;
class ReportRequest;

// The CopresenceManager class is the central interface for Copresence
// functionality. This class handles all the initialization and delegation
// of copresence tasks. Any user of copresence only needs to interact
// with this class.
class CopresenceManager {
 public:
  CopresenceManager() {}
  virtual ~CopresenceManager() {}

  // Accessor for the CopresenceState instance that tracks debug info.
  virtual CopresenceState* state() = 0;

  // This method will execute a report request. Each report request can have
  // multiple (un)publishes, (un)subscribes. This will ensure that once the
  // manager is initialized, it sends all request to the server and handles
  // the response. If an error is encountered, the status callback is used
  // to relay it to the requester.
  virtual void ExecuteReportRequest(const ReportRequest& request,
                                    const std::string& app_id,
                                    const std::string& auth_token,
                                    const StatusCallback& callback) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(CopresenceManager);
};

}  // namespace copresence

#endif  // COMPONENTS_COPRESENCE_PUBLIC_COPRESENCE_MANAGER_H_

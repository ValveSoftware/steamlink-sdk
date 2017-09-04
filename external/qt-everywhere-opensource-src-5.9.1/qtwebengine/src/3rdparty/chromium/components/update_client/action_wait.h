// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_ACTION_WAIT_H_
#define COMPONENTS_UPDATE_CLIENT_ACTION_WAIT_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/time/time.h"

#include "components/update_client/action.h"

namespace update_client {

// Implements a wait between handling updates for the CRXs in this context.
// To avoid thrashing of local computing resources, updates are applied one
// at a time, with a delay between them.
class ActionWait : public Action, protected ActionImpl {
 public:
  explicit ActionWait(const base::TimeDelta& time_delta);
  ~ActionWait() override;

  void Run(UpdateContext* update_context, Callback callback) override;

 private:
  void WaitComplete();

  const base::TimeDelta time_delta_;

  DISALLOW_COPY_AND_ASSIGN(ActionWait);
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_ACTION_WAIT_H_

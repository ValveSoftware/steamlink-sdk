// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_PING_MANAGER_H_
#define COMPONENTS_UPDATE_CLIENT_PING_MANAGER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"

namespace update_client {

class Configurator;
struct CrxUpdateItem;

// Sends fire-and-forget pings.
class PingManager {
 public:
  explicit PingManager(const scoped_refptr<Configurator>& config);
  virtual ~PingManager();

  // Sends a fire and forget ping for the |item|. Returns true if the
  // ping is queued up and may be sent in the future, or false, if an error
  // occurs right away. The ping itself is not persisted and it will be
  // discarded if it can't be sent for any reason.
  virtual bool SendPing(const CrxUpdateItem* item);

 private:
  const scoped_refptr<Configurator> config_;

  DISALLOW_COPY_AND_ASSIGN(PingManager);
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_PING_MANAGER_H_

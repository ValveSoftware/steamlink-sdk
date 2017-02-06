// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_UPDATE_CHECKER_H_
#define COMPONENTS_UPDATE_CLIENT_UPDATE_CHECKER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/update_client/update_response.h"
#include "url/gurl.h"

class GURL;

namespace update_client {

class PersistedData;
class Configurator;
struct CrxUpdateItem;

class UpdateChecker {
 public:
  using UpdateCheckCallback =
      base::Callback<void(int error,
                          const UpdateResponse::Results& results,
                          int retry_after_sec)>;

  using Factory = std::unique_ptr<UpdateChecker> (*)(
      const scoped_refptr<Configurator>& config,
      PersistedData* persistent);

  virtual ~UpdateChecker() {}

  // Initiates an update check for the |items_to_check|. |additional_attributes|
  // provides a way to customize the <request> element. This value is inserted
  // as-is, therefore it must be well-formed as an XML attribute string.
  virtual bool CheckForUpdates(
      const std::vector<CrxUpdateItem*>& items_to_check,
      const std::string& additional_attributes,
      const UpdateCheckCallback& update_check_callback) = 0;

  static std::unique_ptr<UpdateChecker> Create(
      const scoped_refptr<Configurator>& config,
      PersistedData* persistent);

 protected:
  UpdateChecker() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(UpdateChecker);
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_UPDATE_CHECKER_H_

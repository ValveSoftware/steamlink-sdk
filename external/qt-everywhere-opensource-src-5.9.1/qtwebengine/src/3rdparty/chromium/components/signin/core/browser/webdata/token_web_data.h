// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Chromium settings and storage represent user-selected preferences and
// information and MUST not be extracted, overwritten or modified except
// through Chromium defined APIs.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_WEBDATA_TOKEN_WEB_DATA_H__
#define COMPONENTS_SIGNIN_CORE_BROWSER_WEBDATA_TOKEN_WEB_DATA_H__

#include <map>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/webdata/common/web_data_results.h"
#include "components/webdata/common/web_data_service_base.h"
#include "components/webdata/common/web_data_service_consumer.h"
#include "components/webdata/common/web_database.h"

namespace base {
class SingleThreadTaskRunner;
}

class TokenWebDataBackend;
class WebDatabaseService;
class WebDataServiceConsumer;

// TokenWebData is a data repository for storage of authentication tokens.

class TokenWebData : public WebDataServiceBase {
 public:
  TokenWebData(scoped_refptr<WebDatabaseService> wdbs,
               scoped_refptr<base::SingleThreadTaskRunner> ui_thread,
               scoped_refptr<base::SingleThreadTaskRunner> db_thread,
               const ProfileErrorCallback& callback);

  TokenWebData(scoped_refptr<base::SingleThreadTaskRunner> ui_thread,
               scoped_refptr<base::SingleThreadTaskRunner> db_thread);

  // Set a token to use for a specified service.
  void SetTokenForService(const std::string& service,
                          const std::string& token);

  // Remove all tokens stored in the web database.
  void RemoveAllTokens();

  // Removes a token related to |service| from the web database.
  void RemoveTokenForService(const std::string& service);

  // Null on failure. Success is WDResult<std::vector<std::string> >
  virtual Handle GetAllTokens(WebDataServiceConsumer* consumer);

 protected:
  // For unit tests, passes a null callback.
  TokenWebData();

  ~TokenWebData() override;

 private:
  scoped_refptr<TokenWebDataBackend> token_backend_;

  DISALLOW_COPY_AND_ASSIGN(TokenWebData);
};

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_WEBDATA_TOKEN_WEB_DATA_H__

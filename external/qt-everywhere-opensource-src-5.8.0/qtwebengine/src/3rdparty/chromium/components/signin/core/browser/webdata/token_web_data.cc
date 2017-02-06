// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/webdata/token_web_data.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted_delete_on_message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "components/signin/core/browser/webdata/token_service_table.h"
#include "components/webdata/common/web_database_service.h"

using base::Bind;
using base::Time;

class TokenWebDataBackend
    : public base::RefCountedDeleteOnMessageLoop<TokenWebDataBackend> {

 public:
  TokenWebDataBackend(scoped_refptr<base::SingleThreadTaskRunner> db_thread)
      : base::RefCountedDeleteOnMessageLoop<TokenWebDataBackend>(db_thread) {}

  WebDatabase::State RemoveAllTokens(WebDatabase* db) {
    if (TokenServiceTable::FromWebDatabase(db)->RemoveAllTokens()) {
      return WebDatabase::COMMIT_NEEDED;
    }
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  WebDatabase::State RemoveTokenForService(
      const std::string& service, WebDatabase* db) {
    if (TokenServiceTable::FromWebDatabase(db)
            ->RemoveTokenForService(service)) {
      return WebDatabase::COMMIT_NEEDED;
    }
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  WebDatabase::State SetTokenForService(
      const std::string& service, const std::string& token, WebDatabase* db) {
    if (TokenServiceTable::FromWebDatabase(db)->SetTokenForService(service,
                                                                   token)) {
      return WebDatabase::COMMIT_NEEDED;
    }
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  std::unique_ptr<WDTypedResult> GetAllTokens(WebDatabase* db) {
    std::map<std::string, std::string> map;
    TokenServiceTable::FromWebDatabase(db)->GetAllTokens(&map);
    return base::WrapUnique(
        new WDResult<std::map<std::string, std::string>>(TOKEN_RESULT, map));
  }

 protected:
  virtual ~TokenWebDataBackend() {
  }

 private:
  friend class base::RefCountedDeleteOnMessageLoop<TokenWebDataBackend>;
  friend class base::DeleteHelper<TokenWebDataBackend>;
};

TokenWebData::TokenWebData(
    scoped_refptr<WebDatabaseService> wdbs,
    scoped_refptr<base::SingleThreadTaskRunner> ui_thread,
    scoped_refptr<base::SingleThreadTaskRunner> db_thread,
    const ProfileErrorCallback& callback)
    : WebDataServiceBase(wdbs, callback, ui_thread),
      token_backend_(new TokenWebDataBackend(db_thread)) {
}

void TokenWebData::SetTokenForService(const std::string& service,
                                      const std::string& token) {
  wdbs_->ScheduleDBTask(FROM_HERE,
      Bind(&TokenWebDataBackend::SetTokenForService, token_backend_,
           service, token));
}

void TokenWebData::RemoveAllTokens() {
  wdbs_->ScheduleDBTask(FROM_HERE,
      Bind(&TokenWebDataBackend::RemoveAllTokens, token_backend_));
}

void TokenWebData::RemoveTokenForService(const std::string& service) {
  wdbs_->ScheduleDBTask(FROM_HERE,
      Bind(&TokenWebDataBackend::RemoveTokenForService, token_backend_,
           service));
}

// Null on failure. Success is WDResult<std::string>
WebDataServiceBase::Handle TokenWebData::GetAllTokens(
    WebDataServiceConsumer* consumer) {
  return wdbs_->ScheduleDBTaskWithResult(FROM_HERE,
      Bind(&TokenWebDataBackend::GetAllTokens, token_backend_), consumer);
}

TokenWebData::TokenWebData(
    scoped_refptr<base::SingleThreadTaskRunner> ui_thread,
    scoped_refptr<base::SingleThreadTaskRunner> db_thread)
    : WebDataServiceBase(NULL, ProfileErrorCallback(), ui_thread),
      token_backend_(new TokenWebDataBackend(db_thread)) {
}

TokenWebData::~TokenWebData() {
}

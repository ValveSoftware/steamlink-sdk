// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/webdata/password_web_data_service_win.h"

#include "base/bind.h"
#include "components/os_crypt/ie7_password_win.h"
#include "components/password_manager/core/browser/webdata/logins_table.h"
#include "components/webdata/common/web_database_service.h"

PasswordWebDataService::PasswordWebDataService(
    scoped_refptr<WebDatabaseService> wdbs,
    scoped_refptr<base::SingleThreadTaskRunner> ui_thread,
    const ProfileErrorCallback& callback)
    : WebDataServiceBase(wdbs, callback, ui_thread) {
}

void PasswordWebDataService::AddIE7Login(const IE7PasswordInfo& info) {
  wdbs_->ScheduleDBTask(
      FROM_HERE,
      base::Bind(&PasswordWebDataService::AddIE7LoginImpl, this, info));
}

void PasswordWebDataService::RemoveIE7Login(const IE7PasswordInfo& info) {
  wdbs_->ScheduleDBTask(
      FROM_HERE,
      base::Bind(&PasswordWebDataService::RemoveIE7LoginImpl, this, info));
}

PasswordWebDataService::Handle PasswordWebDataService::GetIE7Login(
    const IE7PasswordInfo& info,
    WebDataServiceConsumer* consumer) {
  return wdbs_->ScheduleDBTaskWithResult(
      FROM_HERE,
      base::Bind(&PasswordWebDataService::GetIE7LoginImpl, this, info),
      consumer);
}

WebDatabase::State PasswordWebDataService::AddIE7LoginImpl(
    const IE7PasswordInfo& info,
    WebDatabase* db) {
  return LoginsTable::FromWebDatabase(db)->AddIE7Login(info) ?
      WebDatabase::COMMIT_NEEDED : WebDatabase::COMMIT_NOT_NEEDED;
}

WebDatabase::State PasswordWebDataService::RemoveIE7LoginImpl(
    const IE7PasswordInfo& info,
    WebDatabase* db) {
  return LoginsTable::FromWebDatabase(db)->RemoveIE7Login(info) ?
      WebDatabase::COMMIT_NEEDED : WebDatabase::COMMIT_NOT_NEEDED;
}

std::unique_ptr<WDTypedResult> PasswordWebDataService::GetIE7LoginImpl(
    const IE7PasswordInfo& info,
    WebDatabase* db) {
  IE7PasswordInfo result;
  LoginsTable::FromWebDatabase(db)->GetIE7Login(info, &result);
  return std::unique_ptr<WDTypedResult>(
      new WDResult<IE7PasswordInfo>(PASSWORD_IE7_RESULT, result));
}

////////////////////////////////////////////////////////////////////////////////

PasswordWebDataService::PasswordWebDataService(
    scoped_refptr<base::SingleThreadTaskRunner> ui_thread)
    : WebDataServiceBase(nullptr, ProfileErrorCallback(), ui_thread) {
}

PasswordWebDataService::~PasswordWebDataService() {
}

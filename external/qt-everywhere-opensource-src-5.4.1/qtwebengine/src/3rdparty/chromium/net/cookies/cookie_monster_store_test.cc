// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/cookie_monster_store_test.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_util.h"
#include "net/cookies/parsed_cookie.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net {
LoadedCallbackTask::LoadedCallbackTask(LoadedCallback loaded_callback,
                                       std::vector<CanonicalCookie*> cookies)
    : loaded_callback_(loaded_callback),
      cookies_(cookies) {
}

LoadedCallbackTask::~LoadedCallbackTask() {}

MockPersistentCookieStore::MockPersistentCookieStore()
    : load_return_value_(true),
      loaded_(false) {
}

void MockPersistentCookieStore::SetLoadExpectation(
    bool return_value,
    const std::vector<CanonicalCookie*>& result) {
  load_return_value_ = return_value;
  load_result_ = result;
}

void MockPersistentCookieStore::Load(const LoadedCallback& loaded_callback) {
  std::vector<CanonicalCookie*> out_cookies;
  if (load_return_value_) {
    out_cookies = load_result_;
    loaded_ = true;
  }
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&LoadedCallbackTask::Run,
                 new LoadedCallbackTask(loaded_callback, out_cookies)));
}

void MockPersistentCookieStore::LoadCookiesForKey(
    const std::string& key,
    const LoadedCallback& loaded_callback) {
  if (!loaded_) {
    Load(loaded_callback);
  } else {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&LoadedCallbackTask::Run,
                   new LoadedCallbackTask(loaded_callback,
                                          std::vector<CanonicalCookie*>())));
  }
}

void MockPersistentCookieStore::AddCookie(const CanonicalCookie& cookie) {
  commands_.push_back(
      CookieStoreCommand(CookieStoreCommand::ADD, cookie));
}

void MockPersistentCookieStore::UpdateCookieAccessTime(
    const CanonicalCookie& cookie) {
  commands_.push_back(CookieStoreCommand(
      CookieStoreCommand::UPDATE_ACCESS_TIME, cookie));
}

void MockPersistentCookieStore::DeleteCookie(const CanonicalCookie& cookie) {
  commands_.push_back(
      CookieStoreCommand(CookieStoreCommand::REMOVE, cookie));
}

void MockPersistentCookieStore::Flush(const base::Closure& callback) {
  if (!callback.is_null())
    base::MessageLoop::current()->PostTask(FROM_HERE, callback);
}

void MockPersistentCookieStore::SetForceKeepSessionState() {
}

MockPersistentCookieStore::~MockPersistentCookieStore() {}

MockCookieMonsterDelegate::MockCookieMonsterDelegate() {}

void MockCookieMonsterDelegate::OnCookieChanged(
    const CanonicalCookie& cookie,
    bool removed,
    CookieMonster::Delegate::ChangeCause cause) {
  CookieNotification notification(cookie, removed);
  changes_.push_back(notification);
}

void MockCookieMonsterDelegate::OnLoaded() {}

MockCookieMonsterDelegate::~MockCookieMonsterDelegate() {}

CanonicalCookie BuildCanonicalCookie(const std::string& key,
                                     const std::string& cookie_line,
                                     const base::Time& creation_time) {

  // Parse the cookie line.
  ParsedCookie pc(cookie_line);
  EXPECT_TRUE(pc.IsValid());

  // This helper is simplistic in interpreting a parsed cookie, in order to
  // avoid duplicated CookieMonster's CanonPath() and CanonExpiration()
  // functions. Would be nice to export them, and re-use here.
  EXPECT_FALSE(pc.HasMaxAge());
  EXPECT_TRUE(pc.HasPath());
  base::Time cookie_expires = pc.HasExpires() ?
      cookie_util::ParseCookieTime(pc.Expires()) : base::Time();
  std::string cookie_path = pc.Path();

  return CanonicalCookie(
      GURL(), pc.Name(), pc.Value(), key, cookie_path,
      creation_time, cookie_expires, creation_time,
      pc.IsSecure(), pc.IsHttpOnly(), pc.Priority());
}

void AddCookieToList(
    const std::string& key,
    const std::string& cookie_line,
    const base::Time& creation_time,
    std::vector<CanonicalCookie*>* out_list) {
  scoped_ptr<CanonicalCookie> cookie(
      new CanonicalCookie(
          BuildCanonicalCookie(key, cookie_line, creation_time)));

  out_list->push_back(cookie.release());
}

MockSimplePersistentCookieStore::MockSimplePersistentCookieStore()
    : loaded_(false) {
}

void MockSimplePersistentCookieStore::Load(
    const LoadedCallback& loaded_callback) {
  std::vector<CanonicalCookie*> out_cookies;

  for (CanonicalCookieMap::const_iterator it = cookies_.begin();
       it != cookies_.end(); it++)
    out_cookies.push_back(new CanonicalCookie(it->second));

  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&LoadedCallbackTask::Run,
                 new LoadedCallbackTask(loaded_callback, out_cookies)));
  loaded_ = true;
}

void MockSimplePersistentCookieStore::LoadCookiesForKey(const std::string& key,
    const LoadedCallback& loaded_callback) {
  if (!loaded_) {
    Load(loaded_callback);
  } else {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&LoadedCallbackTask::Run,
                   new LoadedCallbackTask(loaded_callback,
                                          std::vector<CanonicalCookie*>())));
  }
}

void MockSimplePersistentCookieStore::AddCookie(const CanonicalCookie& cookie) {
  int64 creation_time = cookie.CreationDate().ToInternalValue();
  EXPECT_TRUE(cookies_.find(creation_time) == cookies_.end());
  cookies_[creation_time] = cookie;
}

void MockSimplePersistentCookieStore::UpdateCookieAccessTime(
    const CanonicalCookie& cookie) {
  int64 creation_time = cookie.CreationDate().ToInternalValue();
  ASSERT_TRUE(cookies_.find(creation_time) != cookies_.end());
  cookies_[creation_time].SetLastAccessDate(base::Time::Now());
}

void MockSimplePersistentCookieStore::DeleteCookie(
    const CanonicalCookie& cookie) {
  int64 creation_time = cookie.CreationDate().ToInternalValue();
  CanonicalCookieMap::iterator it = cookies_.find(creation_time);
  ASSERT_TRUE(it != cookies_.end());
  cookies_.erase(it);
}

void MockSimplePersistentCookieStore::Flush(const base::Closure& callback) {
  if (!callback.is_null())
    base::MessageLoop::current()->PostTask(FROM_HERE, callback);
}

void MockSimplePersistentCookieStore::SetForceKeepSessionState() {
}

CookieMonster* CreateMonsterFromStoreForGC(
    int num_cookies,
    int num_old_cookies,
    int days_old) {
  base::Time current(base::Time::Now());
  base::Time past_creation(base::Time::Now() - base::TimeDelta::FromDays(1000));
  scoped_refptr<MockSimplePersistentCookieStore> store(
      new MockSimplePersistentCookieStore);
  // Must expire to be persistent
  for (int i = 0; i < num_cookies; i++) {
    base::Time creation_time =
        past_creation + base::TimeDelta::FromMicroseconds(i);
    base::Time expiration_time = current + base::TimeDelta::FromDays(30);
    base::Time last_access_time =
        (i < num_old_cookies) ? current - base::TimeDelta::FromDays(days_old) :
                                current;

    CanonicalCookie cc(
        GURL(), "a", "1", base::StringPrintf("h%05d.izzle", i), "/path",
        creation_time, expiration_time, last_access_time, false, false,
        COOKIE_PRIORITY_DEFAULT);
    store->AddCookie(cc);
  }

  return new CookieMonster(store.get(), NULL);
}

MockSimplePersistentCookieStore::~MockSimplePersistentCookieStore() {}

}  // namespace net

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/cookie_store_test_helpers.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"

namespace net {

const int kDelayedTime = 0;

DelayedCookieMonster::DelayedCookieMonster()
      : cookie_monster_(new CookieMonster(NULL, NULL)),
        did_run_(false),
        result_(false) {
}

DelayedCookieMonster::~DelayedCookieMonster() {
}

void DelayedCookieMonster::SetCookiesInternalCallback(bool result) {
  result_ = result;
  did_run_ = true;
}

void DelayedCookieMonster::GetCookiesWithOptionsInternalCallback(
    const std::string& cookie) {
  cookie_ = cookie;
  did_run_ = true;
}

void DelayedCookieMonster::SetCookieWithOptionsAsync(
    const GURL& url,
    const std::string& cookie_line,
    const CookieOptions& options,
    const CookieMonster::SetCookiesCallback& callback) {
  did_run_ = false;
  cookie_monster_->SetCookieWithOptionsAsync(
      url, cookie_line, options,
      base::Bind(&DelayedCookieMonster::SetCookiesInternalCallback,
                 base::Unretained(this)));
  DCHECK_EQ(did_run_, true);
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&DelayedCookieMonster::InvokeSetCookiesCallback,
                 base::Unretained(this),
                 callback),
      base::TimeDelta::FromMilliseconds(kDelayedTime));
}

void DelayedCookieMonster::GetCookiesWithOptionsAsync(
    const GURL& url,
    const CookieOptions& options,
    const CookieMonster::GetCookiesCallback& callback) {
  did_run_ = false;
  cookie_monster_->GetCookiesWithOptionsAsync(
      url, options,
      base::Bind(&DelayedCookieMonster::GetCookiesWithOptionsInternalCallback,
                 base::Unretained(this)));
  DCHECK_EQ(did_run_, true);
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&DelayedCookieMonster::InvokeGetCookieStringCallback,
                 base::Unretained(this),
                 callback),
      base::TimeDelta::FromMilliseconds(kDelayedTime));
}

void DelayedCookieMonster::GetAllCookiesForURLAsync(
    const GURL& url,
    const GetCookieListCallback& callback) {
  cookie_monster_->GetAllCookiesForURLAsync(url, callback);
}

void DelayedCookieMonster::InvokeSetCookiesCallback(
    const CookieMonster::SetCookiesCallback& callback) {
  if (!callback.is_null())
    callback.Run(result_);
}

void DelayedCookieMonster::InvokeGetCookieStringCallback(
    const CookieMonster::GetCookiesCallback& callback) {
  if (!callback.is_null())
    callback.Run(cookie_);
}

bool DelayedCookieMonster::SetCookieWithOptions(
    const GURL& url,
    const std::string& cookie_line,
    const CookieOptions& options) {
  ADD_FAILURE();
  return false;
}

std::string DelayedCookieMonster::GetCookiesWithOptions(
    const GURL& url,
    const CookieOptions& options) {
  ADD_FAILURE();
  return std::string();
}

void DelayedCookieMonster::DeleteCookie(const GURL& url,
                                        const std::string& cookie_name) {
  ADD_FAILURE();
}

void DelayedCookieMonster::DeleteCookieAsync(const GURL& url,
                                             const std::string& cookie_name,
                                             const base::Closure& callback) {
  ADD_FAILURE();
}

void DelayedCookieMonster::DeleteAllCreatedBetweenAsync(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    const DeleteCallback& callback) {
  ADD_FAILURE();
}

void DelayedCookieMonster::DeleteAllCreatedBetweenForHostAsync(
    const base::Time delete_begin,
    const base::Time delete_end,
    const GURL& url,
    const DeleteCallback& callback) {
  ADD_FAILURE();
}

void DelayedCookieMonster::DeleteSessionCookiesAsync(const DeleteCallback&) {
  ADD_FAILURE();
}

CookieMonster* DelayedCookieMonster::GetCookieMonster() {
  return cookie_monster_.get();
}

}  // namespace net

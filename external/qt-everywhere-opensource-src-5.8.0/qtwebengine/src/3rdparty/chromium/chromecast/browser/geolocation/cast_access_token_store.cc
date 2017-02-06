// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/geolocation/cast_access_token_store.h"

#include "base/callback_helpers.h"
#include "chromecast/browser/cast_browser_context.h"
#include "chromecast/browser/cast_content_browser_client.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_client.h"

namespace chromecast {
namespace shell {

CastAccessTokenStore::CastAccessTokenStore(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
}

CastAccessTokenStore::~CastAccessTokenStore() {
}

void CastAccessTokenStore::GetRequestContextGetterOnUIThread() {
  request_context_ = content::BrowserContext::GetDefaultStoragePartition(
      browser_context_)->GetURLRequestContext();
}

void CastAccessTokenStore::RespondOnOriginatingThread() {
  base::ResetAndReturn(&callback_).Run(access_token_map_, request_context_);
}

void CastAccessTokenStore::LoadAccessTokens(
    const LoadAccessTokensCallback& callback) {
  callback_ = callback;
  content::BrowserThread::PostTaskAndReply(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&CastAccessTokenStore::GetRequestContextGetterOnUIThread,
                 this),
      base::Bind(&CastAccessTokenStore::RespondOnOriginatingThread, this));
}

void CastAccessTokenStore::SaveAccessToken(
    const GURL& server_url, const base::string16& access_token) {
  if (access_token_map_[server_url] != access_token) {
    access_token_map_[server_url] = access_token;
  }
}

}  // namespace shell
}  // namespace chromecast

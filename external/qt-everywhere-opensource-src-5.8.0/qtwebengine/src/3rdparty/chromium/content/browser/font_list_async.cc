// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/font_list_async.h"

#include <utility>

#include "base/bind.h"
#include "base/values.h"
#include "content/common/font_list.h"
#include "content/public/browser/browser_thread.h"

namespace content {

namespace {

// Just executes the given callback with the parameter.
void ReturnFontListToOriginalThread(
    const base::Callback<void(std::unique_ptr<base::ListValue>)>& callback,
    std::unique_ptr<base::ListValue> result) {
  callback.Run(std::move(result));
}

void GetFontListInBlockingPool(
    BrowserThread::ID calling_thread_id,
    const base::Callback<void(std::unique_ptr<base::ListValue>)>& callback) {
  std::unique_ptr<base::ListValue> result(GetFontList_SlowBlocking());
  BrowserThread::PostTask(calling_thread_id, FROM_HERE,
      base::Bind(&ReturnFontListToOriginalThread, callback,
                 base::Passed(&result)));
}

}  // namespace

void GetFontListAsync(
    const base::Callback<void(std::unique_ptr<base::ListValue>)>& callback) {
  BrowserThread::ID id;
  bool well_known_thread = BrowserThread::GetCurrentThreadIdentifier(&id);
  DCHECK(well_known_thread)
      << "Can only call GetFontList from a well-known thread.";

  BrowserThread::PostBlockingPoolSequencedTask(
      kFontListSequenceToken,
      FROM_HERE,
      base::Bind(&GetFontListInBlockingPool, id, callback));
}

}  // namespace content

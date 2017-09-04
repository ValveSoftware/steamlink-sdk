// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SHARED_WORKER_EMBEDDED_SHARED_WORKER_CONTENT_SETTINGS_CLIENT_PROXY_H_
#define CONTENT_RENDERER_SHARED_WORKER_EMBEDDED_SHARED_WORKER_CONTENT_SETTINGS_CLIENT_PROXY_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "third_party/WebKit/public/web/WebWorkerContentSettingsClientProxy.h"
#include "url/gurl.h"

namespace content {

class ThreadSafeSender;

// This proxy is created on the main renderer thread then passed onto
// the blink's worker thread.
class EmbeddedSharedWorkerContentSettingsClientProxy
    : public blink::WebWorkerContentSettingsClientProxy {
 public:
  EmbeddedSharedWorkerContentSettingsClientProxy(
      const GURL& origin_url,
      bool is_unique_origin,
      int routing_id,
      ThreadSafeSender* thread_safe_sender);
  ~EmbeddedSharedWorkerContentSettingsClientProxy() override;

  // WebWorkerContentSettingsClientProxy overrides.
  bool requestFileSystemAccessSync() override;
  bool allowIndexedDB(const blink::WebString& name) override;

 private:
  const GURL origin_url_;
  const bool is_unique_origin_;
  const int routing_id_;
  scoped_refptr<ThreadSafeSender> thread_safe_sender_;

  DISALLOW_COPY_AND_ASSIGN(EmbeddedSharedWorkerContentSettingsClientProxy);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SHARED_WORKER_EMBEDDED_SHARED_WORKER_CONTENT_SETTINGS_CLIENT_PROXY_H_

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SHARED_WORKER_EMBEDDED_SHARED_WORKER_PERMISSION_CLIENT_PROXY_H_
#define CONTENT_RENDERER_SHARED_WORKER_EMBEDDED_SHARED_WORKER_PERMISSION_CLIENT_PROXY_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "third_party/WebKit/public/web/WebWorkerPermissionClientProxy.h"
#include "url/gurl.h"

namespace content {

class ThreadSafeSender;

// This proxy is created on the main renderer thread then passed onto
// the blink's worker thread.
class EmbeddedSharedWorkerPermissionClientProxy
    : public blink::WebWorkerPermissionClientProxy {
 public:
  EmbeddedSharedWorkerPermissionClientProxy(
      const GURL& origin_url,
      bool is_unique_origin,
      int routing_id,
      ThreadSafeSender* thread_safe_sender);
  virtual ~EmbeddedSharedWorkerPermissionClientProxy();

  // WebWorkerPermissionClientProxy overrides.
  virtual bool allowDatabase(const blink::WebString& name,
                             const blink::WebString& display_name,
                             unsigned long estimated_size);
  virtual bool requestFileSystemAccessSync();
  virtual bool allowIndexedDB(const blink::WebString& name);

 private:
  const GURL origin_url_;
  const bool is_unique_origin_;
  const int routing_id_;
  scoped_refptr<ThreadSafeSender> thread_safe_sender_;

  DISALLOW_COPY_AND_ASSIGN(EmbeddedSharedWorkerPermissionClientProxy);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SHARED_WORKER_EMBEDDED_SHARED_WORKER_PERMISSION_CLIENT_PROXY_H_

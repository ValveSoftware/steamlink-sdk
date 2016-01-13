// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SHARED_WORKER_REPOSITORY_H_
#define CONTENT_RENDERER_SHARED_WORKER_REPOSITORY_H_

#include <set>

#include "base/basictypes.h"
#include "content/public/renderer/render_frame_observer.h"
#include "third_party/WebKit/public/web/WebContentSecurityPolicy.h"
#include "third_party/WebKit/public/web/WebSharedWorkerRepositoryClient.h"

namespace content {

class RenderFrameImpl;

class SharedWorkerRepository : public RenderFrameObserver,
                               public blink::WebSharedWorkerRepositoryClient {
 public:
  explicit SharedWorkerRepository(RenderFrameImpl* render_frame);
  virtual ~SharedWorkerRepository();

  // WebSharedWorkerRepositoryClient overrides.
  virtual blink::WebSharedWorkerConnector* createSharedWorkerConnector(
      const blink::WebURL& url,
      const blink::WebString& name,
      DocumentID document_id,
      const blink::WebString& content_security_policy,
      blink::WebContentSecurityPolicyType) OVERRIDE;
  virtual void documentDetached(DocumentID document_id) OVERRIDE;

 private:
  std::set<DocumentID> documents_with_workers_;

  DISALLOW_COPY_AND_ASSIGN(SharedWorkerRepository);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SHARED_WORKER_REPOSITORY_H_

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/shared_worker_repository.h"

#include "content/child/child_thread_impl.h"
#include "content/common/view_messages.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/websharedworker_proxy.h"
#include "third_party/WebKit/public/web/WebContentSecurityPolicy.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

namespace content {

SharedWorkerRepository::SharedWorkerRepository(RenderFrameImpl* render_frame)
    : RenderFrameObserver(render_frame) {
  render_frame->GetWebFrame()->setSharedWorkerRepositoryClient(this);
}

SharedWorkerRepository::~SharedWorkerRepository() {}

blink::WebSharedWorkerConnector*
SharedWorkerRepository::createSharedWorkerConnector(
    const blink::WebURL& url,
    const blink::WebString& name,
    DocumentID document_id,
    const blink::WebString& content_security_policy,
    blink::WebContentSecurityPolicyType security_policy_type,
    blink::WebAddressSpace creation_address_space,
    blink::WebSharedWorkerCreationContextType creation_context_type,
    blink::WebWorkerCreationError* error) {
  ViewHostMsg_CreateWorker_Params params;
  params.url = url;
  params.name = name;
  params.content_security_policy = content_security_policy;
  params.security_policy_type = security_policy_type;
  params.document_id = document_id;
  params.render_frame_route_id = render_frame()->GetRoutingID();
  params.creation_address_space = creation_address_space;
  params.creation_context_type = creation_context_type;
  ViewHostMsg_CreateWorker_Reply reply;
  Send(new ViewHostMsg_CreateWorker(params, &reply));
  *error = reply.error;
  if (reply.route_id == MSG_ROUTING_NONE) {
    return NULL;
  }
  documents_with_workers_.insert(document_id);
  return new WebSharedWorkerProxy(ChildThreadImpl::current()->GetRouter(),
                                  reply.route_id);
}

void SharedWorkerRepository::documentDetached(DocumentID document) {
  std::set<DocumentID>::iterator iter = documents_with_workers_.find(document);
  if (iter != documents_with_workers_.end()) {
    // Notify the browser process that the document has shut down.
    Send(new ViewHostMsg_DocumentDetached(document));
    documents_with_workers_.erase(iter);
  }
}

void SharedWorkerRepository::OnDestruct() {
  delete this;
}

}  // namespace content

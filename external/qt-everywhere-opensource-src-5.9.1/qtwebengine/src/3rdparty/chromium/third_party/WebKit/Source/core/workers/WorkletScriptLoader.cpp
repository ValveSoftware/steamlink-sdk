// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/WorkletScriptLoader.h"

#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "core/workers/Worklet.h"

namespace blink {

WorkletScriptLoader::WorkletScriptLoader(ScriptPromiseResolver* resolver,
                                         Worklet* worklet,
                                         ScriptResource* resource)
    : m_resolver(resolver), m_host(worklet) {
  setResource(resource);
}

void WorkletScriptLoader::cancel() {
  clearResource();
}

void WorkletScriptLoader::notifyFinished(Resource* resource) {
  DCHECK(this->resource() == resource);

  m_host->notifyFinished(this);
  if (resource->errorOccurred()) {
    m_resolver->reject(DOMException::create(NetworkError));
  } else {
    DCHECK(resource->isLoaded());
    m_resolver->resolve();
  }
  clearResource();
}

DEFINE_TRACE(WorkletScriptLoader) {
  visitor->trace(m_resolver);
  visitor->trace(m_host);
  ResourceOwner<ScriptResource, ScriptResourceClient>::trace(visitor);
}

}  // namespace blink

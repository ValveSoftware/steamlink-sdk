// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebServiceWorkerClientsInfo_h
#define WebServiceWorkerClientsInfo_h

#include "WebCallbacks.h"
#include "WebVector.h"

namespace blink {

struct WebServiceWorkerError;

struct WebServiceWorkerClientsInfo {
    WebVector<int> clientIDs;
};

typedef WebCallbacks<WebServiceWorkerClientsInfo, WebServiceWorkerError> WebServiceWorkerClientsCallbacks;

} // namespace blink

#endif // WebServiceWorkerClientsInfo_h

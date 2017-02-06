// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InternalsServiceWorker_h
#define InternalsServiceWorker_h

#include "wtf/Allocator.h"

namespace blink {

class Internals;
class ServiceWorker;

class InternalsServiceWorker {
    STATIC_ONLY(InternalsServiceWorker);
public:
    static void terminateServiceWorker(Internals&, ServiceWorker*);
};

} // namespace blink

#endif // InternalsServiceWorker_h

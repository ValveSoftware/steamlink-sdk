// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/SynchronousMutationObserver.h"

#include "core/dom/Document.h"
#include "core/dom/SynchronousMutationNotifier.h"

namespace blink {

SynchronousMutationObserver::SynchronousMutationObserver()
    : LifecycleObserver(nullptr) {}

void SynchronousMutationObserver::nodeChildrenWillBeRemoved(ContainerNode&) {}
void SynchronousMutationObserver::nodeWillBeRemoved(Node&) {}

}  // namespace blink

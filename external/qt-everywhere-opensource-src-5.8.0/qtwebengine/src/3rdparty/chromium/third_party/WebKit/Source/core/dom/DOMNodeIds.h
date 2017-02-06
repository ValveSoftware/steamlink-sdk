// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DOMNodeIds_h
#define DOMNodeIds_h

#include "core/CoreExport.h"
#include "core/dom/Node.h"
#include "core/dom/WeakIdentifierMap.h"
#include "wtf/Allocator.h"

namespace blink {

DECLARE_WEAK_IDENTIFIER_MAP(Node);

class CORE_EXPORT DOMNodeIds {
    STATIC_ONLY(DOMNodeIds);
public:
    static int idForNode(Node*);
    static Node* nodeForId(int id);
};

} // namespace blink


#endif // DOMNodeIds_h

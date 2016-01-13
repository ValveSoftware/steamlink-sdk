// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InspectorNodeIds_h
#define InspectorNodeIds_h

namespace WebCore {

class Node;

class InspectorNodeIds {
public:
    static int idForNode(Node*);
    static Node* nodeForId(int);
};

} // namespace WebCore


#endif // !defined(InspectorNodeIds_h)

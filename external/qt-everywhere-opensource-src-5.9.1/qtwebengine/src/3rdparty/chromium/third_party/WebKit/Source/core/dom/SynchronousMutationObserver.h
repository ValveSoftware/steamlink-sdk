// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SynchronousMutationObserver_h
#define SynchronousMutationObserver_h

#include "base/macros.h"
#include "core/CoreExport.h"
#include "platform/LifecycleObserver.h"

namespace blink {

class ContainerNode;
class Document;

// This class is a base class for classes which observe DOM tree mutation
// synchronously. If you want to observe DOM tree mutation asynchronously see
// MutationObserver Web API.
//
// TODO(yosin): Following classes should be derived from this class to
// simplify Document class.
//  - DragCaretController
//  - DocumentMarkerController
//  - EventHandler
//  - FrameCaret
//  - InputMethodController
//  - SelectionController
//  - Range set
//  - NodeIterator set
class CORE_EXPORT SynchronousMutationObserver
    : public LifecycleObserver<Document, SynchronousMutationObserver> {
 public:
  // TODO(yosin): We will have following member functions:
  //  - dataWillBeChanged(const CharacterData&);
  //  - didMoveTreeToNewDocument(const Node& root);
  //  - didInsertText(Node*, unsigned offset, unsigned length);
  //  - didRemoveText(Node*, unsigned offset, unsigned length);
  //  - didMergeTextNodes(Text& oldNode, unsigned offset);
  //  - didSplitTextNode(Text& oldNode);

  // Called before removing container node.
  virtual void nodeChildrenWillBeRemoved(ContainerNode&);

  // Called before removing node.
  virtual void nodeWillBeRemoved(Node&);

 protected:
  SynchronousMutationObserver();

 private:
  DISALLOW_COPY_AND_ASSIGN(SynchronousMutationObserver);
};

}  // namespace blink

#endif  // SynchronousMutationObserver_h

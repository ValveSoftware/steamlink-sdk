// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CustomElementUpgradeReaction_h
#define CustomElementUpgradeReaction_h

#include "core/CoreExport.h"
#include "core/dom/custom/CustomElementReaction.h"
#include "platform/heap/Handle.h"
#include "wtf/Noncopyable.h"

namespace blink {

class Element;

class CORE_EXPORT CustomElementUpgradeReaction final
    : public CustomElementReaction {
  WTF_MAKE_NONCOPYABLE(CustomElementUpgradeReaction);

 public:
  CustomElementUpgradeReaction(CustomElementDefinition*);

 private:
  void invoke(Element*) override;
};

}  // namespace blink

#endif  // CustomElementUpgradeReaction_h

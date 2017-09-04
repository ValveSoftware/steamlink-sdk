// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SVGResourceClient_h
#define SVGResourceClient_h

#include "core/CoreExport.h"
#include "core/loader/resource/DocumentResource.h"
#include "platform/heap/Handle.h"

namespace blink {

class TreeScope;

class CORE_EXPORT SVGResourceClient : public DocumentResourceClient {
 public:
  virtual ~SVGResourceClient() {}

  virtual TreeScope* treeScope() = 0;

  virtual void resourceContentChanged() = 0;
  virtual void resourceElementChanged() = 0;

 protected:
  SVGResourceClient() {}

  String debugName() const override { return "SVGResourceClient"; }
  void notifyFinished(Resource*) override { resourceElementChanged(); }
};

}  // namespace blink

#endif  // SVGResourceClient_h

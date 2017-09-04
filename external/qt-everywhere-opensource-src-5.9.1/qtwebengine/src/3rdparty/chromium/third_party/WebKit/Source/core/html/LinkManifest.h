// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LinkManifest_h
#define LinkManifest_h

#include "core/html/LinkResource.h"
#include "wtf/Allocator.h"
#include "wtf/RefPtr.h"

namespace blink {

class HTMLLinkElement;

class LinkManifest final : public LinkResource {
 public:
  static LinkManifest* create(HTMLLinkElement* owner);

  ~LinkManifest() override;

  // LinkResource
  void process() override;
  LinkResourceType type() const override { return Manifest; }
  bool hasLoaded() const override;
  void ownerRemoved() override;

 private:
  explicit LinkManifest(HTMLLinkElement* owner);
};

}  // namespace blink

#endif  // LinkManifest_h

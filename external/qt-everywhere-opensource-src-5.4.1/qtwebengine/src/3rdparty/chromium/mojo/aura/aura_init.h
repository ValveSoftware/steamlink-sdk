// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_AURA_AURA_INIT_MOJO_H_
#define MOJO_AURA_AURA_INIT_MOJO_H_

#include "base/memory/scoped_ptr.h"

namespace ui {
class ContextFactory;
}

namespace mojo {

class ScreenMojo;

// Sets up necessary state for aura when run with the viewmanager.
class AuraInit {
 public:
  AuraInit();
  ~AuraInit();

 private:
  scoped_ptr<ui::ContextFactory> context_factory_;
  scoped_ptr<ScreenMojo> screen_;

  DISALLOW_COPY_AND_ASSIGN(AuraInit);
};

}  // namespace mojo

#endif  // MOJO_AURA_AURA_INIT_MOJO_H_

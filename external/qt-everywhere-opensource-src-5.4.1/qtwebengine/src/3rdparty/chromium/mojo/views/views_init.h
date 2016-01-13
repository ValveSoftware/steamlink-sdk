// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_VIEWS_VIEWS_INIT_H_
#define MOJO_VIEWS_VIEWS_INIT_H_

#include "mojo/aura/aura_init.h"

namespace mojo {

// Sets up necessary state for views when run with the viewmanager.
class ViewsInit {
 public:
  ViewsInit();
  ~ViewsInit();

 private:
  AuraInit aura_init_;

  DISALLOW_COPY_AND_ASSIGN(ViewsInit);
};

}  // namespace mojo

#endif  // MOJO_VIEWS_VIEWS_INIT_H_

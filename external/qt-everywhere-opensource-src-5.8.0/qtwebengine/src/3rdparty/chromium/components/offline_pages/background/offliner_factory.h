// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_BACKGROUND_OFFLINER_FACTORY_H_
#define COMPONENTS_OFFLINE_PAGES_BACKGROUND_OFFLINER_FACTORY_H_

#include "base/macros.h"

namespace offline_pages {

class OfflinerPolicy;
class Offliner;

// An interface to a factory which can create a background offliner.
// The returned factory is owned by the offliner, which is allowed to cache the
// factory and return it again later.
class OfflinerFactory {
 public:
  OfflinerFactory() {}
  virtual ~OfflinerFactory() {}

  virtual Offliner* GetOffliner(const OfflinerPolicy* policy) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(OfflinerFactory);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_BACKGROUND_OFFLINER_FACTORY_H_

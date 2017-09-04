// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_CORE_FALLBACK_ICON_CLIENT_H_
#define COMPONENTS_FAVICON_CORE_FALLBACK_ICON_CLIENT_H_

#include <string>
#include <vector>

#include "components/keyed_service/core/keyed_service.h"

class GURL;

namespace favicon {

// This class abstracts operations that depend on the embedder's environment,
// e.g. Chrome.
class FallbackIconClient : public KeyedService {
 public:
  // Returns a list of font names for fallback icon rendering.
  virtual const std::vector<std::string>& GetFontNameList() const = 0;

 protected:
  ~FallbackIconClient() override {}
};

}  // namespace favicon

#endif  // COMPONENTS_FAVICON_CORE_FALLBACK_ICON_CLIENT_H_

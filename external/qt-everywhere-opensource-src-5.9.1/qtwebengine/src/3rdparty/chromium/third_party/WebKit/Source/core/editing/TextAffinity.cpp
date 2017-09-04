// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/TextAffinity.h"

#include <ostream>  // NOLINT

namespace blink {

std::ostream& operator<<(std::ostream& ostream, TextAffinity affinity) {
  switch (affinity) {
    case TextAffinity::Downstream:
      return ostream << "TextAffinity::Downstream";
    case TextAffinity::Upstream:
      return ostream << "TextAffinity::Upstream";
  }
  return ostream << "TextAffinity(" << static_cast<int>(affinity) << ')';
}

}  // namespace blink

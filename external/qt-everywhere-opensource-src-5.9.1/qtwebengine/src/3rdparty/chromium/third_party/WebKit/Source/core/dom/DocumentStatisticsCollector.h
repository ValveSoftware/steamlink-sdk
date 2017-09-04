// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DocumentStatisticsCollector_h
#define DocumentStatisticsCollector_h

#include "core/CoreExport.h"

namespace blink {

class Document;
struct WebDistillabilityFeatures;

class CORE_EXPORT DocumentStatisticsCollector {
 public:
  static WebDistillabilityFeatures collectStatistics(Document&);
};

}  // namespace blink

#endif

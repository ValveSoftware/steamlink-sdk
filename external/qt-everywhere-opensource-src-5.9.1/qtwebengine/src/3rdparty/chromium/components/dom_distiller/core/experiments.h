// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CORE_EXPERIMENTS_H_
#define COMPONENTS_DOM_DISTILLER_CORE_EXPERIMENTS_H_

namespace dom_distiller {
  enum class DistillerHeuristicsType {
    NONE,
    OG_ARTICLE,
    ADABOOST_MODEL,
    ALWAYS_TRUE,
  };

  DistillerHeuristicsType GetDistillerHeuristicsType();
  bool ShouldShowFeedbackForm();
}

#endif  // COMPONENTS_DOM_DISTILLER_CORE_EXPERIMENTS_H_

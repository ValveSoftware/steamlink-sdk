// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/WorkerSettings.h"

namespace blink {

WorkerSettings::WorkerSettings(Settings* settings) {
  if (settings)
    this->copyFlagValuesFromSettings(settings);
  else
    this->setDefaultValues();
}

void WorkerSettings::copyFlagValuesFromSettings(Settings* settings) {
  m_disableReadingFromCanvas = settings->disableReadingFromCanvas();
}

void WorkerSettings::setDefaultValues() {
  m_disableReadingFromCanvas = false;
}

}  // namespace blink

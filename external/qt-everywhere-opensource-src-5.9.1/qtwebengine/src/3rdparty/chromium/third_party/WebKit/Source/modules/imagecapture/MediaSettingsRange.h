// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaSettingsRange_h
#define MediaSettingsRange_h

#include "bindings/core/v8/ScriptWrappable.h"

namespace blink {

class MediaSettingsRange final : public GarbageCollected<MediaSettingsRange>,
                                 public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static MediaSettingsRange* create(double max,
                                    double min,
                                    double current,
                                    double step) {
    return new MediaSettingsRange(max, min, current, step);
  }

  double max() const { return m_max; }
  double min() const { return m_min; }
  double current() const { return m_current; }
  double step() const { return m_step; }

  DEFINE_INLINE_TRACE() {}

 private:
  MediaSettingsRange(double max, double min, double current, double step)
      : m_max(max), m_min(min), m_current(current), m_step(step) {}

  double m_max;
  double m_min;
  double m_current;
  double m_step;
};

}  // namespace blink

#endif  // MediaSettingsRange_h

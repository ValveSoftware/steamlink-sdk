// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_EDGE_EFFECT_H_
#define CONTENT_BROWSER_ANDROID_EDGE_EFFECT_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "ui/gfx/size_f.h"

namespace cc {
class Layer;
}

namespace content {

/* |EdgeEffect| mirrors its Android counterpart, EdgeEffect.java.
 * The primary difference is ownership; the Android version manages render
 * resources directly, while this version simply applies the effect to
 * existing resources. Conscious tradeoffs were made to align this as closely
 * as possible with the original Android java version.
 * All coordinates and dimensions are in device pixels.
 */
class EdgeEffect {
public:
  enum Edge {
    EDGE_TOP = 0,
    EDGE_LEFT,
    EDGE_BOTTOM,
    EDGE_RIGHT,
    EDGE_COUNT
  };

  EdgeEffect(scoped_refptr<cc::Layer> edge, scoped_refptr<cc::Layer> glow);
  ~EdgeEffect();

  void Pull(base::TimeTicks current_time, float delta_distance);
  void Absorb(base::TimeTicks current_time, float velocity);
  bool Update(base::TimeTicks current_time);
  void Release(base::TimeTicks current_time);

  void Finish();
  bool IsFinished() const;

  void ApplyToLayers(gfx::SizeF window_size,
                     Edge edge,
                     float edge_height,
                     float glow_height,
                     float offset);

private:

  enum State {
    STATE_IDLE = 0,
    STATE_PULL,
    STATE_ABSORB,
    STATE_RECEDE,
    STATE_PULL_DECAY
  };

  scoped_refptr<cc::Layer> edge_;
  scoped_refptr<cc::Layer> glow_;

  float edge_alpha_;
  float edge_scale_y_;
  float glow_alpha_;
  float glow_scale_y_;

  float edge_alpha_start_;
  float edge_alpha_finish_;
  float edge_scale_y_start_;
  float edge_scale_y_finish_;
  float glow_alpha_start_;
  float glow_alpha_finish_;
  float glow_scale_y_start_;
  float glow_scale_y_finish_;

  base::TimeTicks start_time_;
  base::TimeDelta duration_;

  State state_;

  float pull_distance_;

  DISALLOW_COPY_AND_ASSIGN(EdgeEffect);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_EDGE_EFFECT_H_

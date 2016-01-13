// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/base/switches.h"

#include "base/command_line.h"

namespace cc {
namespace switches {

const char kDisableThreadedAnimation[] = "disable-threaded-animation";

// Disables layer-edge anti-aliasing in the compositor.
const char kDisableCompositedAntialiasing[] =
    "disable-composited-antialiasing";

// Disables sending the next BeginMainFrame before the previous commit has
// drawn.
const char kDisableMainFrameBeforeDraw[] = "disable-main-frame-before-draw";

// Disables sending the next BeginMainFrame before the previous commit
// activates. Overrides the kEnableMainFrameBeforeActivation flag.
const char kDisableMainFrameBeforeActivation[] =
    "disable-main-frame-before-activation";

// Enables sending the next BeginMainFrame before the previous commit activates.
const char kEnableMainFrameBeforeActivation[] =
    "enable-main-frame-before-activation";

const char kEnableTopControlsPositionCalculation[] =
    "enable-top-controls-position-calculation";

// The height of the movable top controls.
const char kTopControlsHeight[] = "top-controls-height";

// Percentage of the top controls need to be hidden before they will auto hide.
const char kTopControlsHideThreshold[] = "top-controls-hide-threshold";

// Percentage of the top controls need to be shown before they will auto show.
const char kTopControlsShowThreshold[] = "top-controls-show-threshold";

// Re-rasters everything multiple times to simulate a much slower machine.
// Give a scale factor to cause raster to take that many times longer to
// complete, such as --slow-down-raster-scale-factor=25.
const char kSlowDownRasterScaleFactor[] = "slow-down-raster-scale-factor";

// Max tiles allowed for each tilings interest area.
const char kMaxTilesForInterestArea[] = "max-tiles-for-interest-area";

// The amount of unused resource memory compositor is allowed to keep around.
const char kMaxUnusedResourceMemoryUsagePercentage[] =
    "max-unused-resource-memory-usage-percentage";

// Causes the compositor to render to textures which are then sent to the parent
// through the texture mailbox mechanism.
// Requires --enable-compositor-frame-message.
const char kCompositeToMailbox[] = "composite-to-mailbox";

// Check that property changes during paint do not occur.
const char kStrictLayerPropertyChangeChecking[] =
    "strict-layer-property-change-checking";

// Virtual viewport for fixed-position elements, scrollbars during pinch.
const char kEnablePinchVirtualViewport[] = "enable-pinch-virtual-viewport";
const char kDisablePinchVirtualViewport[] = "disable-pinch-virtual-viewport";

// Disable partial swap which is needed for some OpenGL drivers / emulators.
const char kUIDisablePartialSwap[] = "ui-disable-partial-swap";

// Enables the GPU benchmarking extension
const char kEnableGpuBenchmarking[] = "enable-gpu-benchmarking";

// Renders a border around compositor layers to help debug and study
// layer compositing.
const char kShowCompositedLayerBorders[] = "show-composited-layer-borders";
const char kUIShowCompositedLayerBorders[] = "ui-show-layer-borders";

// Draws a FPS indicator
const char kShowFPSCounter[] = "show-fps-counter";
const char kUIShowFPSCounter[] = "ui-show-fps-counter";

// Renders a border that represents the bounding box for the layer's animation.
const char kShowLayerAnimationBounds[] = "show-layer-animation-bounds";
const char kUIShowLayerAnimationBounds[] = "ui-show-layer-animation-bounds";

// Show rects in the HUD around layers whose properties have changed.
const char kShowPropertyChangedRects[] = "show-property-changed-rects";
const char kUIShowPropertyChangedRects[] = "ui-show-property-changed-rects";

// Show rects in the HUD around damage as it is recorded into each render
// surface.
const char kShowSurfaceDamageRects[] = "show-surface-damage-rects";
const char kUIShowSurfaceDamageRects[] = "ui-show-surface-damage-rects";

// Show rects in the HUD around the screen-space transformed bounds of every
// layer.
const char kShowScreenSpaceRects[] = "show-screenspace-rects";
const char kUIShowScreenSpaceRects[] = "ui-show-screenspace-rects";

// Show rects in the HUD around the screen-space transformed bounds of every
// layer's replica, when they have one.
const char kShowReplicaScreenSpaceRects[] = "show-replica-screenspace-rects";
const char kUIShowReplicaScreenSpaceRects[] =
    "ui-show-replica-screenspace-rects";

// Show rects in the HUD wherever something is known to be drawn opaque and is
// considered occluding the pixels behind it.
const char kShowOccludingRects[] = "show-occluding-rects";
const char kUIShowOccludingRects[] = "ui-show-occluding-rects";

// Show rects in the HUD wherever something is not known to be drawn opaque and
// is not considered to be occluding the pixels behind it.
const char kShowNonOccludingRects[] = "show-nonoccluding-rects";
const char kUIShowNonOccludingRects[] = "ui-show-nonoccluding-rects";

// Prevents the layer tree unit tests from timing out.
const char kCCLayerTreeTestNoTimeout[] = "cc-layer-tree-test-no-timeout";

// Makes pixel tests write their output instead of read it.
const char kCCRebaselinePixeltests[] = "cc-rebaseline-pixeltests";

// Disable touch hit testing in the compositor.
const char kDisableCompositorTouchHitTesting[] =
    "disable-compositor-touch-hit-testing";

}  // namespace switches
}  // namespace cc

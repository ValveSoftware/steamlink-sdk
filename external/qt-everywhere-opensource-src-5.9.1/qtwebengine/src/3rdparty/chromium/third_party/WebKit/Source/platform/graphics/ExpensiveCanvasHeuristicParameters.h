// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ExpensiveCanvasHeuristicParameters_h
#define ExpensiveCanvasHeuristicParameters_h

namespace blink {

namespace ExpensiveCanvasHeuristicParameters {

enum {
  // Layer promotion heuristic parameters
  //======================================

  // FIXME (crbug.com/463239):
  // The Layer promotion heuristics should go away after slimming paint
  // is completely phased in and display list canvases are modified to
  // use a lightweight layering primitive instead of the
  // SkCanvas::saveLayer.

  // Heuristic: Canvases that are overdrawn beyond this factor in a
  // single frame are promoted to a direct composited layer so that
  // their contents not be re-rasterized by the compositor when the
  // containing layer is the object of a paint invalidation.
  ExpensiveOverdrawThreshold = 3,

  ExpensivePathPointCount = 50,

  SVGImageSourcesAreExpensive = 1,

  ConcavePathsAreExpensive = 1,

  ComplexClipsAreExpensive = 1,

  BlurredShadowsAreExpensive = 1,

  // Heuristic: When drawing a source image that has more pixels than
  // the destination canvas by the following factor or more, the draw
  // is considered expensive.
  ExpensiveImageSizeRatio = 4,

  // Display list fallback heuristic parameters
  //============================================

  // Frames ending with more than this number of levels remaining
  // on the state stack at the end of a frame are too expensive to
  // remain in display list mode. This criterion is motivated by an
  // O(N) cost in carying over state from one frame to the next when
  // in display list mode. The value of this parameter should be high
  // enough to almost never kick in other than for cases with unmatched
  // save()/restore() calls are low enough to kick in before state
  // management becomes measurably expensive.
  ExpensiveRecordingStackDepth = 50,

  // GPU vs. display list heuristic parameters
  //===========================================

  // Pixel count beyond which we should always prefer to use display
  // lists. Rationale: The allocation of large textures for canvas
  // tends to starve the compositor, and increase the probability of
  // failure of subsequent allocations required for double buffering.
  PreferDisplayListOverGpuSizeThreshold = 4096 * 4096,

  // Disable Acceleration heuristic parameters
  //===========================================

  GetImageDataForcesNoAcceleration = 1,

  // When a canvas is used as a source image, if its destination is
  // non-accelerated and the source canvas is accelerated, a readback
  // from the gpu is necessary. This option causes the source canvas to
  // switch to non-accelerated when this situation is encountered to
  // prevent future canvas-to-canvas draws from requiring a readback.
  DisableAccelerationToAvoidReadbacks = 1,

  // When drawing very large images to canvases, there is a point where
  // GPU acceleration becomes inefficient due to texture upload overhead,
  // especially when the image is large enough that it is likely to
  // monopolize the texture cache, and when it is being downsized to the
  // point that few of the upload texels are actually sampled. When both
  // of these conditions are met, we disable acceleration.
  DrawImageTextureUploadSoftSizeLimit = 4096 * 4096,
  DrawImageTextureUploadSoftSizeLimitScaleThreshold = 4,
  DrawImageTextureUploadHardSizeLimit = 8192 * 8192,

};  // enum

// Constants and Coefficients for 2D Canvas Dynamic Rendering Mode Switching
// =========================================================================

// Approximate relative costs of different types of operations for the
// accelerated rendering pipeline and the recording rendering pipeline.
// These costs were estimated experimentally using the tools located in the
// third_party/WebKit/Source/modules/canvas2d/performance_analysis directory.

// The RenderingModeCostIndex enum is used to access the heuristic coefficients
// that correspond to a given rendering mode. For exmaple,
// FillRectFixedCost[RecordingModeIndex] is the estimated fixed cost for
// FillRect in recording mode.
enum RenderingModeCostIndex {
  RecordingModeIndex = 0,
  AcceleratedModeIndex = 1,
  NumRederingModesCostIdexes = 2
};

const float FillRectFixedCost[NumRederingModesCostIdexes] = {6.190e-03f,
                                                             7.715e-03f};
const float FillConvexPathFixedCost[NumRederingModesCostIdexes] = {1.251e-02f,
                                                                   1.231e-02f};
const float FillNonConvexPathFixedCost[NumRederingModesCostIdexes] = {
    1.714e-02f, 4.497e-02f};
const float FillTextFixedCost[NumRederingModesCostIdexes] = {1.119e-02f,
                                                             2.203e-02f};

const float StrokeRectFixedCost[NumRederingModesCostIdexes] = {1.485e-02f,
                                                               7.287e-03f};
const float StrokePathFixedCost[NumRederingModesCostIdexes] = {2.390e-02f,
                                                               5.125e-02f};
const float StrokeTextFixedCost[NumRederingModesCostIdexes] = {1.149e-02f,
                                                               1.742e-02f};

const float FillRectVariableCostPerArea[NumRederingModesCostIdexes] = {
    2.933e-07f, 2.188e-09f};
const float FillConvexPathVariableCostPerArea[NumRederingModesCostIdexes] = {
    7.871e-07f, 1.608e-07f};
const float FillNonConvexPathVariableCostPerArea[NumRederingModesCostIdexes] = {
    8.336e-07f, 1.384e-06f};
const float FillTextVariableCostPerArea[NumRederingModesCostIdexes] = {
    1.411e-06f, 0.0f};

const float StrokeRectVariableCostPerArea[NumRederingModesCostIdexes] = {
    9.882e-07f, 0.0f};
const float StrokePathVariableCostPerArea[NumRederingModesCostIdexes] = {
    1.583e-06f, 2.401e-06f};
const float StrokeTextVariableCostPerArea[NumRederingModesCostIdexes] = {
    1.530e-06f, 6.699e-07f};

const float PatternFillTypeFixedCost[NumRederingModesCostIdexes] = {1.377e-02f,
                                                                    1.035e-02f};
const float LinearGradientFillTypeFixedCost[NumRederingModesCostIdexes] = {
    7.694e-03f, 6.900e-03f};
const float RadialGradientFillTypeFixedCost[NumRederingModesCostIdexes] = {
    2.260e-02f, 7.193e-03f};

const float PatternFillTypeVariableCostPerArea[NumRederingModesCostIdexes] = {
    6.080e-07f, 0.0f};
const float LinearGradientFillVariableCostPerArea[NumRederingModesCostIdexes] =
    {9.635e-07f, 0.0f};
const float RadialGradientFillVariableCostPerArea[NumRederingModesCostIdexes] =
    {6.662e-06f, 0.0f};

const float ShadowFixedCost[NumRederingModesCostIdexes] = {2.502e-02f,
                                                           2.274e-02f};
const float ShadowVariableCostPerAreaTimesShadowBlurSquared
    [NumRederingModesCostIdexes] = {6.856e-09f, 0.0f};

const float PutImageDataFixedCost[NumRederingModesCostIdexes] = {1.209e-03f,
                                                                 1.885e-02f};
const float PutImageDataVariableCostPerArea[NumRederingModesCostIdexes] = {
    6.231e-06f, 4.116e-06f};

const float DrawSVGImageFixedCost[NumRederingModesCostIdexes] = {1.431e-01f,
                                                                 2.958e-01f};
const float DrawPNGImageFixedCost[NumRederingModesCostIdexes] = {1.278e-02f,
                                                                 1.306e-02f};

const float DrawSVGImageVariableCostPerArea[NumRederingModesCostIdexes] = {
    1.030e-05f, 4.463e-06f};
const float DrawPNGImageVariableCostPerArea[NumRederingModesCostIdexes] = {
    1.727e-06f, 0.0f};

// Two conditions must be met before the isAccelerationOptimalForCanvasContent
// heuristics recommends switching out of the accelerated mode:
//   1. The difference in estimated cost per frame is larger than
//      MinCostPerFrameImprovementToSuggestDisableAcceleration. This ensures
//      that the overhead involved in a switch of rendering mode and the risk of
//      making a wrong decision are justified by a large expected increased
//      performance.
//   2. The percent reduction in rendering cost is larger than
//      MinPercentageImprovementToSuggestDisableAcceleration. This ensures that
//      there is a high level of confidence that the performance would be
//      improved in recording mode.
const float MinCostPerFrameImprovementToSuggestDisableAcceleration = 15.0f;
const float MinPercentageImprovementToSuggestDisableAcceleration = 30.0f;

// Minimum number of frames that need to be rendered
// before the rendering pipeline may be switched. Having this set
// to more than 1 increases the sample size of usage data before a
// decision is made, improving the accuracy of heuristics.
const int MinFramesBeforeSwitch = 3;

}  // namespace ExpensiveCanvasHeuristicParameters

}  // namespace blink

#endif

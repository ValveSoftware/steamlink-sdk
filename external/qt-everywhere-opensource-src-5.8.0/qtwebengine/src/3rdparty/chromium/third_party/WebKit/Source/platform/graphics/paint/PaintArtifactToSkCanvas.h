// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintArtifactToSkCanvas_h
#define PaintArtifactToSkCanvas_h

#include "platform/PlatformExport.h"
#include "third_party/skia/include/core/SkRefCnt.h"

class SkCanvas;
class SkPicture;
struct SkRect;

namespace blink {

class PaintArtifact;

// Paints a paint artifact to an SkCanvas from the paint chunks and their
// associated properties, rather than from the transform (etc.) display items in
// the display item list.
//
// This is limited to Skia commands, and so is not full-featured. In particular,
// Skia does not support 3D transforms, which require support from the
// compositor.
PLATFORM_EXPORT void paintArtifactToSkCanvas(const PaintArtifact&, SkCanvas*);

// Using the previous, converts the paint artifact to an SkPicture.
PLATFORM_EXPORT sk_sp<const SkPicture> paintArtifactToSkPicture(
    const PaintArtifact&, const SkRect& bounds);

} // namespace blink

#endif // PaintArtifactToSkCanvas_h

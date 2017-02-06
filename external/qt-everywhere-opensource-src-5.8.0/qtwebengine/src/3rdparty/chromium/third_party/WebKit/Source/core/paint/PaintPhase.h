/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 *           (C) 2004 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef PaintPhase_h
#define PaintPhase_h

namespace blink {

//  The painting of a layer occurs in 4 phases, Each involves a recursive descent
// into the layer's layout objects in painting order:
//  1. Background phase: backgrounds and borders of all blocks are painted.
//     Inlines are not painted at all.
//  2. Float phase: floating objects are painted above block backgrounds but entirely
//     below inline content that can overlap them.
//  3. Foreground phase: all inlines are fully painted. Atomic inline elements will
//     get all 4 phases invoked on them during this phase, as if they were stacking
//     contexts (see ObjectPainter::paintAllPhasesAtomically()).
//  4. Outline phase: outlines are painted over the foreground.

enum PaintPhase {
    // Background phase
    //
    // Paint background of the current object and non-self-painting descendants.
    PaintPhaseBlockBackground = 0,
    //
    // The following two values are added besides the normal PaintPhaseBlockBackground
    // to distinguish backgrounds for the object itself and for descendants, because
    // the two backgrounds are often painted with different scroll offsets and clips.
    //
    // Paint background of the current object only.
    PaintPhaseSelfBlockBackgroundOnly = 1,
    // Paint backgrounds of non-self-painting descendants only. The painter should call
    // each non-self-painting child's paint method by passing paintInfo.forDescendants() which
    // converts PaintPhaseDescendantsBlockBackgroundsOnly to PaintPhaseBlockBackground.
    PaintPhaseDescendantBlockBackgroundsOnly = 2,

    // Float phase
    PaintPhaseFloat = 3,

    // Foreground phase
    PaintPhaseForeground = 4,

    // Outline phase
    //
    // Paint outline for the current object and non-self-painting descendants.
    PaintPhaseOutline = 5,
    //
    // Similar to the background phase, the following two values are added for painting
    // outlines of the object itself and for descendants.
    //
    // Paint outline for the current object only.
    PaintPhaseSelfOutlineOnly = 6,
    // Paint outlines of non-self-painting descendants only. The painter should call each
    // non-self-painting child's paint method by passing paintInfo.forDescendants() which
    // converts PaintPhaseDescendantsOutlinesOnly to PaintPhaseBlockOutline.
    PaintPhaseDescendantOutlinesOnly = 7,

    // The below are auxiliary phases which are used to paint special effects.
    PaintPhaseSelection = 8,
    PaintPhaseTextClip = 9,
    PaintPhaseMask = 10,
    PaintPhaseClippingMask = 11,

    PaintPhaseMax = PaintPhaseClippingMask,
    // These values must be kept in sync with DisplayItem::Type and DisplayItem::typeAsDebugString().
};

inline bool shouldPaintSelfBlockBackground(PaintPhase phase)
{
    return phase == PaintPhaseBlockBackground || phase == PaintPhaseSelfBlockBackgroundOnly;
}

inline bool shouldPaintSelfOutline(PaintPhase phase)
{
    return phase == PaintPhaseOutline || phase == PaintPhaseSelfOutlineOnly;
}

inline bool shouldPaintDescendantBlockBackgrounds(PaintPhase phase)
{
    return phase == PaintPhaseBlockBackground || phase == PaintPhaseDescendantBlockBackgroundsOnly;
}

inline bool shouldPaintDescendantOutlines(PaintPhase phase)
{
    return phase == PaintPhaseOutline || phase == PaintPhaseDescendantOutlinesOnly;
}

// Those flags are meant as global tree operations. This means
// that they should be constant for a paint phase.
enum GlobalPaintFlag {
    GlobalPaintNormalPhase = 0,
    // Used when painting selection as part of a drag-image. This
    // flag disables a lot of the painting code and specifically
    // triggers a PaintPhaseSelection.
    GlobalPaintSelectionOnly = 1 << 0,
    // Used when painting a drag-image or printing in order to
    // ignore the hardware layers and paint the whole tree
    // into the topmost layer.
    GlobalPaintFlattenCompositingLayers = 1 << 1,
    // Used when printing in order to adapt the output to the medium, for
    // instance by not painting shadows and selections on text, and add
    // URL metadata for links.
    GlobalPaintPrinting = 1 << 2
};

typedef unsigned GlobalPaintFlags;

} // namespace blink

#endif // PaintPhase_h

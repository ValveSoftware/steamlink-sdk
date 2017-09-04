/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc.
 * All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
 */

#ifndef CSSToStyleMap_h
#define CSSToStyleMap_h

#include "core/CSSPropertyNames.h"
#include "core/animation/Timing.h"
#include "core/animation/css/CSSTransitionData.h"
#include "core/style/ComputedStyleConstants.h"
#include "platform/animation/TimingFunction.h"
#include "wtf/Allocator.h"

namespace blink {

class FillLayer;
class CSSValue;
class StyleResolverState;
class NinePieceImage;
class BorderImageLengthBox;

class CSSToStyleMap {
  STATIC_ONLY(CSSToStyleMap);

 public:
  static void mapFillAttachment(StyleResolverState&,
                                FillLayer*,
                                const CSSValue&);
  static void mapFillClip(StyleResolverState&, FillLayer*, const CSSValue&);
  static void mapFillComposite(StyleResolverState&,
                               FillLayer*,
                               const CSSValue&);
  static void mapFillBlendMode(StyleResolverState&,
                               FillLayer*,
                               const CSSValue&);
  static void mapFillOrigin(StyleResolverState&, FillLayer*, const CSSValue&);
  static void mapFillImage(StyleResolverState&, FillLayer*, const CSSValue&);
  static void mapFillRepeatX(StyleResolverState&, FillLayer*, const CSSValue&);
  static void mapFillRepeatY(StyleResolverState&, FillLayer*, const CSSValue&);
  static void mapFillSize(StyleResolverState&, FillLayer*, const CSSValue&);
  static void mapFillXPosition(StyleResolverState&,
                               FillLayer*,
                               const CSSValue&);
  static void mapFillYPosition(StyleResolverState&,
                               FillLayer*,
                               const CSSValue&);
  static void mapFillMaskSourceType(StyleResolverState&,
                                    FillLayer*,
                                    const CSSValue&);

  static double mapAnimationDelay(const CSSValue&);
  static Timing::PlaybackDirection mapAnimationDirection(const CSSValue&);
  static double mapAnimationDuration(const CSSValue&);
  static Timing::FillMode mapAnimationFillMode(const CSSValue&);
  static double mapAnimationIterationCount(const CSSValue&);
  static AtomicString mapAnimationName(const CSSValue&);
  static EAnimPlayState mapAnimationPlayState(const CSSValue&);
  static CSSTransitionData::TransitionProperty mapAnimationProperty(
      const CSSValue&);
  static PassRefPtr<TimingFunction> mapAnimationTimingFunction(
      const CSSValue&,
      bool allowStepMiddle = false);

  static void mapNinePieceImage(StyleResolverState&,
                                CSSPropertyID,
                                const CSSValue&,
                                NinePieceImage&);
  static void mapNinePieceImageSlice(StyleResolverState&,
                                     const CSSValue&,
                                     NinePieceImage&);
  static BorderImageLengthBox mapNinePieceImageQuad(StyleResolverState&,
                                                    const CSSValue&);
  static void mapNinePieceImageRepeat(StyleResolverState&,
                                      const CSSValue&,
                                      NinePieceImage&);
};

}  // namespace blink

#endif

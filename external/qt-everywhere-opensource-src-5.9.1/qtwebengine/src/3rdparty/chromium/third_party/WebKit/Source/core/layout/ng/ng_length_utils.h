// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGLengthUtils_h
#define NGLengthUtils_h

#include "core/CoreExport.h"
#include "core/layout/ng/ng_units.h"
#include "core/layout/ng/ng_writing_mode.h"
#include "platform/text/TextDirection.h"
#include "wtf/Optional.h"

namespace blink {
class ComputedStyle;
class LayoutUnit;
class Length;
struct MinAndMaxContentSizes;
class NGConstraintSpace;
struct NGBoxStrut;
class NGFragmentBase;

enum class LengthResolveType {
  MinSize,
  MaxSize,
  ContentSize,
  MarginBorderPaddingSize
};

#define NGSizeIndefinite LayoutUnit(-1)

// Whether the caller needs to compute min-content and max-content sizes to
// pass them to ResolveInlineLength / ComputeInlineSizeForFragment.
// If this function returns false, it is safe to pass an empty
// MinAndMaxContentSizes struct to those functions.
CORE_EXPORT bool NeedMinAndMaxContentSizes(const ComputedStyle&);

// Convert an inline-axis length to a layout unit using the given constraint
// space.
CORE_EXPORT LayoutUnit
ResolveInlineLength(const NGConstraintSpace&,
                    const ComputedStyle&,
                    const WTF::Optional<MinAndMaxContentSizes>&,
                    const Length&,
                    LengthResolveType);

// Convert a block-axis length to a layout unit using the given constraint
// space and content size.
CORE_EXPORT LayoutUnit ResolveBlockLength(const NGConstraintSpace&,
                                          const ComputedStyle&,
                                          const Length&,
                                          LayoutUnit contentSize,
                                          LengthResolveType);

// Resolves the given length to a layout unit, constraining it by the min
// logical width and max logical width properties from the ComputedStyle
// object.
CORE_EXPORT LayoutUnit
ComputeInlineSizeForFragment(const NGConstraintSpace&,
                             const ComputedStyle&,
                             const WTF::Optional<MinAndMaxContentSizes>&);

// Resolves the given length to a layout unit, constraining it by the min
// logical height and max logical height properties from the ComputedStyle
// object.
CORE_EXPORT LayoutUnit ComputeBlockSizeForFragment(const NGConstraintSpace&,
                                                   const ComputedStyle&,
                                                   LayoutUnit contentSize);

CORE_EXPORT NGBoxStrut ComputeMargins(const NGConstraintSpace&,
                                      const ComputedStyle&,
                                      const NGWritingMode writing_mode,
                                      const TextDirection direction);

CORE_EXPORT NGBoxStrut ComputeBorders(const ComputedStyle&);

CORE_EXPORT NGBoxStrut ComputePadding(const NGConstraintSpace&,
                                      const ComputedStyle&);

// Resolves margin: auto in the inline direction after a box has been laid out.
// This uses the available size from the constraint space and the box size from
// the fragment to compute the margins that are auto, if any, and adjusts
// the given NGBoxStrut accordingly.
CORE_EXPORT void ApplyAutoMargins(const NGConstraintSpace&,
                                  const ComputedStyle&,
                                  const NGFragmentBase&,
                                  NGBoxStrut* margins);

}  // namespace blink

#endif  // NGLengthUtils_h

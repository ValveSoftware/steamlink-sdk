// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintPrinters_h
#define PaintPrinters_h

#include <iosfwd>

namespace blink {

struct PaintChunk;
struct PaintProperties;
class ClipPaintPropertyNode;
class TransformPaintPropertyNode;
class EffectPaintPropertyNode;
class ScrollPaintPropertyNode;

// GTest print support for platform paint classes.
//
// To avoid ODR violations, these should also be declared in the respective
// headers defining these types. This is required because otherwise a template
// instantiation may be instantiated differently, depending on whether this
// declaration is found.
//
// As a result, it is not necessary to include this file in tests in order to
// use these printers. If, however, you get a link error about these symbols,
// you need to make sure the blink_platform_test_support target is linked in
// your unit test binary.
void PrintTo(const ClipPaintPropertyNode&, std::ostream*);
void PrintTo(const PaintChunk&, std::ostream*);
void PrintTo(const PaintProperties&, std::ostream*);
void PrintTo(const TransformPaintPropertyNode&, std::ostream*);
void PrintTo(const EffectPaintPropertyNode&, std::ostream*);
void PrintTo(const ScrollPaintPropertyNode&, std::ostream*);

}  // namespace blink

#endif  // PaintPrinters_h

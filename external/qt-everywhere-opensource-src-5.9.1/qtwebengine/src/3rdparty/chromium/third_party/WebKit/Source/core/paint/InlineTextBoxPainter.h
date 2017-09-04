// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InlineTextBoxPainter_h
#define InlineTextBoxPainter_h

#include "core/style/ComputedStyleConstants.h"
#include "platform/geometry/LayoutRect.h"
#include "wtf/Allocator.h"

namespace blink {

struct PaintInfo;

class Color;
class CompositionUnderline;
class ComputedStyle;
class DocumentMarker;
class Font;
class GraphicsContext;
class InlineTextBox;
class LayoutObject;
class LayoutPoint;
class LayoutTextCombine;

enum class DocumentMarkerPaintPhase { Foreground, Background };

class InlineTextBoxPainter {
  STACK_ALLOCATED();

 public:
  InlineTextBoxPainter(const InlineTextBox& inlineTextBox)
      : m_inlineTextBox(inlineTextBox) {}

  void paint(const PaintInfo&, const LayoutPoint&);
  void paintDocumentMarkers(const PaintInfo&,
                            const LayoutPoint& boxOrigin,
                            const ComputedStyle&,
                            const Font&,
                            DocumentMarkerPaintPhase);
  void paintDocumentMarker(GraphicsContext&,
                           const LayoutPoint& boxOrigin,
                           DocumentMarker*,
                           const ComputedStyle&,
                           const Font&,
                           bool grammar);
  void paintTextMatchMarkerForeground(const PaintInfo&,
                                      const LayoutPoint& boxOrigin,
                                      DocumentMarker*,
                                      const ComputedStyle&,
                                      const Font&);
  void paintTextMatchMarkerBackground(const PaintInfo&,
                                      const LayoutPoint& boxOrigin,
                                      DocumentMarker*,
                                      const ComputedStyle&,
                                      const Font&);

  static void removeFromTextBlobCache(const InlineTextBox&);
  static bool paintsMarkerHighlights(const LayoutObject&);

 private:
  enum class PaintOptions { Normal, CombinedText };

  void paintCompositionBackgrounds(GraphicsContext&,
                                   const LayoutPoint& boxOrigin,
                                   const ComputedStyle&,
                                   const Font&,
                                   bool useCustomUnderlines);
  void paintSingleCompositionBackgroundRun(GraphicsContext&,
                                           const LayoutPoint& boxOrigin,
                                           const ComputedStyle&,
                                           const Font&,
                                           Color backgroundColor,
                                           int startPos,
                                           int endPos);
  template <PaintOptions>
  void paintSelection(GraphicsContext&,
                      const LayoutRect& boxRect,
                      const ComputedStyle&,
                      const Font&,
                      Color textColor,
                      LayoutTextCombine* = nullptr);
  void paintDecoration(const PaintInfo&,
                       const LayoutPoint& boxOrigin,
                       TextDecoration);
  void paintCompositionUnderline(GraphicsContext&,
                                 const LayoutPoint& boxOrigin,
                                 const CompositionUnderline&);
  unsigned underlinePaintStart(const CompositionUnderline&);
  unsigned underlinePaintEnd(const CompositionUnderline&);
  bool shouldPaintTextBox(const PaintInfo&);
  void expandToIncludeNewlineForSelection(LayoutRect&);
  LayoutObject& inlineLayoutObject() const;

  const InlineTextBox& m_inlineTextBox;
};

}  // namespace blink

#endif  // InlineTextBoxPainter_h

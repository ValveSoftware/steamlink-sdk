/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2009, 2013 Apple Inc. All rights reserved.
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

#ifndef LayoutText_h
#define LayoutText_h

#include "core/CoreExport.h"
#include "core/dom/Text.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/TextRunConstructor.h"
#include "platform/LengthFunctions.h"
#include "wtf/Forward.h"
#include "wtf/PassRefPtr.h"

namespace blink {

class AbstractInlineTextBox;
class InlineTextBox;

// LayoutText is the root class for anything that represents
// a text node (see core/dom/Text.h).
//
// This is a common node in the tree so to the limit memory overhead,
// this class inherits directly from LayoutObject.
// Also this class is used by both CSS and SVG layouts so LayoutObject
// was a natural choice.
//
// The actual layout of text is handled by the containing inline
// (LayoutInline) or block (LayoutBlockFlow). They will invoke the Unicode
// Bidirectional Algorithm to break the text into actual lines.
// The result of layout is the line box tree, which represents lines
// on the screen. It is stored into m_firstTextBox and m_lastTextBox.
// To understand how lines are broken by the bidi algorithm, read e.g.
// LayoutBlockFlow::layoutInlineChildren.
//
//
// ***** LINE BOXES OWNERSHIP *****
// m_firstTextBox and m_lastTextBox are not owned by LayoutText
// but are pointers into the enclosing inline / block (see LayoutInline's
// and LayoutBlockFlow's m_lineBoxes).
//
//
// This class implements the preferred logical widths computation
// for its underlying text. The widths are stored into m_minWidth
// and m_maxWidth. They are computed lazily based on
// m_preferredLogicalWidthsDirty.
//
// The previous comment applies also for painting. See e.g.
// BlockFlowPainter::paintContents in particular the use of LineBoxListPainter.
class CORE_EXPORT LayoutText : public LayoutObject {
 public:
  // FIXME: If the node argument is not a Text node or the string argument is
  // not the content of the Text node, updating text-transform property
  // doesn't re-transform the string.
  LayoutText(Node*, PassRefPtr<StringImpl>);
#if ENABLE(ASSERT)
  ~LayoutText() override;
#endif

  const char* name() const override { return "LayoutText"; }

  virtual bool isTextFragment() const;
  virtual bool isWordBreak() const;

  virtual PassRefPtr<StringImpl> originalText() const;

  void extractTextBox(InlineTextBox*);
  void attachTextBox(InlineTextBox*);
  void removeTextBox(InlineTextBox*);

  const String& text() const { return m_text; }
  virtual unsigned textStartOffset() const { return 0; }
  String plainText() const;

  InlineTextBox* createInlineTextBox(int start, unsigned short length);
  void dirtyOrDeleteLineBoxesIfNeeded(bool fullLayout);
  void dirtyLineBoxes();

  void absoluteRects(Vector<IntRect>&,
                     const LayoutPoint& accumulatedOffset) const final;
  void absoluteRectsForRange(Vector<IntRect>&,
                             unsigned startOffset = 0,
                             unsigned endOffset = INT_MAX,
                             bool useSelectionHeight = false);

  void absoluteQuads(Vector<FloatQuad>&) const final;
  void absoluteQuadsForRange(Vector<FloatQuad>&,
                             unsigned startOffset = 0,
                             unsigned endOffset = INT_MAX,
                             bool useSelectionHeight = false);
  FloatRect localBoundingBoxRectForAccessibility() const final;

  enum ClippingOption { NoClipping, ClipToEllipsis };
  enum LocalOrAbsoluteOption { LocalQuads, AbsoluteQuads };
  void quads(Vector<FloatQuad>&,
             ClippingOption = NoClipping,
             LocalOrAbsoluteOption = AbsoluteQuads) const;

  PositionWithAffinity positionForPoint(const LayoutPoint&) override;

  bool is8Bit() const { return m_text.is8Bit(); }
  const LChar* characters8() const { return m_text.impl()->characters8(); }
  const UChar* characters16() const { return m_text.impl()->characters16(); }
  bool hasEmptyText() const { return m_text.isEmpty(); }
  UChar characterAt(unsigned) const;
  UChar uncheckedCharacterAt(unsigned) const;
  UChar operator[](unsigned i) const { return uncheckedCharacterAt(i); }
  UChar32 codepointAt(unsigned) const;
  unsigned textLength() const {
    return m_text.length();
  }  // non virtual implementation of length()
  bool containsOnlyWhitespace(unsigned from, unsigned len) const;
  void positionLineBox(InlineBox*);

  virtual float width(unsigned from,
                      unsigned len,
                      const Font&,
                      LayoutUnit xPos,
                      TextDirection,
                      HashSet<const SimpleFontData*>* fallbackFonts = nullptr,
                      FloatRect* glyphBounds = nullptr) const;
  virtual float width(unsigned from,
                      unsigned len,
                      LayoutUnit xPos,
                      TextDirection,
                      bool firstLine = false,
                      HashSet<const SimpleFontData*>* fallbackFonts = nullptr,
                      FloatRect* glyphBounds = nullptr) const;

  float minLogicalWidth() const;
  float maxLogicalWidth() const;

  void trimmedPrefWidths(LayoutUnit leadWidth,
                         LayoutUnit& firstLineMinWidth,
                         bool& hasBreakableStart,
                         LayoutUnit& lastLineMinWidth,
                         bool& hasBreakableEnd,
                         bool& hasBreakableChar,
                         bool& hasBreak,
                         LayoutUnit& firstLineMaxWidth,
                         LayoutUnit& lastLineMaxWidth,
                         LayoutUnit& minWidth,
                         LayoutUnit& maxWidth,
                         bool& stripFrontSpaces,
                         TextDirection);

  virtual LayoutRect linesBoundingBox() const;

  // Returns the bounding box of visual overflow rects of all line boxes.
  LayoutRect visualOverflowRect() const;

  FloatPoint firstRunOrigin() const;
  float firstRunX() const;
  float firstRunY() const;

  virtual void setText(PassRefPtr<StringImpl>, bool force = false);
  void setTextWithOffset(PassRefPtr<StringImpl>,
                         unsigned offset,
                         unsigned len,
                         bool force = false);

  virtual void transformText();

  bool canBeSelectionLeaf() const override { return true; }
  void setSelectionState(SelectionState) final;
  LayoutRect localSelectionRect() const final;
  LayoutRect localCaretRect(
      InlineBox*,
      int caretOffset,
      LayoutUnit* extraWidthToEndOfLine = nullptr) override;

  InlineTextBox* firstTextBox() const { return m_firstTextBox; }
  InlineTextBox* lastTextBox() const { return m_lastTextBox; }

  // True if we have inline text box children which implies rendered text (or
  // whitespace) output.
  bool hasTextBoxes() const { return firstTextBox(); }

  int caretMinOffset() const override;
  int caretMaxOffset() const override;
  unsigned resolvedTextLength() const;

  bool containsReversedText() const { return m_containsReversedText; }

  bool isSecure() const { return style()->textSecurity() != TSNONE; }
  void momentarilyRevealLastTypedCharacter(unsigned lastTypedCharacterOffset);

  bool isAllCollapsibleWhitespace() const;
  bool isRenderedCharacter(int offsetInNode) const;

  void removeAndDestroyTextBoxes();

  PassRefPtr<AbstractInlineTextBox> firstAbstractInlineTextBox();

  float hyphenWidth(const Font&, TextDirection);

  LayoutRect debugRect() const override;

 protected:
  void willBeDestroyed() override;

  void styleWillChange(StyleDifference, const ComputedStyle&) final {}
  void styleDidChange(StyleDifference, const ComputedStyle* oldStyle) override;

  virtual void setTextInternal(PassRefPtr<StringImpl>);
  virtual UChar previousCharacter() const;

  void addLayerHitTestRects(LayerHitTestRects&,
                            const PaintLayer* currentLayer,
                            const LayoutPoint& layerOffset,
                            const LayoutRect& containerRect) const override;

  virtual InlineTextBox* createTextBox(
      int start,
      unsigned short length);  // Subclassed by SVG.

  void invalidateDisplayItemClients(PaintInvalidationReason) const override;

 private:
  void computePreferredLogicalWidths(float leadWidth);
  void computePreferredLogicalWidths(
      float leadWidth,
      HashSet<const SimpleFontData*>& fallbackFonts,
      FloatRect& glyphBounds);

  // Make length() private so that callers that have a LayoutText*
  // will use the more efficient textLength() instead, while
  // callers with a LayoutObject* can continue to use length().
  unsigned length() const final { return textLength(); }

  // See the class comment as to why we shouldn't call this function directly.
  void paint(const PaintInfo&, const LayoutPoint&) const final {
    ASSERT_NOT_REACHED();
  }
  void layout() final { ASSERT_NOT_REACHED(); }
  bool nodeAtPoint(HitTestResult&,
                   const HitTestLocation&,
                   const LayoutPoint&,
                   HitTestAction) final {
    ASSERT_NOT_REACHED();
    return false;
  }

  void deleteTextBoxes();
  float widthFromFont(const Font&,
                      int start,
                      int len,
                      float leadWidth,
                      float textWidthSoFar,
                      TextDirection,
                      HashSet<const SimpleFontData*>* fallbackFonts,
                      FloatRect* glyphBoundsAccumulation) const;

  void secureText(UChar mask);

  bool isText() const =
      delete;  // This will catch anyone doing an unnecessary check.

  LayoutRect localVisualRect() const override;

  void checkConsistency() const;

  // We put the bitfield first to minimize padding on 64-bit.

  // Whether or not we can be broken into multiple lines.
  bool m_hasBreakableChar : 1;
  // Whether or not we have a hard break (e.g., <pre> with '\n').
  bool m_hasBreak : 1;
  // Whether or not we have a variable width tab character (e.g., <pre> with
  // '\t').
  bool m_hasTab : 1;
  bool m_hasBreakableStart : 1;
  bool m_hasBreakableEnd : 1;
  bool m_hasEndWhiteSpace : 1;
  // This bit indicates that the text run has already dirtied specific line
  // boxes, and this hint will enable layoutInlineChildren to avoid just
  // dirtying everything when character data is modified (e.g., appended/
  // inserted or removed).
  bool m_linesDirty : 1;
  bool m_containsReversedText : 1;
  mutable bool m_knownToHaveNoOverflowAndNoFallbackFonts : 1;

  float m_minWidth;
  float m_maxWidth;
  float m_firstLineMinWidth;
  float m_lastLineLineMinWidth;

  String m_text;

  // The line boxes associated with this object.
  // Read the LINE BOXES OWNERSHIP section in the class header comment.
  InlineTextBox* m_firstTextBox;
  InlineTextBox* m_lastTextBox;
};

inline UChar LayoutText::uncheckedCharacterAt(unsigned i) const {
  SECURITY_DCHECK(i < textLength());
  return is8Bit() ? characters8()[i] : characters16()[i];
}

inline UChar LayoutText::characterAt(unsigned i) const {
  if (i >= textLength())
    return 0;

  return uncheckedCharacterAt(i);
}

inline UChar32 LayoutText::codepointAt(unsigned i) const {
  if (i >= textLength())
    return 0;
  if (is8Bit())
    return characters8()[i];
  UChar32 c;
  U16_GET(characters16(), 0, i, textLength(), c);
  return c;
}

inline float LayoutText::hyphenWidth(const Font& font,
                                     TextDirection direction) {
  const ComputedStyle& style = styleRef();
  return font.width(constructTextRun(font, style.hyphenString().getString(),
                                     style, direction));
}

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutText, isText());

#if !ENABLE(ASSERT)
inline void LayoutText::checkConsistency() const {}
#endif

inline LayoutText* Text::layoutObject() const {
  return toLayoutText(CharacterData::layoutObject());
}

void applyTextTransform(const ComputedStyle*, String&, UChar);

}  // namespace blink

#endif  // LayoutText_h

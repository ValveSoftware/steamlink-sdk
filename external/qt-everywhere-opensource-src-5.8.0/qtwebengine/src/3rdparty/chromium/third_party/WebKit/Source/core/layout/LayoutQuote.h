/*
 * Copyright (C) 2011 Nokia Inc. All rights reserved.
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
 *
 */

#ifndef LayoutQuote_h
#define LayoutQuote_h

#include "core/layout/LayoutInline.h"
#include "core/style/ComputedStyle.h"
#include "core/style/ComputedStyleConstants.h"
#include "core/style/QuotesData.h"

namespace blink {

class Document;
class LayoutTextFragment;

// LayoutQuote is the layout object associated with generated quotes
// ("content: open-quote | close-quote | no-open-quote | no-close-quote").
// http://www.w3.org/TR/CSS2/generate.html#quotes-insert
//
// This object is generated thus always anonymous.
//
// For performance reasons, LayoutQuotes form a doubly-linked list. See |m_next|
// and |m_previous| below.
class LayoutQuote final : public LayoutInline {
public:
    LayoutQuote(Document*, const QuoteType);
    ~LayoutQuote() override;
    void attachQuote();

    const char* name() const override { return "LayoutQuote"; }

private:
    void detachQuote();

    void willBeDestroyed() override;
    bool isOfType(LayoutObjectType type) const override { return type == LayoutObjectQuote || LayoutInline::isOfType(type); }
    void styleDidChange(StyleDifference, const ComputedStyle*) override;
    void willBeRemovedFromTree() override;

    String computeText() const;
    void updateText();
    const QuotesData* quotesData() const;
    void updateDepth();
    bool isAttached() { return m_attached; }

    LayoutTextFragment* findFragmentChild() const;

    // Type of this LayoutQuote: open-quote, close-quote, no-open-quote,
    // no-close-quote.
    QuoteType m_type;

    // Number of open quotes in the tree. Also called the nesting level
    // in CSS 2.1.
    // Used to determine if a LayoutQuote is invalid (closing quote without a
    // matching opening quote) and which quote character to use (see the 'quote'
    // property that is used to define quote character pairs).
    int m_depth;

    // The next and previous LayoutQuote in layout tree order.
    // LayoutQuotes are linked together by this doubly-linked list.
    // Those are used to compute |m_depth| in an efficient manner.
    LayoutQuote* m_next;
    LayoutQuote* m_previous;

    // This tracks whether this LayoutQuote was inserted into the layout tree
    // and its position in the linked list is correct (m_next and m_previous).
    // It's used for both performance (avoid unneeded tree walks to find the
    // previous and next quotes) and conformance (|m_depth| relies on an
    // up-to-date linked list positions).
    bool m_attached;

    // Cached text for this quote.
    String m_text;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutQuote, isQuote());

} // namespace blink

#endif // LayoutQuote_h

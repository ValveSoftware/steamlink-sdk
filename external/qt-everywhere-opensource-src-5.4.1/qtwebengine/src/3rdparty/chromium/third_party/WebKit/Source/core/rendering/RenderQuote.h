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

#ifndef RenderQuote_h
#define RenderQuote_h

#include "core/rendering/RenderInline.h"
#include "core/rendering/style/QuotesData.h"
#include "core/rendering/style/RenderStyle.h"
#include "core/rendering/style/RenderStyleConstants.h"

namespace WebCore {

class Document;

class RenderQuote FINAL : public RenderInline {
public:
    RenderQuote(Document*, const QuoteType);
    virtual ~RenderQuote();
    void attachQuote();

private:
    void detachQuote();

    virtual void willBeDestroyed() OVERRIDE;
    virtual const char* renderName() const OVERRIDE { return "RenderQuote"; };
    virtual bool isQuote() const OVERRIDE { return true; };
    virtual void styleDidChange(StyleDifference, const RenderStyle*) OVERRIDE;
    virtual void willBeRemovedFromTree() OVERRIDE;

    String computeText() const;
    void updateText();
    const QuotesData* quotesData() const;
    void updateDepth();
    bool isAttached() { return m_attached; }

    QuoteType m_type;
    int m_depth;
    RenderQuote* m_next;
    RenderQuote* m_previous;
    bool m_attached;
    String m_text;
};

DEFINE_RENDER_OBJECT_TYPE_CASTS(RenderQuote, isQuote());

} // namespace WebCore

#endif // RenderQuote_h

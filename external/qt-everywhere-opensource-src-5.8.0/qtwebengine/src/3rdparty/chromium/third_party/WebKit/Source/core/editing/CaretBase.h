/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CaretBase_h
#define CaretBase_h

#include "core/CoreExport.h"
#include "core/editing/VisiblePosition.h"
#include "platform/geometry/IntRect.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "wtf/Noncopyable.h"

namespace blink {

class CullRect;
class DisplayItemClient;
class LocalFrame;
class GraphicsContext;
class LayoutBlock;
class LayoutViewItem;

enum class CaretVisibility { Visible, Hidden };

class CORE_EXPORT CaretBase : public  GarbageCollectedFinalized<CaretBase> {
    WTF_MAKE_NONCOPYABLE(CaretBase);
public:
    explicit CaretBase(CaretVisibility = CaretVisibility::Hidden);
    virtual ~CaretBase();

    void invalidateCaretRect(Node*, bool caretRectChanged = false);
    void clearCaretRect();
    // Creating VisiblePosition causes synchronous layout so we should use the
    // PositionWithAffinity version if possible.
    // A position in HTMLTextFromControlElement is a typical example.
    bool updateCaretRect(const PositionWithAffinity& caretPosition);
    bool updateCaretRect(const VisiblePosition& caretPosition);
    IntRect absoluteBoundsForLocalRect(Node*, const LayoutRect&) const;
    bool shouldRepaintCaret(Node&) const;
    bool shouldRepaintCaret(const LayoutViewItem) const;
    void paintCaret(Node*, GraphicsContext&, const LayoutPoint&, DisplayItem::Type) const;

    const LayoutRect& localCaretRectWithoutUpdate() const { return m_caretLocalRect; }

    virtual void setCaretVisibility(CaretVisibility);
    bool caretIsVisible() const { return m_caretVisibility == CaretVisibility::Visible; }
    CaretVisibility getCaretVisibility() const { return m_caretVisibility; }

    static LayoutBlock* caretLayoutObject(Node*);
    static void invalidateLocalCaretRect(Node*, const LayoutRect&);

    DECLARE_VIRTUAL_TRACE();

private:
    static DisplayItemClient* displayItemClientForCaret(Node*);

    LayoutRect m_caretLocalRect; // caret rect in coords local to the layoutObject responsible for painting the caret
    CaretVisibility m_caretVisibility;
};

} // namespace blink

#endif // CaretBase_h

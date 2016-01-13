/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef AXScrollView_h
#define AXScrollView_h

#include "core/accessibility/AXObject.h"

namespace WebCore {

class AXScrollbar;
class Scrollbar;
class ScrollView;

class AXScrollView FINAL : public AXObject {
public:
    static PassRefPtr<AXScrollView> create(ScrollView*);
    virtual AccessibilityRole roleValue() const OVERRIDE { return ScrollAreaRole; }
    ScrollView* scrollView() const { return m_scrollView; }

    virtual ~AXScrollView();
    virtual void detach() OVERRIDE;

protected:
    virtual ScrollableArea* getScrollableAreaIfScrollable() const OVERRIDE;
    virtual void scrollTo(const IntPoint&) const OVERRIDE;

private:
    explicit AXScrollView(ScrollView*);

    virtual bool computeAccessibilityIsIgnored() const OVERRIDE;
    virtual bool isAXScrollView() const OVERRIDE { return true; }
    virtual bool isEnabled() const OVERRIDE { return true; }

    virtual bool isAttachment() const OVERRIDE;
    virtual Widget* widgetForAttachmentView() const OVERRIDE;

    virtual AXObject* scrollBar(AccessibilityOrientation) OVERRIDE;
    virtual void addChildren() OVERRIDE;
    virtual void clearChildren() OVERRIDE;
    virtual AXObject* accessibilityHitTest(const IntPoint&) const OVERRIDE;
    virtual void updateChildrenIfNecessary() OVERRIDE;
    virtual void setNeedsToUpdateChildren() OVERRIDE { m_childrenDirty = true; }
    void updateScrollbars();

    virtual FrameView* documentFrameView() const OVERRIDE;
    virtual LayoutRect elementRect() const OVERRIDE;
    virtual AXObject* parentObject() const OVERRIDE;
    virtual AXObject* parentObjectIfExists() const OVERRIDE;

    AXObject* webAreaObject() const;
    virtual AXObject* firstChild() const OVERRIDE { return webAreaObject(); }
    AXScrollbar* addChildScrollbar(Scrollbar*);
    void removeChildScrollbar(AXObject*);

    ScrollView* m_scrollView;
    RefPtr<AXObject> m_horizontalScrollbar;
    RefPtr<AXObject> m_verticalScrollbar;
    bool m_childrenDirty;
};

DEFINE_AX_OBJECT_TYPE_CASTS(AXScrollView, isAXScrollView());

} // namespace WebCore

#endif // AXScrollView_h

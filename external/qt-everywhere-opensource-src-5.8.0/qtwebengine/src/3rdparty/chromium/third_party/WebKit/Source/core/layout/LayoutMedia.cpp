/*
 * Copyright (C) 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
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

#include "core/layout/LayoutMedia.h"

#include "core/html/HTMLMediaElement.h"
#include "core/html/shadow/MediaControls.h"
#include "core/layout/LayoutView.h"

namespace blink {

LayoutMedia::LayoutMedia(HTMLMediaElement* video)
    : LayoutImage(video)
{
    setImageResource(LayoutImageResource::create());
}

LayoutMedia::~LayoutMedia()
{
}

HTMLMediaElement* LayoutMedia::mediaElement() const
{
    return toHTMLMediaElement(node());
}

void LayoutMedia::layout()
{
    LayoutSize oldSize = contentBoxRect().size();

    LayoutImage::layout();

    LayoutRect newRect = contentBoxRect();

    LayoutState state(*this, locationOffset());

    // Iterate the children in reverse order so that the media controls are laid
    // out before the text track container. This is to ensure that the text
    // track rendering has an up-to-date position of the media controls for
    // overlap checking, see LayoutVTTCue.
#if ENABLE(ASSERT)
    bool seenTextTrackContainer = false;
#endif
    for (LayoutObject* child = m_children.lastChild(); child; child = child->previousSibling()) {
#if ENABLE(ASSERT)
        if (child->node()->isMediaControls())
            ASSERT(!seenTextTrackContainer);
        else if (child->node()->isTextTrackContainer())
            seenTextTrackContainer = true;
        else
            ASSERT_NOT_REACHED();
#endif

        if (newRect.size() == oldSize && !child->needsLayout())
            continue;

        LayoutBox* layoutBox = toLayoutBox(child);
        layoutBox->setLocation(newRect.location());
        // TODO(foolip): Remove the mutableStyleRef() and depend on CSS
        // width/height: inherit to match the media element size.
        layoutBox->mutableStyleRef().setHeight(Length(newRect.height(), Fixed));
        layoutBox->mutableStyleRef().setWidth(Length(newRect.width(), Fixed));
        layoutBox->forceLayout();
    }

    clearNeedsLayout();

    // Notify our MediaControls that a layout has happened.
    if (mediaElement() && mediaElement()->mediaControls() && newRect.width() != oldSize.width())
        mediaElement()->mediaControls()->notifyPanelWidthChanged(newRect.width());
}

bool LayoutMedia::isChildAllowed(LayoutObject* child, const ComputedStyle&) const
{
    // Two types of child layout objects are allowed: media controls
    // and the text track container. Filter children by node type.
    ASSERT(child->node());

    // The user agent stylesheet (mediaControls.css) has
    // ::-webkit-media-controls { display: flex; }. If author style
    // sets display: inline we would get an inline layoutObject as a child
    // of replaced content, which is not supposed to be possible. This
    // check can be removed if ::-webkit-media-controls is made
    // internal.
    if (child->node()->isMediaControls())
        return child->isFlexibleBox();

    if (child->node()->isTextTrackContainer())
        return true;

    return false;
}

void LayoutMedia::paintReplaced(const PaintInfo&, const LayoutPoint&) const
{
}

void LayoutMedia::willBeDestroyed()
{
    if (view())
        view()->unregisterMediaForPositionChangeNotification(*this);
    LayoutImage::willBeDestroyed();
}

void LayoutMedia::insertedIntoTree()
{
    LayoutImage::insertedIntoTree();

    // Note that if we don't want them and aren't registered, then this
    // will do nothing.
    if (HTMLMediaElement* element = mediaElement())
        element->updatePositionNotificationRegistration();
}

void LayoutMedia::notifyPositionMayHaveChanged(const IntRect& visibleRect)
{
    // Tell our element about it.
    if (HTMLMediaElement* element = mediaElement())
        element->notifyPositionMayHaveChanged(visibleRect);
}

void LayoutMedia::setRequestPositionUpdates(bool want)
{
    if (want)
        view()->registerMediaForPositionChangeNotification(*this);
    else
        view()->unregisterMediaForPositionChangeNotification(*this);
}

} // namespace blink

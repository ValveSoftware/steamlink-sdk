/*
 * Copyright (C) 1999 Lars Knoll <knoll@kde.org>
 * Copyright (C) 1999 Antti Koivisto <koivisto@kde.org>
 * Copyright (C) 2006 Allan Sandfeld Jensen <kde@carewolf.com>
 * Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
 * Copyright (C) 2004, 2005, 2006, 2007, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>
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

#ifndef LayoutImageResource_h
#define LayoutImageResource_h

#include "core/fetch/ImageResource.h"
#include "core/style/StyleImage.h"

namespace blink {

class LayoutObject;

class LayoutImageResource : public GarbageCollectedFinalized<LayoutImageResource> {
    WTF_MAKE_NONCOPYABLE(LayoutImageResource);
public:
    virtual ~LayoutImageResource();

    static LayoutImageResource* create()
    {
        return new LayoutImageResource;
    }

    virtual void initialize(LayoutObject*);
    virtual void shutdown();

    void setImageResource(ImageResource*);
    ImageResource* cachedImage() const { return m_cachedImage.get(); }
    virtual bool hasImage() const { return m_cachedImage; }

    void resetAnimation();
    bool maybeAnimated() const;

    virtual PassRefPtr<Image> image(const IntSize&, float) const;
    virtual bool errorOccurred() const { return m_cachedImage && m_cachedImage->errorOccurred(); }

    virtual bool imageHasRelativeSize() const { return m_cachedImage ? m_cachedImage->imageHasRelativeSize() : false; }

    virtual LayoutSize imageSize(float multiplier) const;

    virtual WrappedImagePtr imagePtr() const { return m_cachedImage.get(); }

    DEFINE_INLINE_VIRTUAL_TRACE() { visitor->trace(m_cachedImage); }

protected:
    LayoutImageResource();
    LayoutObject* m_layoutObject;
    Member<ImageResource> m_cachedImage;
};

} // namespace blink

#endif // LayoutImage_h

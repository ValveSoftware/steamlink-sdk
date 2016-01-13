/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
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
 */

#ifndef SVGFEImageElement_h
#define SVGFEImageElement_h

#include "core/SVGNames.h"
#include "core/fetch/ImageResource.h"
#include "core/fetch/ResourcePtr.h"
#include "core/svg/SVGAnimatedBoolean.h"
#include "core/svg/SVGAnimatedPreserveAspectRatio.h"
#include "core/svg/SVGFilterPrimitiveStandardAttributes.h"
#include "core/svg/SVGURIReference.h"
#include "core/svg/graphics/filters/SVGFEImage.h"
#include "platform/graphics/ImageBuffer.h"

namespace WebCore {

class SVGFEImageElement FINAL : public SVGFilterPrimitiveStandardAttributes,
                                public SVGURIReference,
                                public ImageResourceClient {
public:
    DECLARE_NODE_FACTORY(SVGFEImageElement);

    bool currentFrameHasSingleSecurityOrigin() const;

    virtual ~SVGFEImageElement();
    SVGAnimatedPreserveAspectRatio* preserveAspectRatio() { return m_preserveAspectRatio.get(); }

private:
    explicit SVGFEImageElement(Document&);

    bool isSupportedAttribute(const QualifiedName&);
    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;
    virtual void svgAttributeChanged(const QualifiedName&) OVERRIDE;
    virtual void notifyFinished(Resource*) OVERRIDE;

    virtual PassRefPtr<FilterEffect> build(SVGFilterBuilder*, Filter*) OVERRIDE;

    void clearResourceReferences();
    void fetchImageResource();

    virtual void buildPendingResource() OVERRIDE;
    virtual InsertionNotificationRequest insertedInto(ContainerNode*) OVERRIDE;
    virtual void removedFrom(ContainerNode*) OVERRIDE;

    RefPtr<SVGAnimatedPreserveAspectRatio> m_preserveAspectRatio;

    ResourcePtr<ImageResource> m_cachedImage;
};

} // namespace WebCore

#endif

/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "core/css/resolver/ElementStyleResources.h"

#include "core/CSSPropertyNames.h"
#include "core/css/CSSCursorImageValue.h"
#include "core/css/CSSGradientValue.h"
#include "core/css/CSSImageValue.h"
#include "core/css/CSSSVGDocumentValue.h"
#include "core/dom/Document.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/layout/svg/ReferenceFilterBuilder.h"
#include "core/style/ComputedStyle.h"
#include "core/style/ContentData.h"
#include "core/style/CursorData.h"
#include "core/style/FillLayer.h"
#include "core/style/StyleFetchedImage.h"
#include "core/style/StyleFetchedImageSet.h"
#include "core/style/StyleGeneratedImage.h"
#include "core/style/StyleImage.h"
#include "core/style/StyleInvalidImage.h"
#include "core/style/StylePendingImage.h"
#include "platform/graphics/filters/FilterOperation.h"

namespace blink {

ElementStyleResources::ElementStyleResources(Document& document, float deviceScaleFactor)
    : m_document(&document)
    , m_deviceScaleFactor(deviceScaleFactor)
{
}

StyleImage* ElementStyleResources::styleImage(CSSPropertyID property, const CSSValue& value)
{
    if (value.isImageValue())
        return cachedOrPendingFromValue(property, toCSSImageValue(value));

    if (value.isImageGeneratorValue())
        return generatedOrPendingFromValue(property, toCSSImageGeneratorValue(value));

    if (value.isImageSetValue())
        return setOrPendingFromValue(property, toCSSImageSetValue(value));

    if (value.isCursorImageValue())
        return cursorOrPendingFromValue(property, toCSSCursorImageValue(value));

    return nullptr;
}

StyleImage* ElementStyleResources::generatedOrPendingFromValue(CSSPropertyID property, const CSSImageGeneratorValue& value)
{
    if (value.isPending()) {
        m_pendingImageProperties.add(property);
        return StylePendingImage::create(value);
    }
    return StyleGeneratedImage::create(value);
}

StyleImage* ElementStyleResources::setOrPendingFromValue(CSSPropertyID property, const CSSImageSetValue& value)
{
    if (value.isCachePending(m_deviceScaleFactor)) {
        m_pendingImageProperties.add(property);
        return StylePendingImage::create(value);
    }
    return value.cachedImage(m_deviceScaleFactor);
}

StyleImage* ElementStyleResources::cachedOrPendingFromValue(CSSPropertyID property, const CSSImageValue& value)
{
    if (value.isCachePending()) {
        m_pendingImageProperties.add(property);
        return StylePendingImage::create(value);
    }
    value.restoreCachedResourceIfNeeded(*m_document);
    return value.cachedImage();
}

StyleImage* ElementStyleResources::cursorOrPendingFromValue(CSSPropertyID property, const CSSCursorImageValue& value)
{
    if (value.isCachePending(m_deviceScaleFactor)) {
        m_pendingImageProperties.add(property);
        return StylePendingImage::create(value);
    }
    return value.cachedImage(m_deviceScaleFactor);
}

void ElementStyleResources::addPendingSVGDocument(FilterOperation* filterOperation, const CSSSVGDocumentValue* cssSVGDocumentValue)
{
    m_pendingSVGDocuments.set(filterOperation, cssSVGDocumentValue);
}

void ElementStyleResources::loadPendingSVGDocuments(ComputedStyle* computedStyle)
{
    if (!computedStyle->hasFilter() || m_pendingSVGDocuments.isEmpty())
        return;

    FilterOperations::FilterOperationVector& filterOperations = computedStyle->mutableFilter().operations();
    for (unsigned i = 0; i < filterOperations.size(); ++i) {
        FilterOperation* filterOperation = filterOperations.at(i);
        if (filterOperation->type() == FilterOperation::REFERENCE) {
            ReferenceFilterOperation* referenceFilter = toReferenceFilterOperation(filterOperation);

            const CSSSVGDocumentValue* value = m_pendingSVGDocuments.get(referenceFilter);
            if (!value)
                continue;
            DocumentResource* resource = value->load(m_document);
            if (!resource)
                continue;

            // Stash the DocumentResource on the reference filter.
            ReferenceFilterBuilder::setDocumentResourceReference(referenceFilter, new DocumentResourceReference(resource));
        }
    }
}

StyleImage* ElementStyleResources::loadPendingImage(ComputedStyle* style, StylePendingImage* pendingImage, CrossOriginAttributeValue crossOrigin)
{
    if (CSSImageValue* imageValue = pendingImage->cssImageValue())
        return imageValue->cacheImage(m_document, crossOrigin);

    if (CSSPaintValue* paintValue = pendingImage->cssPaintValue()) {
        StyleGeneratedImage* image = StyleGeneratedImage::create(*paintValue);
        style->addPaintImage(image);
        return image;
    }

    if (CSSImageGeneratorValue* imageGeneratorValue = pendingImage->cssImageGeneratorValue()) {
        imageGeneratorValue->loadSubimages(m_document);
        return StyleGeneratedImage::create(*imageGeneratorValue);
    }

    if (CSSCursorImageValue* cursorImageValue = pendingImage->cssCursorImageValue())
        return cursorImageValue->cacheImage(m_document, m_deviceScaleFactor);

    if (CSSImageSetValue* imageSetValue = pendingImage->cssImageSetValue())
        return imageSetValue->cacheImage(m_document, m_deviceScaleFactor, crossOrigin);

    ASSERT_NOT_REACHED();
    return nullptr;
}

void ElementStyleResources::loadPendingImages(ComputedStyle* style)
{
    // We must loop over the properties and then look at the style to see if
    // a pending image exists, and only load that image. For example:
    //
    // <style>
    //    div { background-image: url(a.png); }
    //    div { background-image: url(b.png); }
    //    div { background-image: none; }
    // </style>
    // <div></div>
    //
    // We call styleImage() for both a.png and b.png adding the
    // CSSPropertyBackgroundImage property to the m_pendingImageProperties set,
    // then we null out the background image because of the "none".
    //
    // If we eagerly loaded the images we'd fetch a.png, even though it's not
    // used. If we didn't null check below we'd crash since the none actually
    // removed all background images.

    for (CSSPropertyID property : m_pendingImageProperties) {
        switch (property) {
        case CSSPropertyBackgroundImage: {
            for (FillLayer* backgroundLayer = &style->accessBackgroundLayers(); backgroundLayer; backgroundLayer = backgroundLayer->next()) {
                if (backgroundLayer->image() && backgroundLayer->image()->isPendingImage())
                    backgroundLayer->setImage(loadPendingImage(style, toStylePendingImage(backgroundLayer->image())));
            }
            break;
        }
        case CSSPropertyContent: {
            for (ContentData* contentData = const_cast<ContentData*>(style->contentData()); contentData; contentData = contentData->next()) {
                if (contentData->isImage()) {
                    StyleImage* image = toImageContentData(contentData)->image();
                    if (image->isPendingImage())
                        toImageContentData(contentData)->setImage(loadPendingImage(style, toStylePendingImage(image)));
                }
            }
            break;
        }
        case CSSPropertyCursor: {
            if (CursorList* cursorList = style->cursors()) {
                for (size_t i = 0; i < cursorList->size(); ++i) {
                    CursorData& currentCursor = cursorList->at(i);
                    if (StyleImage* image = currentCursor.image()) {
                        if (image->isPendingImage())
                            currentCursor.setImage(loadPendingImage(style, toStylePendingImage(image)));
                    }
                }
            }
            break;
        }
        case CSSPropertyListStyleImage: {
            if (style->listStyleImage() && style->listStyleImage()->isPendingImage())
                style->setListStyleImage(loadPendingImage(style, toStylePendingImage(style->listStyleImage())));
            break;
        }
        case CSSPropertyBorderImageSource: {
            if (style->borderImageSource() && style->borderImageSource()->isPendingImage())
                style->setBorderImageSource(loadPendingImage(style, toStylePendingImage(style->borderImageSource())));
            break;
        }
        case CSSPropertyWebkitBoxReflect: {
            if (StyleReflection* reflection = style->boxReflect()) {
                const NinePieceImage& maskImage = reflection->mask();
                if (maskImage.image() && maskImage.image()->isPendingImage()) {
                    StyleImage* loadedImage = loadPendingImage(style, toStylePendingImage(maskImage.image()));
                    reflection->setMask(NinePieceImage(loadedImage, maskImage.imageSlices(), maskImage.fill(), maskImage.borderSlices(), maskImage.outset(), maskImage.horizontalRule(), maskImage.verticalRule()));
                }
            }
            break;
        }
        case CSSPropertyWebkitMaskBoxImageSource: {
            if (style->maskBoxImageSource() && style->maskBoxImageSource()->isPendingImage())
                style->setMaskBoxImageSource(loadPendingImage(style, toStylePendingImage(style->maskBoxImageSource())));
            break;
        }
        case CSSPropertyWebkitMaskImage: {
            for (FillLayer* maskLayer = &style->accessMaskLayers(); maskLayer; maskLayer = maskLayer->next()) {
                if (maskLayer->image() && maskLayer->image()->isPendingImage())
                    maskLayer->setImage(loadPendingImage(style, toStylePendingImage(maskLayer->image())));
            }
            break;
        }
        case CSSPropertyShapeOutside:
            if (style->shapeOutside() && style->shapeOutside()->image() && style->shapeOutside()->image()->isPendingImage())
                style->shapeOutside()->setImage(loadPendingImage(style, toStylePendingImage(style->shapeOutside()->image()), CrossOriginAttributeAnonymous));
            break;
        default:
            ASSERT_NOT_REACHED();
        }
    }
}

void ElementStyleResources::loadPendingResources(ComputedStyle* computedStyle)
{
    loadPendingImages(computedStyle);
    loadPendingSVGDocuments(computedStyle);
}

} // namespace blink

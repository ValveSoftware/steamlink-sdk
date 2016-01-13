/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2008, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef HTMLImageElement_h
#define HTMLImageElement_h

#include "core/html/HTMLElement.h"
#include "core/html/HTMLImageLoader.h"
#include "core/html/canvas/CanvasImageSource.h"
#include "platform/graphics/GraphicsTypes.h"
#include "wtf/WeakPtr.h"

namespace WebCore {

class HTMLFormElement;
class ImageCandidate;

class HTMLImageElement FINAL : public HTMLElement, public CanvasImageSource {
public:
    static PassRefPtrWillBeRawPtr<HTMLImageElement> create(Document&);
    static PassRefPtrWillBeRawPtr<HTMLImageElement> create(Document&, HTMLFormElement*, bool createdByParser);
    static PassRefPtrWillBeRawPtr<HTMLImageElement> createForJSConstructor(Document&, int width, int height);

    virtual ~HTMLImageElement();
    virtual void trace(Visitor*) OVERRIDE;

    int width(bool ignorePendingStylesheets = false);
    int height(bool ignorePendingStylesheets = false);

    int naturalWidth() const;
    int naturalHeight() const;
    const String& currentSrc() const;

    bool isServerMap() const;

    const AtomicString& altText() const;

    CompositeOperator compositeOperator() const { return m_compositeOperator; }

    ImageResource* cachedImage() const { return imageLoader().image(); }
    void setImageResource(ImageResource* i) { imageLoader().setImage(i); };

    void setLoadManually(bool loadManually) { imageLoader().setLoadManually(loadManually); }

    const AtomicString& alt() const;

    void setHeight(int);

    KURL src() const;
    void setSrc(const String&);

    void setWidth(int);

    int x() const;
    int y() const;

    bool complete() const;

    bool hasPendingActivity() const { return imageLoader().hasPendingActivity(); }

    virtual bool canContainRangeEndPoint() const OVERRIDE { return false; }

    void addClient(ImageLoaderClient* client) { imageLoader().addClient(client); }
    void removeClient(ImageLoaderClient* client) { imageLoader().removeClient(client); }

    virtual const AtomicString imageSourceURL() const OVERRIDE;

    virtual HTMLFormElement* formOwner() const OVERRIDE;
    void formRemovedFromTree(const Node& formRoot);

    // CanvasImageSourceImplementations
    virtual PassRefPtr<Image> getSourceImageForCanvas(SourceImageMode, SourceImageStatus*) const;
    virtual bool wouldTaintOrigin(SecurityOrigin*) const OVERRIDE;
    virtual FloatSize sourceSize() const OVERRIDE;
    virtual FloatSize defaultDestinationSize() const OVERRIDE;
    virtual const KURL& sourceURL() const OVERRIDE;

    enum UpdateFromElementBehavior {
        UpdateNormal,
        UpdateIgnorePreviousError
    };
    // public so that HTMLPictureElement can call this as well.
    void selectSourceURL(UpdateFromElementBehavior);
protected:
    explicit HTMLImageElement(Document&, HTMLFormElement* = 0, bool createdByParser = false);

    virtual void didMoveToNewDocument(Document& oldDocument) OVERRIDE;

private:
    virtual bool areAuthorShadowsAllowed() const OVERRIDE { return false; }

    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;
    virtual bool isPresentationAttribute(const QualifiedName&) const OVERRIDE;
    virtual void collectStyleForPresentationAttribute(const QualifiedName&, const AtomicString&, MutableStylePropertySet*) OVERRIDE;

    virtual void attach(const AttachContext& = AttachContext()) OVERRIDE;
    virtual RenderObject* createRenderer(RenderStyle*) OVERRIDE;

    virtual bool canStartSelection() const OVERRIDE;

    virtual bool isURLAttribute(const Attribute&) const OVERRIDE;
    virtual bool hasLegalLinkAttribute(const QualifiedName&) const OVERRIDE;
    virtual const QualifiedName& subResourceAttributeName() const OVERRIDE;

    virtual bool draggable() const OVERRIDE;

    virtual InsertionNotificationRequest insertedInto(ContainerNode*) OVERRIDE;
    virtual void removedFrom(ContainerNode*) OVERRIDE;
    virtual bool shouldRegisterAsNamedItem() const OVERRIDE { return true; }
    virtual bool shouldRegisterAsExtraNamedItem() const OVERRIDE { return true; }
    virtual bool isInteractiveContent() const OVERRIDE;
    virtual Image* imageContents() OVERRIDE;

    void resetFormOwner();
    ImageCandidate findBestFitImageFromPictureParent();
    void setBestFitURLAndDPRFromImageCandidate(const ImageCandidate&);
    HTMLImageLoader& imageLoader() const { return *m_imageLoader; }

    OwnPtrWillBeMember<HTMLImageLoader> m_imageLoader;
#if ENABLE(OILPAN)
    Member<HTMLFormElement> m_form;
#else
    WeakPtr<HTMLFormElement> m_form;
#endif
    CompositeOperator m_compositeOperator;
    AtomicString m_bestFitImageURL;
    float m_imageDevicePixelRatio;
    bool m_formWasSetByParser;
    bool m_elementCreatedByParser;
};

} //namespace

#endif

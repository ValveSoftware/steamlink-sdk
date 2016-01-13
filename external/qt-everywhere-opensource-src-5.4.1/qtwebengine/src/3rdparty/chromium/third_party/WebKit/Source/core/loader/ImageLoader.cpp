/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2009, 2010 Apple Inc. All rights reserved.
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

#include "config.h"
#include "core/loader/ImageLoader.h"

#include "bindings/v8/ScriptController.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/IncrementLoadEventDelayCount.h"
#include "core/dom/Microtask.h"
#include "core/events/Event.h"
#include "core/events/EventSender.h"
#include "core/fetch/CrossOriginAccessControl.h"
#include "core/fetch/FetchRequest.h"
#include "core/fetch/MemoryCache.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/rendering/RenderImage.h"
#include "core/rendering/RenderVideo.h"
#include "core/rendering/svg/RenderSVGImage.h"
#include "platform/weborigin/SecurityOrigin.h"

namespace WebCore {

static ImageEventSender& loadEventSender()
{
    DEFINE_STATIC_LOCAL(ImageEventSender, sender, (EventTypeNames::load));
    return sender;
}

static ImageEventSender& errorEventSender()
{
    DEFINE_STATIC_LOCAL(ImageEventSender, sender, (EventTypeNames::error));
    return sender;
}

static inline bool pageIsBeingDismissed(Document* document)
{
    return document->pageDismissalEventBeingDispatched() != Document::NoDismissal;
}

class ImageLoader::Task : public blink::WebThread::Task {
public:
    Task(ImageLoader* loader)
        : m_loader(loader)
        , m_shouldBypassMainWorldContentSecurityPolicy(false)
        , m_weakFactory(this)
    {
        LocalFrame* frame = loader->m_element->document().frame();
        m_shouldBypassMainWorldContentSecurityPolicy = frame->script().shouldBypassMainWorldContentSecurityPolicy();
    }

    virtual void run() OVERRIDE
    {
        if (m_loader) {
            m_loader->doUpdateFromElement(m_shouldBypassMainWorldContentSecurityPolicy);
        }
    }

    void clearLoader()
    {
        m_loader = 0;
    }

    WeakPtr<Task> createWeakPtr()
    {
        return m_weakFactory.createWeakPtr();
    }

private:
    ImageLoader* m_loader;
    bool m_shouldBypassMainWorldContentSecurityPolicy;
    WeakPtrFactory<Task> m_weakFactory;
};

ImageLoader::ImageLoader(Element* element)
    : m_element(element)
    , m_image(0)
    , m_derefElementTimer(this, &ImageLoader::timerFired)
    , m_hasPendingLoadEvent(false)
    , m_hasPendingErrorEvent(false)
    , m_imageComplete(true)
    , m_loadManually(false)
    , m_elementIsProtected(false)
    , m_highPriorityClientCount(0)
{
}

ImageLoader::~ImageLoader()
{
    if (m_pendingTask)
        m_pendingTask->clearLoader();

    if (m_image)
        m_image->removeClient(this);

    ASSERT(m_hasPendingLoadEvent || !loadEventSender().hasPendingEvents(this));
    if (m_hasPendingLoadEvent)
        loadEventSender().cancelEvent(this);

    ASSERT(m_hasPendingErrorEvent || !errorEventSender().hasPendingEvents(this));
    if (m_hasPendingErrorEvent)
        errorEventSender().cancelEvent(this);
}

void ImageLoader::trace(Visitor* visitor)
{
    visitor->trace(m_element);
}

void ImageLoader::setImage(ImageResource* newImage)
{
    setImageWithoutConsideringPendingLoadEvent(newImage);

    // Only consider updating the protection ref-count of the Element immediately before returning
    // from this function as doing so might result in the destruction of this ImageLoader.
    updatedHasPendingEvent();
}

void ImageLoader::setImageWithoutConsideringPendingLoadEvent(ImageResource* newImage)
{
    ASSERT(m_failedLoadURL.isEmpty());
    ImageResource* oldImage = m_image.get();
    if (newImage != oldImage) {
        sourceImageChanged();
        m_image = newImage;
        if (m_hasPendingLoadEvent) {
            loadEventSender().cancelEvent(this);
            m_hasPendingLoadEvent = false;
        }
        if (m_hasPendingErrorEvent) {
            errorEventSender().cancelEvent(this);
            m_hasPendingErrorEvent = false;
        }
        m_imageComplete = true;
        if (newImage)
            newImage->addClient(this);
        if (oldImage)
            oldImage->removeClient(this);
    }

    if (RenderImageResource* imageResource = renderImageResource())
        imageResource->resetAnimation();
}

void ImageLoader::doUpdateFromElement(bool bypassMainWorldCSP)
{
    // We don't need to call clearLoader here: Either we were called from the
    // task, or our caller updateFromElement cleared the task's loader (and set
    // m_pendingTask to null).
    m_pendingTask.clear();
    // Make sure to only decrement the count when we exit this function
    OwnPtr<IncrementLoadEventDelayCount> delayLoad;
    delayLoad.swap(m_delayLoad);

    Document& document = m_element->document();
    if (!document.isActive())
        return;

    AtomicString attr = m_element->imageSourceURL();

    KURL url = imageURL();
    ResourcePtr<ImageResource> newImage = 0;
    if (!url.isNull()) {
        FetchRequest request(ResourceRequest(url), element()->localName());
        if (bypassMainWorldCSP)
            request.setContentSecurityCheck(DoNotCheckContentSecurityPolicy);

        AtomicString crossOriginMode = m_element->fastGetAttribute(HTMLNames::crossoriginAttr);
        if (!crossOriginMode.isNull())
            request.setCrossOriginAccessControl(document.securityOrigin(), crossOriginMode);

        if (m_loadManually) {
            bool autoLoadOtherImages = document.fetcher()->autoLoadImages();
            document.fetcher()->setAutoLoadImages(false);
            newImage = new ImageResource(request.resourceRequest());
            newImage->setLoading(true);
            document.fetcher()->m_documentResources.set(newImage->url(), newImage.get());
            document.fetcher()->setAutoLoadImages(autoLoadOtherImages);
        } else {
            newImage = document.fetcher()->fetchImage(request);
        }

        // If we do not have an image here, it means that a cross-site
        // violation occurred, or that the image was blocked via Content
        // Security Policy, or the page is being dismissed. Trigger an
        // error event if the page is not being dismissed.
        if (!newImage && !pageIsBeingDismissed(&document)) {
            m_failedLoadURL = attr;
            m_hasPendingErrorEvent = true;
            errorEventSender().dispatchEventSoon(this);
        } else
            clearFailedLoadURL();
    } else if (!attr.isNull()) {
        // Fire an error event if the url is empty.
        m_hasPendingErrorEvent = true;
        errorEventSender().dispatchEventSoon(this);
    }

    ImageResource* oldImage = m_image.get();
    if (newImage != oldImage) {
        sourceImageChanged();

        if (m_hasPendingLoadEvent) {
            loadEventSender().cancelEvent(this);
            m_hasPendingLoadEvent = false;
        }

        // Cancel error events that belong to the previous load, which is now cancelled by changing the src attribute.
        // If newImage is null and m_hasPendingErrorEvent is true, we know the error event has been just posted by
        // this load and we should not cancel the event.
        // FIXME: If both previous load and this one got blocked with an error, we can receive one error event instead of two.
        if (m_hasPendingErrorEvent && newImage) {
            errorEventSender().cancelEvent(this);
            m_hasPendingErrorEvent = false;
        }

        m_image = newImage;
        m_hasPendingLoadEvent = newImage;
        m_imageComplete = !newImage;

        if (newImage) {
            updateRenderer();

            // If newImage is cached, addClient() will result in the load event
            // being queued to fire. Ensure this happens after beforeload is
            // dispatched.
            newImage->addClient(this);
        } else {
            updateRenderer();
        }

        if (oldImage)
            oldImage->removeClient(this);
    }

    if (RenderImageResource* imageResource = renderImageResource())
        imageResource->resetAnimation();

    // Only consider updating the protection ref-count of the Element immediately before returning
    // from this function as doing so might result in the destruction of this ImageLoader.
    updatedHasPendingEvent();
}

void ImageLoader::updateFromElement(LoadType loadType)
{
    AtomicString attr = m_element->imageSourceURL();

    if (!m_failedLoadURL.isEmpty() && attr == m_failedLoadURL)
        return;

    // If we have a pending task, we have to clear it -- either we're
    // now loading immediately, or we need to reset the task's state.
    if (m_pendingTask) {
        m_pendingTask->clearLoader();
        m_pendingTask.clear();
    }

    KURL url = imageURL();
    if (!attr.isNull() && !url.isNull()) {
        bool loadImmediately = shouldLoadImmediately(url) || (loadType == ForceLoadImmediately);
        if (loadImmediately) {
            doUpdateFromElement(false);
        } else {
            OwnPtr<Task> task = adoptPtr(new Task(this));
            m_pendingTask = task->createWeakPtr();
            Microtask::enqueueMicrotask(task.release());
            m_delayLoad = adoptPtr(new IncrementLoadEventDelayCount(m_element->document()));
            return;
        }
    } else {
        doUpdateFromElement(false);
    }
}

void ImageLoader::updateFromElementIgnoringPreviousError()
{
    clearFailedLoadURL();
    updateFromElement();
}

KURL ImageLoader::imageURL() const
{
    KURL url;

    // Don't load images for inactive documents. We don't want to slow down the
    // raw HTML parsing case by loading images we don't intend to display.
    Document& document = m_element->document();
    if (!document.isActive())
        return url;

    AtomicString attr = m_element->imageSourceURL();

    // Do not load any image if the 'src' attribute is missing or if it is
    // an empty string.
    if (!attr.isNull() && !stripLeadingAndTrailingHTMLSpaces(attr).isEmpty()) {
        url = document.completeURL(sourceURI(attr));
    }
    return url;
}

bool ImageLoader::shouldLoadImmediately(const KURL& url) const
{
    if (m_loadManually)
        return true;
    if (isHTMLObjectElement(m_element) || isHTMLEmbedElement(m_element))
        return true;

    if (url.protocolIsData())
        return true;
    if (memoryCache()->resourceForURL(url))
        return true;
    return false;
}

void ImageLoader::notifyFinished(Resource* resource)
{
    ASSERT(m_failedLoadURL.isEmpty());
    ASSERT(resource == m_image.get());

    m_imageComplete = true;
    updateRenderer();

    if (!m_hasPendingLoadEvent)
        return;

    if (resource->errorOccurred()) {
        loadEventSender().cancelEvent(this);
        m_hasPendingLoadEvent = false;

        m_hasPendingErrorEvent = true;
        errorEventSender().dispatchEventSoon(this);

        // Only consider updating the protection ref-count of the Element immediately before returning
        // from this function as doing so might result in the destruction of this ImageLoader.
        updatedHasPendingEvent();
        return;
    }
    if (resource->wasCanceled()) {
        m_hasPendingLoadEvent = false;
        // Only consider updating the protection ref-count of the Element immediately before returning
        // from this function as doing so might result in the destruction of this ImageLoader.
        updatedHasPendingEvent();
        return;
    }
    loadEventSender().dispatchEventSoon(this);
}

RenderImageResource* ImageLoader::renderImageResource()
{
    RenderObject* renderer = m_element->renderer();

    if (!renderer)
        return 0;

    // We don't return style generated image because it doesn't belong to the ImageLoader.
    // See <https://bugs.webkit.org/show_bug.cgi?id=42840>
    if (renderer->isImage() && !static_cast<RenderImage*>(renderer)->isGeneratedContent())
        return toRenderImage(renderer)->imageResource();

    if (renderer->isSVGImage())
        return toRenderSVGImage(renderer)->imageResource();

    if (renderer->isVideo())
        return toRenderVideo(renderer)->imageResource();

    return 0;
}

void ImageLoader::updateRenderer()
{
    RenderImageResource* imageResource = renderImageResource();

    if (!imageResource)
        return;

    // Only update the renderer if it doesn't have an image or if what we have
    // is a complete image.  This prevents flickering in the case where a dynamic
    // change is happening between two images.
    ImageResource* cachedImage = imageResource->cachedImage();
    if (m_image != cachedImage && (m_imageComplete || !cachedImage))
        imageResource->setImageResource(m_image.get());
}

void ImageLoader::updatedHasPendingEvent()
{
    // If an Element that does image loading is removed from the DOM the load/error event for the image is still observable.
    // As long as the ImageLoader is actively loading, the Element itself needs to be ref'ed to keep it from being
    // destroyed by DOM manipulation or garbage collection.
    // If such an Element wishes for the load to stop when removed from the DOM it needs to stop the ImageLoader explicitly.
    bool wasProtected = m_elementIsProtected;
    m_elementIsProtected = m_hasPendingLoadEvent || m_hasPendingErrorEvent;
    if (wasProtected == m_elementIsProtected)
        return;

    if (m_elementIsProtected) {
        if (m_derefElementTimer.isActive())
            m_derefElementTimer.stop();
        else
            m_keepAlive = m_element;
    } else {
        ASSERT(!m_derefElementTimer.isActive());
        m_derefElementTimer.startOneShot(0, FROM_HERE);
    }
}

void ImageLoader::timerFired(Timer<ImageLoader>*)
{
    m_keepAlive.clear();
}

void ImageLoader::dispatchPendingEvent(ImageEventSender* eventSender)
{
    ASSERT(eventSender == &loadEventSender() || eventSender == &errorEventSender());
    const AtomicString& eventType = eventSender->eventType();
    if (eventType == EventTypeNames::load)
        dispatchPendingLoadEvent();
    if (eventType == EventTypeNames::error)
        dispatchPendingErrorEvent();
}

void ImageLoader::dispatchPendingLoadEvent()
{
    if (!m_hasPendingLoadEvent)
        return;
    if (!m_image)
        return;
    m_hasPendingLoadEvent = false;
    if (element()->document().frame())
        dispatchLoadEvent();

    // Only consider updating the protection ref-count of the Element immediately before returning
    // from this function as doing so might result in the destruction of this ImageLoader.
    updatedHasPendingEvent();
}

void ImageLoader::dispatchPendingErrorEvent()
{
    if (!m_hasPendingErrorEvent)
        return;
    m_hasPendingErrorEvent = false;

    if (element()->document().frame())
        element()->dispatchEvent(Event::create(EventTypeNames::error));

    // Only consider updating the protection ref-count of the Element immediately before returning
    // from this function as doing so might result in the destruction of this ImageLoader.
    updatedHasPendingEvent();
}

void ImageLoader::addClient(ImageLoaderClient* client)
{
    if (client->requestsHighLiveResourceCachePriority()) {
        if (m_image && !m_highPriorityClientCount++)
            memoryCache()->updateDecodedResource(m_image.get(), UpdateForPropertyChange, MemoryCacheLiveResourcePriorityHigh);
    }
#if ENABLE(OILPAN)
    m_clients.add(client, adoptPtr(new ImageLoaderClientRemover(*this, *client)));
#else
    m_clients.add(client);
#endif
}

void ImageLoader::willRemoveClient(ImageLoaderClient& client)
{
    if (client.requestsHighLiveResourceCachePriority()) {
        ASSERT(m_highPriorityClientCount);
        m_highPriorityClientCount--;
        if (m_image && !m_highPriorityClientCount)
            memoryCache()->updateDecodedResource(m_image.get(), UpdateForPropertyChange, MemoryCacheLiveResourcePriorityLow);
    }
}

void ImageLoader::removeClient(ImageLoaderClient* client)
{
    willRemoveClient(*client);
    m_clients.remove(client);
}

void ImageLoader::dispatchPendingLoadEvents()
{
    loadEventSender().dispatchPendingEvents();
}

void ImageLoader::dispatchPendingErrorEvents()
{
    errorEventSender().dispatchPendingEvents();
}

void ImageLoader::elementDidMoveToNewDocument()
{
    if (m_delayLoad) {
        m_delayLoad->documentChanged(m_element->document());
    }
    clearFailedLoadURL();
    setImage(0);
}

void ImageLoader::sourceImageChanged()
{
#if ENABLE(OILPAN)
    PersistentHeapHashMap<WeakMember<ImageLoaderClient>, OwnPtr<ImageLoaderClientRemover> >::iterator end = m_clients.end();
    for (PersistentHeapHashMap<WeakMember<ImageLoaderClient>, OwnPtr<ImageLoaderClientRemover> >::iterator it = m_clients.begin(); it != end; ++it) {
        it->key->notifyImageSourceChanged();
    }
#else
    HashSet<ImageLoaderClient*>::iterator end = m_clients.end();
    for (HashSet<ImageLoaderClient*>::iterator it = m_clients.begin(); it != end; ++it) {
        ImageLoaderClient* handle = *it;
        handle->notifyImageSourceChanged();
    }
#endif
}

inline void ImageLoader::clearFailedLoadURL()
{
    m_failedLoadURL = AtomicString();
}

#if ENABLE(OILPAN)
ImageLoader::ImageLoaderClientRemover::~ImageLoaderClientRemover()
{
    m_loader.willRemoveClient(m_client);
}
#endif

}

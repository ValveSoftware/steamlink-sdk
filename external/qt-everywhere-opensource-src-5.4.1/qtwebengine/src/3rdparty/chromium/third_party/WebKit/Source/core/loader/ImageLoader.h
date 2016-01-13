/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2009 Apple Inc. All rights reserved.
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

#ifndef ImageLoader_h
#define ImageLoader_h

#include "core/fetch/ImageResource.h"
#include "core/fetch/ImageResourceClient.h"
#include "core/fetch/ResourcePtr.h"
#include "platform/heap/Handle.h"
#include "wtf/HashSet.h"
#include "wtf/WeakPtr.h"
#include "wtf/text/AtomicString.h"

namespace WebCore {

class IncrementLoadEventDelayCount;

class ImageLoaderClient : public WillBeGarbageCollectedMixin {
public:
    virtual void notifyImageSourceChanged() = 0;

    // Determines whether the observed ImageResource should have higher priority in the decoded resources cache.
    virtual bool requestsHighLiveResourceCachePriority() { return false; }

    virtual void trace(Visitor*) { }

protected:
    ImageLoaderClient() { }
};

class Element;
class ImageLoader;
class RenderImageResource;

template<typename T> class EventSender;
typedef EventSender<ImageLoader> ImageEventSender;

class ImageLoader : public NoBaseWillBeGarbageCollectedFinalized<ImageLoader>, public ImageResourceClient {
public:
    explicit ImageLoader(Element*);
    virtual ~ImageLoader();
    void trace(Visitor*);

    enum LoadType {
        LoadNormally,
        ForceLoadImmediately
    };

    // This function should be called when the element is attached to a document; starts
    // loading if a load hasn't already been started.
    void updateFromElement(LoadType = LoadNormally);

    // This function should be called whenever the 'src' attribute is set, even if its value
    // doesn't change; starts new load unconditionally (matches Firefox and Opera behavior).
    void updateFromElementIgnoringPreviousError();

    void elementDidMoveToNewDocument();

    Element* element() const { return m_element; }
    bool imageComplete() const
    {
        return m_imageComplete && !m_pendingTask;
    }

    ImageResource* image() const { return m_image.get(); }
    void setImage(ImageResource*); // Cancels pending load events, and doesn't dispatch new ones.

    void setLoadManually(bool loadManually) { m_loadManually = loadManually; }

    bool hasPendingActivity() const
    {
        return m_hasPendingLoadEvent || m_hasPendingErrorEvent || m_pendingTask;
    }

    void dispatchPendingEvent(ImageEventSender*);

    static void dispatchPendingLoadEvents();
    static void dispatchPendingErrorEvents();

    void addClient(ImageLoaderClient*);
    void removeClient(ImageLoaderClient*);

protected:
    virtual void notifyFinished(Resource*) OVERRIDE;

private:
    class Task;

    // Called from the task or from updateFromElement to initiate the load.
    void doUpdateFromElement(bool bypassMainWorldCSP = false);

    virtual void dispatchLoadEvent() = 0;
    virtual String sourceURI(const AtomicString&) const = 0;

    void updatedHasPendingEvent();

    void dispatchPendingLoadEvent();
    void dispatchPendingErrorEvent();

    RenderImageResource* renderImageResource();
    void updateRenderer();

    void setImageWithoutConsideringPendingLoadEvent(ImageResource*);
    void sourceImageChanged();
    void clearFailedLoadURL();

    void timerFired(Timer<ImageLoader>*);

    KURL imageURL() const;

    // Used to determine whether to immediately initiate the load
    // or to schedule a microtask.
    bool shouldLoadImmediately(const KURL&) const;

    void willRemoveClient(ImageLoaderClient&);

    RawPtrWillBeMember<Element> m_element;
    ResourcePtr<ImageResource> m_image;
    // FIXME: Oilpan: We might be able to remove this Persistent hack when
    // ImageResourceClient is traceable.
    GC_PLUGIN_IGNORE("http://crbug.com/383741")
    RefPtrWillBePersistent<Element> m_keepAlive;
#if ENABLE(OILPAN)
    class ImageLoaderClientRemover {
    public:
        ImageLoaderClientRemover(ImageLoader& loader, ImageLoaderClient& client) : m_loader(loader), m_client(client) { }
        ~ImageLoaderClientRemover();

    private:
        ImageLoader& m_loader;
        ImageLoaderClient& m_client;
    };
    friend class ImageLoaderClientRemover;
    // Oilpan: This ImageLoader object must outlive its clients because they
    // need to call ImageLoader::willRemoveClient before they
    // die. Non-Persistent HeapHashMap doesn't work well because weak processing
    // for HeapHashMap is not triggered when both of ImageLoader and
    // ImageLoaderClient are unreachable.
    GC_PLUGIN_IGNORE("http://crbug.com/383742")
    PersistentHeapHashMap<WeakMember<ImageLoaderClient>, OwnPtr<ImageLoaderClientRemover> > m_clients;
#else
    HashSet<ImageLoaderClient*> m_clients;
#endif
    Timer<ImageLoader> m_derefElementTimer;
    AtomicString m_failedLoadURL;
    WeakPtr<Task> m_pendingTask; // owned by Microtask
    OwnPtr<IncrementLoadEventDelayCount> m_delayLoad;
    bool m_hasPendingLoadEvent : 1;
    bool m_hasPendingErrorEvent : 1;
    bool m_imageComplete : 1;
    bool m_loadManually : 1;
    bool m_elementIsProtected : 1;
    unsigned m_highPriorityClientCount;
};

}

#endif

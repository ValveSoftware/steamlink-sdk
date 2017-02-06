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

#include "core/CoreExport.h"
#include "core/fetch/ImageResource.h"
#include "core/fetch/ImageResourceObserver.h"
#include "core/fetch/ResourceClient.h"
#include "platform/heap/Handle.h"
#include "wtf/HashSet.h"
#include "wtf/WeakPtr.h"
#include "wtf/text/AtomicString.h"
#include <memory>

namespace blink {

class IncrementLoadEventDelayCount;
class FetchRequest;
class Document;
class Element;
class ImageLoader;
class LayoutImageResource;

template<typename T> class EventSender;
using ImageEventSender = EventSender<ImageLoader>;

class CORE_EXPORT ImageLoader : public GarbageCollectedFinalized<ImageLoader>, public ImageResourceObserver {
    USING_PRE_FINALIZER(ImageLoader, dispose);
public:
    explicit ImageLoader(Element*);
    ~ImageLoader() override;

    DECLARE_TRACE();

    enum UpdateFromElementBehavior {
        // This should be the update behavior when the element is attached to a document, or when DOM mutations trigger a new load.
        // Starts loading if a load hasn't already been started.
        UpdateNormal,
        // This should be the update behavior when the resource was changed (via 'src', 'srcset' or 'sizes').
        // Starts a new load even if a previous load of the same resource have failed, to match Firefox's behavior.
        // FIXME - Verify that this is the right behavior according to the spec.
        UpdateIgnorePreviousError,
        // This forces the image to update its intrinsic size, even if the image source has not changed.
        UpdateSizeChanged,
        // This force the image to refetch and reload the image source, even if it has not changed.
        UpdateForcedReload
    };

    enum BypassMainWorldBehavior {
        BypassMainWorldCSP,
        DoNotBypassMainWorldCSP
    };

    void updateFromElement(UpdateFromElementBehavior = UpdateNormal, ReferrerPolicy = ReferrerPolicyDefault);

    void elementDidMoveToNewDocument();

    Element* element() const { return m_element; }
    bool imageComplete() const
    {
        return m_imageComplete && !m_pendingTask;
    }

    ImageResource* image() const { return m_image.get(); }
    void setImage(ImageResource*); // Cancels pending load events, and doesn't dispatch new ones.

    void setLoadingImageDocument() { m_loadingImageDocument = true; }

    bool hasPendingActivity() const
    {
        return m_hasPendingLoadEvent || m_hasPendingErrorEvent || m_pendingTask;
    }

    bool hasPendingError() const
    {
        return m_hasPendingErrorEvent;
    }

    bool hadError() const
    {
        return !m_failedLoadURL.isEmpty();
    }

    void dispatchPendingEvent(ImageEventSender*);

    static void dispatchPendingLoadEvents();
    static void dispatchPendingErrorEvents();

    bool getImageAnimationPolicy(ImageAnimationPolicy&) final;
protected:
    void imageNotifyFinished(ImageResource*) override;

private:
    class Task;

    // Called from the task or from updateFromElement to initiate the load.
    void doUpdateFromElement(BypassMainWorldBehavior, UpdateFromElementBehavior, ReferrerPolicy = ReferrerPolicyDefault);

    virtual void dispatchLoadEvent() = 0;
    virtual void noImageResourceToLoad() { }

    void updatedHasPendingEvent();

    void dispatchPendingLoadEvent();
    void dispatchPendingErrorEvent();

    LayoutImageResource* layoutImageResource();
    void updateLayoutObject();

    void setImageWithoutConsideringPendingLoadEvent(ImageResource*);
    void clearFailedLoadURL();
    void dispatchErrorEvent();
    void crossSiteOrCSPViolationOccurred(AtomicString);
    void enqueueImageLoadingMicroTask(UpdateFromElementBehavior, ReferrerPolicy);

    void timerFired(Timer<ImageLoader>*);

    KURL imageSourceToKURL(AtomicString) const;

    // Used to determine whether to immediately initiate the load
    // or to schedule a microtask.
    bool shouldLoadImmediately(const KURL&) const;

    // For Oilpan, we must run dispose() as a prefinalizer and call
    // m_image->removeClient(this) (and more.) Otherwise, the ImageResource can invoke
    // didAddClient() for the ImageLoader that is about to die in the current
    // lazy sweeping, and the didAddClient() can access on-heap objects that
    // have already been finalized in the current lazy sweeping.
    void dispose();

    Member<Element> m_element;
    Member<ImageResource> m_image;
    // FIXME: Oilpan: We might be able to remove this Persistent hack when
    // ImageResourceClient is traceable.
    GC_PLUGIN_IGNORE("http://crbug.com/383741")
    Persistent<Element> m_keepAlive;

    Timer<ImageLoader> m_derefElementTimer;
    AtomicString m_failedLoadURL;
    WeakPtr<Task> m_pendingTask; // owned by Microtask
    std::unique_ptr<IncrementLoadEventDelayCount> m_loadDelayCounter;
    bool m_hasPendingLoadEvent : 1;
    bool m_hasPendingErrorEvent : 1;
    bool m_imageComplete : 1;
    bool m_loadingImageDocument : 1;
    bool m_elementIsProtected : 1;
    bool m_suppressErrorEvents : 1;
};

} // namespace blink

#endif

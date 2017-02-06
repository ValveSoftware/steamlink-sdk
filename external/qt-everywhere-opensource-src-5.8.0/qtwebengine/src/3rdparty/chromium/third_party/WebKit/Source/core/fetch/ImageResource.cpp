/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)
    Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
    Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "core/fetch/ImageResource.h"

#include "core/fetch/ImageResourceObserver.h"
#include "core/fetch/MemoryCache.h"
#include "core/fetch/ResourceClient.h"
#include "core/fetch/ResourceClientOrObserverWalker.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/fetch/ResourceLoader.h"
#include "core/svg/graphics/SVGImage.h"
#include "platform/Logging.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/SharedBuffer.h"
#include "platform/TraceEvent.h"
#include "platform/graphics/BitmapImage.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCachePolicy.h"
#include "wtf/CurrentTime.h"
#include "wtf/StdLibExtras.h"
#include <memory>

namespace blink {

using ImageResourceObserverWalker = ResourceClientOrObserverWalker<ImageResourceObserver, ImageResourceObserver>;

ImageResource* ImageResource::fetch(FetchRequest& request, ResourceFetcher* fetcher)
{
    if (request.resourceRequest().requestContext() == WebURLRequest::RequestContextUnspecified)
        request.mutableResourceRequest().setRequestContext(WebURLRequest::RequestContextImage);
    if (fetcher->context().pageDismissalEventBeingDispatched()) {
        KURL requestURL = request.resourceRequest().url();
        if (requestURL.isValid() && fetcher->context().canRequest(Resource::Image, request.resourceRequest(), requestURL, request.options(), request.forPreload(), request.getOriginRestriction()))
            fetcher->context().sendImagePing(requestURL);
        return nullptr;
    }

    return toImageResource(fetcher->requestResource(request, ImageResourceFactory()));
}

ImageResource::ImageResource(const ResourceRequest& resourceRequest, const ResourceLoaderOptions& options)
    : Resource(resourceRequest, Image, options)
    , m_devicePixelRatioHeaderValue(1.0)
    , m_image(nullptr)
    , m_hasDevicePixelRatioHeaderValue(false)
{
    WTF_LOG(ResourceLoading, "new ImageResource(ResourceRequest) %p", this);
}

ImageResource::ImageResource(blink::Image* image, const ResourceLoaderOptions& options)
    : Resource(ResourceRequest(""), Image, options)
    , m_devicePixelRatioHeaderValue(1.0)
    , m_image(image)
    , m_hasDevicePixelRatioHeaderValue(false)
{
    WTF_LOG(ResourceLoading, "new ImageResource(Image) %p", this);
    setStatus(Cached);
}

ImageResource::~ImageResource()
{
    WTF_LOG(ResourceLoading, "~ImageResource %p", this);
    clearImage();
}

DEFINE_TRACE(ImageResource)
{
    visitor->trace(m_multipartParser);
    Resource::trace(visitor);
    ImageObserver::trace(visitor);
    MultipartImageResourceParser::Client::trace(visitor);
}

void ImageResource::checkNotify()
{
    notifyObserversInternal(MarkFinishedOption::ShouldMarkFinished);
    Resource::checkNotify();
}

void ImageResource::notifyObserversInternal(MarkFinishedOption markFinishedOption)
{
    if (isLoading())
        return;

    ImageResourceObserverWalker walker(m_observers);
    while (auto* observer = walker.next()) {
        if (markFinishedOption == MarkFinishedOption::ShouldMarkFinished)
            markObserverFinished(observer);
        observer->imageNotifyFinished(this);
    }
}

void ImageResource::markObserverFinished(ImageResourceObserver* observer)
{
    if (m_observers.contains(observer)) {
        m_finishedObservers.add(observer);
        m_observers.remove(observer);
    }
}

void ImageResource::ensureImage()
{
    if (m_data && !m_image && !errorOccurred()) {
        createImage();
        m_image->setData(m_data, true);
    }
}

void ImageResource::didAddClient(ResourceClient* client)
{
    ensureImage();
    Resource::didAddClient(client);
}

void ImageResource::addObserver(ImageResourceObserver* observer)
{
    willAddClientOrObserver();

    m_observers.add(observer);

    if (isCacheValidator())
        return;

    ensureImage();

    if (m_image && !m_image->isNull()) {
        observer->imageChanged(this);
    }

    if (isLoaded()) {
        markObserverFinished(observer);
        observer->imageNotifyFinished(this);
    }
}

void ImageResource::removeObserver(ImageResourceObserver* observer)
{
    ASSERT(observer);

    if (m_observers.contains(observer))
        m_observers.remove(observer);
    else if (m_finishedObservers.contains(observer))
        m_finishedObservers.remove(observer);
    else
        ASSERT_NOT_REACHED();

    didRemoveClientOrObserver();
}

static void priorityFromObserver(const ImageResourceObserver* observer, ResourcePriority& priority)
{
    ResourcePriority nextPriority = observer->computeResourcePriority();
    if (nextPriority.visibility == ResourcePriority::NotVisible)
        return;
    priority.visibility = ResourcePriority::Visible;
    priority.intraPriorityValue += nextPriority.intraPriorityValue;
}

ResourcePriority ImageResource::priorityFromObservers()
{
    ResourcePriority priority;

    ImageResourceObserverWalker finishedWalker(m_finishedObservers);
    while (const auto* observer = finishedWalker.next()) {
        priorityFromObserver(observer, priority);
    }

    ImageResourceObserverWalker walker(m_observers);
    while (const auto* observer = walker.next()) {
        priorityFromObserver(observer, priority);
    }

    return priority;
}

bool ImageResource::isSafeToUnlock() const
{
    // Note that |m_image| holds a reference to |m_data| in addition to the one held by the Resource parent class.
    return !m_image || (m_image->hasOneRef() && m_data->refCount() == 2);
}

void ImageResource::destroyDecodedDataForFailedRevalidation()
{
    clearImage();
    setDecodedSize(0);
}

void ImageResource::destroyDecodedDataIfPossible()
{
    if (!hasClientsOrObservers() && !isLoading() && (!m_image || (m_image->hasOneRef() && m_image->isBitmapImage()))) {
        clearImage();
        setDecodedSize(0);
    } else if (m_image && !errorOccurred()) {
        m_image->destroyDecodedData();
    }
}

void ImageResource::doResetAnimation()
{
    if (m_image)
        m_image->resetAnimation();
}

void ImageResource::allClientsAndObserversRemoved()
{
    if (m_image && !errorOccurred()) {
        // If possible, delay the resetting until back at the event loop.
        // Doing so after a conservative GC prevents resetAnimation() from
        // upsetting ongoing animation updates (crbug.com/613709)
        if (!ThreadHeap::willObjectBeLazilySwept(this))
            Platform::current()->currentThread()->getWebTaskRunner()->postTask(BLINK_FROM_HERE, WTF::bind(&ImageResource::doResetAnimation, wrapWeakPersistent(this)));
        else
            m_image->resetAnimation();
    }
    if (m_multipartParser)
        m_multipartParser->cancel();
    Resource::allClientsAndObserversRemoved();
}

void ImageResource::appendData(const char* data, size_t length)
{
    if (m_multipartParser) {
        m_multipartParser->appendData(data, length);
    } else {
        Resource::appendData(data, length);
        updateImage(false);
    }
}

std::pair<blink::Image*, float> ImageResource::brokenImage(float deviceScaleFactor)
{
    if (deviceScaleFactor >= 2) {
        DEFINE_STATIC_REF(blink::Image, brokenImageHiRes, (blink::Image::loadPlatformResource("missingImage@2x")));
        return std::make_pair(brokenImageHiRes, 2);
    }

    DEFINE_STATIC_REF(blink::Image, brokenImageLoRes, (blink::Image::loadPlatformResource("missingImage")));
    return std::make_pair(brokenImageLoRes, 1);
}

bool ImageResource::willPaintBrokenImage() const
{
    return errorOccurred();
}

blink::Image* ImageResource::getImage()
{
    ASSERT(!isPurgeable());

    if (errorOccurred()) {
        // Returning the 1x broken image is non-ideal, but we cannot reliably access the appropriate
        // deviceScaleFactor from here. It is critical that callers use ImageResource::brokenImage()
        // when they need the real, deviceScaleFactor-appropriate broken image icon.
        return brokenImage(1).first;
    }

    if (m_image)
        return m_image.get();

    return blink::Image::nullImage();
}

bool ImageResource::usesImageContainerSize() const
{
    if (m_image)
        return m_image->usesContainerSize();

    return false;
}

bool ImageResource::imageHasRelativeSize() const
{
    if (m_image)
        return m_image->hasRelativeSize();

    return false;
}

LayoutSize ImageResource::imageSize(RespectImageOrientationEnum shouldRespectImageOrientation, float multiplier, SizeType sizeType)
{
    ASSERT(!isPurgeable());

    if (!m_image)
        return LayoutSize();

    LayoutSize size;

    if (m_image->isBitmapImage() && shouldRespectImageOrientation == RespectImageOrientation)
        size = LayoutSize(toBitmapImage(m_image.get())->sizeRespectingOrientation());
    else
        size = LayoutSize(m_image->size());

    if (sizeType == IntrinsicCorrectedToDPR && m_hasDevicePixelRatioHeaderValue && m_devicePixelRatioHeaderValue > 0)
        multiplier = 1 / m_devicePixelRatioHeaderValue;

    if (multiplier == 1 || m_image->hasRelativeSize())
        return size;

    // Don't let images that have a width/height >= 1 shrink below 1 when zoomed.
    LayoutSize minimumSize(size.width() > LayoutUnit() ? LayoutUnit(1) : LayoutUnit(),
        LayoutUnit(size.height() > LayoutUnit() ? LayoutUnit(1) : LayoutUnit()));
    size.scale(multiplier);
    size.clampToMinimumSize(minimumSize);
    return size;
}

void ImageResource::notifyObservers(const IntRect* changeRect)
{
    ImageResourceObserverWalker finishedWalker(m_finishedObservers);
    while (auto* observer = finishedWalker.next()) {
        observer->imageChanged(this, changeRect);
    }

    ImageResourceObserverWalker walker(m_observers);
    while (auto* observer = walker.next()) {
        observer->imageChanged(this, changeRect);
    }
}

void ImageResource::clear()
{
    prune();
    clearImage();
    m_data.clear();
    setEncodedSize(0);
}

inline void ImageResource::createImage()
{
    // Create the image if it doesn't yet exist.
    if (m_image)
        return;

    if (m_response.mimeType() == "image/svg+xml") {
        m_image = SVGImage::create(this);
    } else {
        m_image = BitmapImage::create(this);
    }
}

inline void ImageResource::clearImage()
{
    if (!m_image)
        return;

    // If our Image has an observer, it's always us so we need to clear the back pointer
    // before dropping our reference.
    m_image->clearImageObserver();
    m_image.clear();
}

void ImageResource::updateImage(bool allDataReceived)
{
    TRACE_EVENT0("blink", "ImageResource::updateImage");

    if (m_data)
        createImage();

    bool sizeAvailable = false;

    // Have the image update its data from its internal buffer.
    // It will not do anything now, but will delay decoding until
    // queried for info (like size or specific image frames).
    if (m_image)
        sizeAvailable = m_image->setData(m_data, allDataReceived);

    // Go ahead and tell our observers to try to draw if we have either
    // received all the data or the size is known. Each chunk from the
    // network causes observers to repaint, which will force that chunk
    // to decode.
    if (sizeAvailable || allDataReceived) {
        if (!m_image || m_image->isNull()) {
            if (!errorOccurred())
                setStatus(DecodeError);
            clear();
            if (memoryCache()->contains(this))
                memoryCache()->remove(this);
        }

        // It would be nice to only redraw the decoded band of the image, but with the current design
        // (decoding delayed until painting) that seems hard.
        notifyObservers();
    }
}

void ImageResource::updateImageAndClearBuffer()
{
    clearImage();
    updateImage(true);
    m_data.clear();
}

void ImageResource::finish(double loadFinishTime)
{
    if (m_multipartParser) {
        m_multipartParser->finish();
        if (m_data)
            updateImageAndClearBuffer();
    } else {
        updateImage(true);
    }
    Resource::finish(loadFinishTime);
}

void ImageResource::error(const ResourceError& error)
{
    if (m_multipartParser)
        m_multipartParser->cancel();
    clear();
    Resource::error(error);
    notifyObservers();
}

void ImageResource::responseReceived(const ResourceResponse& response, std::unique_ptr<WebDataConsumerHandle> handle)
{
    ASSERT(!handle);
    ASSERT(!m_multipartParser);
    // If there's no boundary, just handle the request normally.
    if (response.isMultipart() && !response.multipartBoundary().isEmpty())
        m_multipartParser = new MultipartImageResourceParser(response, response.multipartBoundary(), this);
    Resource::responseReceived(response, std::move(handle));
    if (RuntimeEnabledFeatures::clientHintsEnabled()) {
        m_devicePixelRatioHeaderValue = m_response.httpHeaderField(HTTPNames::Content_DPR).toFloat(&m_hasDevicePixelRatioHeaderValue);
        if (!m_hasDevicePixelRatioHeaderValue || m_devicePixelRatioHeaderValue <= 0.0) {
            m_devicePixelRatioHeaderValue = 1.0;
            m_hasDevicePixelRatioHeaderValue = false;
        }

    }
}

void ImageResource::decodedSizeChangedTo(const blink::Image* image, size_t newSize)
{
    if (!image || image != m_image)
        return;

    setDecodedSize(newSize);
}

void ImageResource::didDraw(const blink::Image* image)
{
    if (!image || image != m_image)
        return;
    // decodedSize() == 0 indicates that the image is decoded into DiscardableMemory,
    // not in MemoryCache. So we don't need to call Resource::didAccessDecodedData()
    // to update MemoryCache.
    if (decodedSize() != 0)
        Resource::didAccessDecodedData();
}

bool ImageResource::shouldPauseAnimation(const blink::Image* image)
{
    if (!image || image != m_image)
        return false;

    ImageResourceObserverWalker finishedWalker(m_finishedObservers);
    while (auto* observer = finishedWalker.next()) {
        if (observer->willRenderImage())
            return false;
    }

    ImageResourceObserverWalker walker(m_observers);
    while (auto* observer = walker.next()) {
        if (observer->willRenderImage())
            return false;
    }

    return true;
}

void ImageResource::animationAdvanced(const blink::Image* image)
{
    if (!image || image != m_image)
        return;
    notifyObservers();
}

void ImageResource::updateImageAnimationPolicy()
{
    if (!m_image)
        return;

    ImageAnimationPolicy newPolicy = ImageAnimationPolicyAllowed;

    ImageResourceObserverWalker finishedWalker(m_finishedObservers);
    while (auto* observer = finishedWalker.next()) {
        if (observer->getImageAnimationPolicy(newPolicy))
            break;
    }

    ImageResourceObserverWalker walker(m_observers);
    while (auto* observer = walker.next()) {
        if (observer->getImageAnimationPolicy(newPolicy))
            break;
    }

    if (m_image->animationPolicy() != newPolicy) {
        m_image->resetAnimation();
        m_image->setAnimationPolicy(newPolicy);
    }
}

void ImageResource::reloadIfLoFi(ResourceFetcher* fetcher)
{
    if (m_resourceRequest.loFiState() != WebURLRequest::LoFiOn)
        return;
    if (isLoaded() && !m_response.httpHeaderField("chrome-proxy").contains("q=low"))
        return;
    m_resourceRequest.setCachePolicy(WebCachePolicy::BypassingCache);
    m_resourceRequest.setLoFiState(WebURLRequest::LoFiOff);
    if (isLoading())
        m_loader->cancel();
    clear();
    m_data.clear();
    notifyObservers();
    setStatus(NotStarted);
    fetcher->startLoad(this);
}

void ImageResource::changedInRect(const blink::Image* image, const IntRect& rect)
{
    if (!image || image != m_image)
        return;
    notifyObservers(&rect);
}

void ImageResource::onePartInMultipartReceived(const ResourceResponse& response)
{
    ASSERT(m_multipartParser);

    m_response = response;
    if (m_multipartParsingState == MultipartParsingState::WaitingForFirstPart) {
        // We have nothing to do because we don't have any data.
        m_multipartParsingState = MultipartParsingState::ParsingFirstPart;
        return;
    }
    updateImageAndClearBuffer();

    if (m_multipartParsingState == MultipartParsingState::ParsingFirstPart) {
        m_multipartParsingState = MultipartParsingState::FinishedParsingFirstPart;
        // Notify finished when the first part ends.
        if (!errorOccurred())
            setStatus(Cached);
        // We will also notify clients/observers of the finish in
        // Resource::finish()/error() so we don't mark them finished here.
        notifyObserversInternal(MarkFinishedOption::DoNotMarkFinished);
        notifyClientsInternal(MarkFinishedOption::DoNotMarkFinished);
        if (m_loader)
            m_loader->didFinishLoadingFirstPartInMultipart();
    }
}

void ImageResource::multipartDataReceived(const char* bytes, size_t size)
{
    ASSERT(m_multipartParser);
    Resource::appendData(bytes, size);
}

bool ImageResource::isAccessAllowed(SecurityOrigin* securityOrigin)
{
    if (response().wasFetchedViaServiceWorker())
        return response().serviceWorkerResponseType() != WebServiceWorkerResponseTypeOpaque;
    if (!getImage()->currentFrameHasSingleSecurityOrigin())
        return false;
    if (passesAccessControlCheck(securityOrigin))
        return true;
    return !securityOrigin->taintsCanvas(response().url());
}

} // namespace blink

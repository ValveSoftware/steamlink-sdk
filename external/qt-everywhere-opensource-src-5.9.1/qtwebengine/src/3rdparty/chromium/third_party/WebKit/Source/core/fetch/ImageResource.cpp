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
#include "core/fetch/ResourceFetcher.h"
#include "core/fetch/ResourceLoader.h"
#include "core/fetch/ResourceLoadingLog.h"
#include "core/svg/graphics/SVGImage.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/SharedBuffer.h"
#include "platform/geometry/IntSize.h"
#include "platform/graphics/BitmapImage.h"
#include "platform/graphics/PlaceholderImage.h"
#include "platform/tracing/TraceEvent.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCachePolicy.h"
#include "wtf/CurrentTime.h"
#include "wtf/HashCountedSet.h"
#include "wtf/StdLibExtras.h"
#include "wtf/Vector.h"
#include <memory>
#include <v8.h>

namespace blink {
namespace {
// The amount of time to wait before informing the clients that the image has
// been updated (in seconds). This effectively throttles invalidations that
// result from new data arriving for this image.
constexpr double kFlushDelaySeconds = 1.;
}  // namespace

class ImageResource::ImageResourceFactory : public ResourceFactory {
  STACK_ALLOCATED();

 public:
  ImageResourceFactory(const FetchRequest& fetchRequest)
      : ResourceFactory(Resource::Image), m_fetchRequest(&fetchRequest) {}

  Resource* create(const ResourceRequest& request,
                   const ResourceLoaderOptions& options,
                   const String&) const override {
    return new ImageResource(request, options,
                             m_fetchRequest->placeholderImageRequestType() ==
                                 FetchRequest::AllowPlaceholder);
  }

 private:
  // Weak, unowned pointer. Must outlive |this|.
  const FetchRequest* m_fetchRequest;
};

ImageResource* ImageResource::fetch(FetchRequest& request,
                                    ResourceFetcher* fetcher) {
  if (request.resourceRequest().requestContext() ==
      WebURLRequest::RequestContextUnspecified) {
    request.mutableResourceRequest().setRequestContext(
        WebURLRequest::RequestContextImage);
  }
  if (fetcher->context().pageDismissalEventBeingDispatched()) {
    KURL requestURL = request.resourceRequest().url();
    if (requestURL.isValid() &&
        fetcher->context().canRequest(Resource::Image,
                                      request.resourceRequest(), requestURL,
                                      request.options(), request.forPreload(),
                                      request.getOriginRestriction()))
      fetcher->context().sendImagePing(requestURL);
    return nullptr;
  }

  ImageResource* resource = toImageResource(
      fetcher->requestResource(request, ImageResourceFactory(request)));
  if (resource &&
      request.placeholderImageRequestType() != FetchRequest::AllowPlaceholder &&
      resource->m_isPlaceholder) {
    // If the image is a placeholder, but this fetch doesn't allow a
    // placeholder, then load the original image. Note that the cache is not
    // bypassed here - it should be fine to use a cached copy if possible.
    resource->reloadIfLoFiOrPlaceholder(fetcher,
                                        ReloadCachePolicy::UseExistingPolicy);
  }
  return resource;
}

ImageResource::ImageResource(const ResourceRequest& resourceRequest,
                             const ResourceLoaderOptions& options,
                             bool isPlaceholder)
    : Resource(resourceRequest, Image, options),
      m_devicePixelRatioHeaderValue(1.0),
      m_image(nullptr),
      m_hasDevicePixelRatioHeaderValue(false),
      m_isSchedulingReload(false),
      m_isPlaceholder(isPlaceholder),
      m_flushTimer(this, &ImageResource::flushImageIfNeeded) {
  RESOURCE_LOADING_DVLOG(1) << "new ImageResource(ResourceRequest) " << this;
}

ImageResource::ImageResource(blink::Image* image,
                             const ResourceLoaderOptions& options)
    : Resource(ResourceRequest(""), Image, options),
      m_devicePixelRatioHeaderValue(1.0),
      m_image(image),
      m_hasDevicePixelRatioHeaderValue(false),
      m_isSchedulingReload(false),
      m_isPlaceholder(false),
      m_flushTimer(this, &ImageResource::flushImageIfNeeded) {
  RESOURCE_LOADING_DVLOG(1) << "new ImageResource(Image) " << this;
  setStatus(Cached);
}

ImageResource::~ImageResource() {
  RESOURCE_LOADING_DVLOG(1) << "~ImageResource " << this;
  clearImage();
}

DEFINE_TRACE(ImageResource) {
  visitor->trace(m_multipartParser);
  Resource::trace(visitor);
  ImageObserver::trace(visitor);
  MultipartImageResourceParser::Client::trace(visitor);
}

void ImageResource::checkNotify() {
  // Don't notify observers and clients of completion if this ImageResource is
  // about to be reloaded.
  if (m_isSchedulingReload || shouldReloadBrokenPlaceholder())
    return;

  notifyObserversInternal(MarkFinishedOption::ShouldMarkFinished);
  Resource::checkNotify();
}

void ImageResource::notifyObserversInternal(
    MarkFinishedOption markFinishedOption) {
  if (isLoading())
    return;

  for (auto* observer : m_observers.asVector()) {
    if (m_observers.contains(observer)) {
      if (markFinishedOption == MarkFinishedOption::ShouldMarkFinished)
        markObserverFinished(observer);
      observer->imageNotifyFinished(this);
    }
  }
}

void ImageResource::markObserverFinished(ImageResourceObserver* observer) {
  if (m_observers.contains(observer)) {
    m_finishedObservers.add(observer);
    m_observers.remove(observer);
  }
}

void ImageResource::didAddClient(ResourceClient* client) {
  DCHECK((m_multipartParser && isLoading()) || !data() || m_image);

  // Don't notify observers and clients of completion if this ImageResource is
  // about to be reloaded.
  if (m_isSchedulingReload || shouldReloadBrokenPlaceholder())
    return;

  Resource::didAddClient(client);
}

void ImageResource::addObserver(ImageResourceObserver* observer) {
  willAddClientOrObserver(MarkAsReferenced);

  m_observers.add(observer);

  if (isCacheValidator())
    return;

  // When the response is not multipart, if |data()| exists, |m_image| must be
  // created. This is assured that |updateImage()| is called when |appendData()|
  // is called.
  //
  // On the other hand, when the response is multipart, |updateImage()| is not
  // called in |appendData()|, which means |m_image| might not be created even
  // when |data()| exists. This is intentional since creating a |m_image| on
  // receiving data might destroy an existing image in a previous part.
  DCHECK((m_multipartParser && isLoading()) || !data() || m_image);

  if (m_image && !m_image->isNull()) {
    observer->imageChanged(this);
  }

  if (isLoaded() && !m_isSchedulingReload && !shouldReloadBrokenPlaceholder()) {
    markObserverFinished(observer);
    observer->imageNotifyFinished(this);
  }
}

void ImageResource::removeObserver(ImageResourceObserver* observer) {
  DCHECK(observer);

  if (m_observers.contains(observer))
    m_observers.remove(observer);
  else if (m_finishedObservers.contains(observer))
    m_finishedObservers.remove(observer);
  else
    NOTREACHED();

  didRemoveClientOrObserver();
}

static void priorityFromObserver(const ImageResourceObserver* observer,
                                 ResourcePriority& priority) {
  ResourcePriority nextPriority = observer->computeResourcePriority();
  if (nextPriority.visibility == ResourcePriority::NotVisible)
    return;
  priority.visibility = ResourcePriority::Visible;
  priority.intraPriorityValue += nextPriority.intraPriorityValue;
}

ResourcePriority ImageResource::priorityFromObservers() {
  ResourcePriority priority;

  for (auto* observer : m_finishedObservers.asVector()) {
    if (m_finishedObservers.contains(observer))
      priorityFromObserver(observer, priority);
  }
  for (auto* observer : m_observers.asVector()) {
    if (m_observers.contains(observer))
      priorityFromObserver(observer, priority);
  }

  return priority;
}

void ImageResource::destroyDecodedDataForFailedRevalidation() {
  clearImage();
  setDecodedSize(0);
}

void ImageResource::destroyDecodedDataIfPossible() {
  if (!m_image)
    return;
  CHECK(!errorOccurred());
  m_image->destroyDecodedData();
}

void ImageResource::doResetAnimation() {
  if (m_image)
    m_image->resetAnimation();
}

void ImageResource::allClientsAndObserversRemoved() {
  if (m_image) {
    CHECK(!errorOccurred());
    // If possible, delay the resetting until back at the event loop. Doing so
    // after a conservative GC prevents resetAnimation() from upsetting ongoing
    // animation updates (crbug.com/613709)
    if (!ThreadHeap::willObjectBeLazilySwept(this)) {
      Platform::current()->currentThread()->getWebTaskRunner()->postTask(
          BLINK_FROM_HERE, WTF::bind(&ImageResource::doResetAnimation,
                                     wrapWeakPersistent(this)));
    } else {
      m_image->resetAnimation();
    }
  }
  if (m_multipartParser)
    m_multipartParser->cancel();
  Resource::allClientsAndObserversRemoved();
}

PassRefPtr<const SharedBuffer> ImageResource::resourceBuffer() const {
  if (data())
    return data();
  if (m_image)
    return m_image->data();
  return nullptr;
}

void ImageResource::appendData(const char* data, size_t length) {
  v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(length);
  if (m_multipartParser) {
    m_multipartParser->appendData(data, length);
  } else {
    Resource::appendData(data, length);

    // If we don't have the size available yet, then update immediately since
    // we need to know the image size as soon as possible. Likewise for
    // animated images, update right away since we shouldn't throttle animated
    // images.
    if (m_sizeAvailable == Image::SizeUnavailable ||
        (m_image && m_image->maybeAnimated())) {
      updateImage(false);
      return;
    }

    // For other cases, only update at |kFlushDelaySeconds| intervals. This
    // throttles how frequently we update |m_image| and how frequently we
    // inform the clients which causes an invalidation of this image. In other
    // words, we only invalidate this image every |kFlushDelaySeconds| seconds
    // while loading.
    if (!m_flushTimer.isActive()) {
      double now = WTF::monotonicallyIncreasingTime();
      if (!m_lastFlushTime)
        m_lastFlushTime = now;

      DCHECK_LE(m_lastFlushTime, now);
      double flushDelay = m_lastFlushTime - now + kFlushDelaySeconds;
      if (flushDelay < 0.)
        flushDelay = 0.;
      m_flushTimer.startOneShot(flushDelay, BLINK_FROM_HERE);
    }
  }
}

void ImageResource::flushImageIfNeeded(TimerBase*) {
  // We might have already loaded the image fully, in which case we don't need
  // to call |updateImage()|.
  if (isLoading()) {
    m_lastFlushTime = WTF::monotonicallyIncreasingTime();
    updateImage(false);
  }
}

std::pair<blink::Image*, float> ImageResource::brokenImage(
    float deviceScaleFactor) {
  if (deviceScaleFactor >= 2) {
    DEFINE_STATIC_REF(blink::Image, brokenImageHiRes,
                      (blink::Image::loadPlatformResource("missingImage@2x")));
    return std::make_pair(brokenImageHiRes, 2);
  }

  DEFINE_STATIC_REF(blink::Image, brokenImageLoRes,
                    (blink::Image::loadPlatformResource("missingImage")));
  return std::make_pair(brokenImageLoRes, 1);
}

bool ImageResource::willPaintBrokenImage() const {
  return errorOccurred();
}

blink::Image* ImageResource::getImage() {
  if (errorOccurred()) {
    // Returning the 1x broken image is non-ideal, but we cannot reliably access
    // the appropriate deviceScaleFactor from here. It is critical that callers
    // use ImageResource::brokenImage() when they need the real,
    // deviceScaleFactor-appropriate broken image icon.
    return brokenImage(1).first;
  }

  if (m_image)
    return m_image.get();

  return blink::Image::nullImage();
}

bool ImageResource::usesImageContainerSize() const {
  if (m_image)
    return m_image->usesContainerSize();

  return false;
}

bool ImageResource::imageHasRelativeSize() const {
  if (m_image)
    return m_image->hasRelativeSize();

  return false;
}

LayoutSize ImageResource::imageSize(
    RespectImageOrientationEnum shouldRespectImageOrientation,
    float multiplier,
    SizeType sizeType) {
  if (!m_image)
    return LayoutSize();

  LayoutSize size;

  if (m_image->isBitmapImage() &&
      shouldRespectImageOrientation == RespectImageOrientation) {
    size =
        LayoutSize(toBitmapImage(m_image.get())->sizeRespectingOrientation());
  } else {
    size = LayoutSize(m_image->size());
  }

  if (sizeType == IntrinsicCorrectedToDPR && m_hasDevicePixelRatioHeaderValue &&
      m_devicePixelRatioHeaderValue > 0)
    multiplier = 1 / m_devicePixelRatioHeaderValue;

  if (multiplier == 1 || m_image->hasRelativeSize())
    return size;

  // Don't let images that have a width/height >= 1 shrink below 1 when zoomed.
  LayoutSize minimumSize(
      size.width() > LayoutUnit() ? LayoutUnit(1) : LayoutUnit(),
      LayoutUnit(size.height() > LayoutUnit() ? LayoutUnit(1) : LayoutUnit()));
  size.scale(multiplier);
  size.clampToMinimumSize(minimumSize);
  return size;
}

void ImageResource::notifyObservers(const IntRect* changeRect) {
  for (auto* observer : m_finishedObservers.asVector()) {
    if (m_finishedObservers.contains(observer))
      observer->imageChanged(this, changeRect);
  }
  for (auto* observer : m_observers.asVector()) {
    if (m_observers.contains(observer))
      observer->imageChanged(this, changeRect);
  }
}

void ImageResource::clear() {
  clearImage();
  clearData();
  setEncodedSize(0);
}

inline void ImageResource::createImage() {
  // Create the image if it doesn't yet exist.
  if (m_image)
    return;

  if (response().mimeType() == "image/svg+xml") {
    m_image = SVGImage::create(this);
  } else {
    m_image = BitmapImage::create(this);
  }
}

inline void ImageResource::clearImage() {
  if (!m_image)
    return;
  int64_t length = m_image->data() ? m_image->data()->size() : 0;
  v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(-length);

  // If our Image has an observer, it's always us so we need to clear the back
  // pointer before dropping our reference.
  m_image->clearImageObserver();
  m_image.clear();
  m_sizeAvailable = Image::SizeUnavailable;
}

void ImageResource::updateImage(bool allDataReceived) {
  TRACE_EVENT0("blink", "ImageResource::updateImage");

  if (data())
    createImage();

  // Have the image update its data from its internal buffer. It will not do
  // anything now, but will delay decoding until queried for info (like size or
  // specific image frames).
  if (data()) {
    DCHECK(m_image);
    m_sizeAvailable = m_image->setData(data(), allDataReceived);
  }

  // Go ahead and tell our observers to try to draw if we have either received
  // all the data or the size is known. Each chunk from the network causes
  // observers to repaint, which will force that chunk to decode.
  if (m_sizeAvailable == Image::SizeUnavailable && !allDataReceived)
    return;

  if (m_isPlaceholder && allDataReceived && m_image && !m_image->isNull()) {
    if (m_sizeAvailable == Image::SizeAvailable) {
      // TODO(sclittle): Show the original image if the response consists of the
      // entire image, such as if the entire image response body is smaller than
      // the requested range.
      IntSize dimensions = m_image->size();
      clearImage();
      m_image = PlaceholderImage::create(this, dimensions);
    } else {
      // Clear the image so that it gets treated like a decoding error, since
      // the attempt to build a placeholder image failed.
      clearImage();
    }
  }

  if (!m_image || m_image->isNull()) {
    size_t size = encodedSize();
    clear();
    if (!errorOccurred())
      setStatus(DecodeError);
    if (!allDataReceived && loader())
      loader()->didFinishLoading(nullptr, monotonicallyIncreasingTime(), size);
    memoryCache()->remove(this);
  }

  // It would be nice to only redraw the decoded band of the image, but with the
  // current design (decoding delayed until painting) that seems hard.
  notifyObservers();
}

void ImageResource::updateImageAndClearBuffer() {
  clearImage();
  updateImage(true);
  clearData();
}

void ImageResource::finish(double loadFinishTime) {
  if (m_multipartParser) {
    m_multipartParser->finish();
    if (data())
      updateImageAndClearBuffer();
  } else {
    updateImage(true);
    // As encoded image data can be created from m_image  (see
    // ImageResource::resourceBuffer(), we don't have to keep m_data. Let's
    // clear this. As for the lifetimes of m_image and m_data, see this
    // document:
    // https://docs.google.com/document/d/1v0yTAZ6wkqX2U_M6BNIGUJpM1s0TIw1VsqpxoL7aciY/edit?usp=sharing
    clearData();
  }
  Resource::finish(loadFinishTime);
}

void ImageResource::error(const ResourceError& error) {
  if (m_multipartParser)
    m_multipartParser->cancel();
  clear();
  Resource::error(error);
  notifyObservers();
}

void ImageResource::responseReceived(
    const ResourceResponse& response,
    std::unique_ptr<WebDataConsumerHandle> handle) {
  DCHECK(!handle);
  DCHECK(!m_multipartParser);
  // If there's no boundary, just handle the request normally.
  if (response.isMultipart() && !response.multipartBoundary().isEmpty()) {
    m_multipartParser = new MultipartImageResourceParser(
        response, response.multipartBoundary(), this);
  }
  Resource::responseReceived(response, std::move(handle));
  if (RuntimeEnabledFeatures::clientHintsEnabled()) {
    m_devicePixelRatioHeaderValue =
        this->response()
            .httpHeaderField(HTTPNames::Content_DPR)
            .toFloat(&m_hasDevicePixelRatioHeaderValue);
    if (!m_hasDevicePixelRatioHeaderValue ||
        m_devicePixelRatioHeaderValue <= 0.0) {
      m_devicePixelRatioHeaderValue = 1.0;
      m_hasDevicePixelRatioHeaderValue = false;
    }
  }
}

void ImageResource::decodedSizeChangedTo(const blink::Image* image,
                                         size_t newSize) {
  if (!image || image != m_image)
    return;

  setDecodedSize(newSize);
}

void ImageResource::didDraw(const blink::Image* image) {}

bool ImageResource::shouldPauseAnimation(const blink::Image* image) {
  if (!image || image != m_image)
    return false;

  for (auto* observer : m_finishedObservers.asVector()) {
    if (m_finishedObservers.contains(observer) && observer->willRenderImage())
      return false;
  }

  for (auto* observer : m_observers.asVector()) {
    if (m_observers.contains(observer) && observer->willRenderImage())
      return false;
  }

  return true;
}

void ImageResource::animationAdvanced(const blink::Image* image) {
  if (!image || image != m_image)
    return;
  notifyObservers();
}

void ImageResource::updateImageAnimationPolicy() {
  if (!m_image)
    return;

  ImageAnimationPolicy newPolicy = ImageAnimationPolicyAllowed;
  for (auto* observer : m_finishedObservers.asVector()) {
    if (m_finishedObservers.contains(observer) &&
        observer->getImageAnimationPolicy(newPolicy))
      break;
  }
  for (auto* observer : m_observers.asVector()) {
    if (m_observers.contains(observer) &&
        observer->getImageAnimationPolicy(newPolicy))
      break;
  }

  if (m_image->animationPolicy() != newPolicy) {
    m_image->resetAnimation();
    m_image->setAnimationPolicy(newPolicy);
  }
}

static bool isLoFiImage(const ImageResource& resource) {
  if (resource.resourceRequest().loFiState() != WebURLRequest::LoFiOn)
    return false;
  return !resource.isLoaded() ||
         resource.response()
             .httpHeaderField("chrome-proxy-content-transform")
             .contains("empty-image");
}

void ImageResource::reloadIfLoFiOrPlaceholder(
    ResourceFetcher* fetcher,
    ReloadCachePolicy reloadCachePolicy) {
  if (!m_isPlaceholder && !isLoFiImage(*this))
    return;

  // Prevent clients and observers from being notified of completion while the
  // reload is being scheduled, so that e.g. canceling an existing load in
  // progress doesn't cause clients and observers to be notified of completion
  // prematurely.
  DCHECK(!m_isSchedulingReload);
  m_isSchedulingReload = true;

  if (reloadCachePolicy == ReloadCachePolicy::BypassCache)
    setCachePolicyBypassingCache();
  setLoFiStateOff();

  if (m_isPlaceholder) {
    m_isPlaceholder = false;
    clearRangeRequestHeader();
  }

  if (isLoading()) {
    loader()->cancel();
    // Canceling the loader causes error() to be called, which in turn calls
    // clear() and notifyObservers(), so there's no need to call these again
    // here.
  } else {
    clear();
    notifyObservers();
  }

  setStatus(NotStarted);

  DCHECK(m_isSchedulingReload);
  m_isSchedulingReload = false;

  fetcher->startLoad(this);
}

void ImageResource::changedInRect(const blink::Image* image,
                                  const IntRect& rect) {
  if (!image || image != m_image)
    return;
  notifyObservers(&rect);
}

void ImageResource::onePartInMultipartReceived(
    const ResourceResponse& response) {
  DCHECK(m_multipartParser);

  setResponse(response);
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
    if (loader())
      loader()->didFinishLoadingFirstPartInMultipart();
  }
}

void ImageResource::multipartDataReceived(const char* bytes, size_t size) {
  DCHECK(m_multipartParser);
  Resource::appendData(bytes, size);
}

bool ImageResource::isAccessAllowed(SecurityOrigin* securityOrigin) {
  if (response().wasFetchedViaServiceWorker()) {
    return response().serviceWorkerResponseType() !=
           WebServiceWorkerResponseTypeOpaque;
  }
  if (!getImage()->currentFrameHasSingleSecurityOrigin())
    return false;
  if (passesAccessControlCheck(securityOrigin))
    return true;
  return !securityOrigin->taintsCanvas(response().url());
}

}  // namespace blink

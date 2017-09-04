/*
 * Copyright (C) 2006, 2007, 2008, 2010 Apple Inc. All rights reserved.
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/html/ImageDocument.h"

#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "core/HTMLNames.h"
#include "core/dom/RawDataDocumentParser.h"
#include "core/events/EventListener.h"
#include "core/events/MouseEvent.h"
#include "core/fetch/ImageResource.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/frame/UseCounter.h"
#include "core/frame/VisualViewport.h"
#include "core/html/HTMLBodyElement.h"
#include "core/html/HTMLContentElement.h"
#include "core/html/HTMLDivElement.h"
#include "core/html/HTMLHeadElement.h"
#include "core/html/HTMLHtmlElement.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/HTMLMetaElement.h"
#include "core/layout/LayoutObject.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/page/Page.h"
#include "platform/HostWindow.h"
#include "wtf/text/StringBuilder.h"
#include <limits>

using namespace std;

namespace {

// The base square size is set to 10 because it rounds nicely for both the
// minimum scale (0.1) and maximum scale (5.0).
const int kBaseCheckerSize = 10;

}  // namespace

namespace blink {

using namespace HTMLNames;

class ImageEventListener : public EventListener {
 public:
  static ImageEventListener* create(ImageDocument* document) {
    return new ImageEventListener(document);
  }
  static const ImageEventListener* cast(const EventListener* listener) {
    return listener->type() == ImageEventListenerType
               ? static_cast<const ImageEventListener*>(listener)
               : 0;
  }

  bool operator==(const EventListener& other) const override;

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->trace(m_doc);
    EventListener::trace(visitor);
  }

 private:
  ImageEventListener(ImageDocument* document)
      : EventListener(ImageEventListenerType), m_doc(document) {}

  virtual void handleEvent(ExecutionContext*, Event*);

  Member<ImageDocument> m_doc;
};

class ImageDocumentParser : public RawDataDocumentParser {
 public:
  static ImageDocumentParser* create(ImageDocument* document) {
    return new ImageDocumentParser(document);
  }

  ImageDocument* document() const {
    return toImageDocument(RawDataDocumentParser::document());
  }

 private:
  ImageDocumentParser(ImageDocument* document)
      : RawDataDocumentParser(document) {}

  void appendBytes(const char*, size_t) override;
  virtual void finish();
};

// --------

static float pageZoomFactor(const Document* document) {
  LocalFrame* frame = document->frame();
  return frame ? frame->pageZoomFactor() : 1;
}

static String imageTitle(const String& filename, const IntSize& size) {
  StringBuilder result;
  result.append(filename);
  result.append(" (");
  // FIXME: Localize numbers. Safari/OSX shows localized numbers with group
  // separaters. For example, "1,920x1,080".
  result.appendNumber(size.width());
  result.append(static_cast<UChar>(0xD7));  // U+00D7 (multiplication sign)
  result.appendNumber(size.height());
  result.append(')');
  return result.toString();
}

static LayoutSize cachedImageSize(HTMLImageElement* element) {
  DCHECK(element->cachedImage());
  return element->cachedImage()->imageSize(
      LayoutObject::shouldRespectImageOrientation(element->layoutObject()),
      1.0f);
}

void ImageDocumentParser::appendBytes(const char* data, size_t length) {
  if (!length)
    return;

  LocalFrame* frame = document()->frame();
  Settings* settings = frame->settings();
  if (!frame->loader().client()->allowImage(
          !settings || settings->imagesEnabled(), document()->url()))
    return;

  if (document()->cachedImage()) {
    RELEASE_ASSERT(length <= std::numeric_limits<unsigned>::max());
    // If decoding has already failed, there's no point in sending additional
    // data to the ImageResource.
    if (document()->cachedImage()->getStatus() != Resource::DecodeError)
      document()->cachedImage()->appendData(data, length);
  }

  if (!isDetached())
    document()->imageUpdated();
}

void ImageDocumentParser::finish() {
  if (!isStopped() && document()->imageElement() && document()->cachedImage()) {
    ImageResource* cachedImage = document()->cachedImage();
    DocumentLoader* loader = document()->loader();
    cachedImage->setResponse(loader->response());
    cachedImage->finish(loader->timing().responseEnd());

    // Report the natural image size in the page title, regardless of zoom
    // level.  At a zoom level of 1 the image is guaranteed to have an integer
    // size.
    IntSize size = flooredIntSize(cachedImageSize(document()->imageElement()));
    if (size.width()) {
      // Compute the title, we use the decoded filename of the resource, falling
      // back on the (decoded) hostname if there is no path.
      String fileName =
          decodeURLEscapeSequences(document()->url().lastPathComponent());
      if (fileName.isEmpty())
        fileName = document()->url().host();
      document()->setTitle(imageTitle(fileName, size));
      if (isDetached())
        return;
    }

    document()->imageUpdated();
    document()->imageLoaded();
  }

  if (!isDetached())
    document()->finishedParsing();
}

// --------

ImageDocument::ImageDocument(const DocumentInit& initializer)
    : HTMLDocument(initializer, ImageDocumentClass),
      m_divElement(nullptr),
      m_imageElement(nullptr),
      m_imageSizeIsKnown(false),
      m_didShrinkImage(false),
      m_shouldShrinkImage(shouldShrinkToFit()),
      m_imageIsLoaded(false),
      m_styleCheckerSize(0),
      m_styleMouseCursorMode(Default),
      m_shrinkToFitMode(frame()->settings()->viewportEnabled() ? Viewport
                                                               : Desktop) {
  setCompatibilityMode(QuirksMode);
  lockCompatibilityMode();
  UseCounter::count(*this, UseCounter::ImageDocument);
  if (!isInMainFrame())
    UseCounter::count(*this, UseCounter::ImageDocumentInFrame);
}

DocumentParser* ImageDocument::createParser() {
  return ImageDocumentParser::create(this);
}

void ImageDocument::createDocumentStructure() {
  HTMLHtmlElement* rootElement = HTMLHtmlElement::create(*this);
  appendChild(rootElement);
  rootElement->insertedByParser();

  if (isStopped())
    return;  // runScriptsAtDocumentElementAvailable can detach the frame.

  HTMLHeadElement* head = HTMLHeadElement::create(*this);
  HTMLMetaElement* meta = HTMLMetaElement::create(*this);
  meta->setAttribute(nameAttr, "viewport");
  meta->setAttribute(contentAttr, "width=device-width, minimum-scale=0.1");
  head->appendChild(meta);

  HTMLBodyElement* body = HTMLBodyElement::create(*this);

  if (shouldShrinkToFit()) {
    // Display the image prominently centered in the frame.
    body->setAttribute(styleAttr, "margin: 0px; background: #0e0e0e;");

    // See w3c example on how to center an element:
    // https://www.w3.org/Style/Examples/007/center.en.html
    m_divElement = HTMLDivElement::create(*this);
    m_divElement->setAttribute(styleAttr,
                               "display: flex;"
                               "flex-direction: column;"
                               "justify-content: center;"
                               "align-items: center;"
                               "min-height: min-content;"
                               "min-width: min-content;"
                               "height: 100%;"
                               "width: 100%;");
    HTMLContentElement* content = HTMLContentElement::create(*this);
    m_divElement->appendChild(content);

    ShadowRoot& shadowRoot = body->ensureUserAgentShadowRoot();
    shadowRoot.appendChild(m_divElement);
  } else {
    body->setAttribute(styleAttr, "margin: 0px;");
  }

  willInsertBody();

  m_imageElement = HTMLImageElement::create(*this);
  updateImageStyle();
  m_imageElement->setLoadingImageDocument();
  m_imageElement->setSrc(url().getString());
  body->appendChild(m_imageElement.get());
  if (loader() && m_imageElement->cachedImage())
    m_imageElement->cachedImage()->responseReceived(loader()->response(),
                                                    nullptr);

  if (shouldShrinkToFit()) {
    // Add event listeners
    EventListener* listener = ImageEventListener::create(this);
    if (LocalDOMWindow* domWindow = this->domWindow())
      domWindow->addEventListener(EventTypeNames::resize, listener, false);

    if (m_shrinkToFitMode == Desktop) {
      m_imageElement->addEventListener(EventTypeNames::click, listener, false);
    } else if (m_shrinkToFitMode == Viewport) {
      m_imageElement->addEventListener(EventTypeNames::touchend, listener,
                                       false);
      m_imageElement->addEventListener(EventTypeNames::touchcancel, listener,
                                       false);
    }
  }

  rootElement->appendChild(head);
  rootElement->appendChild(body);
}

float ImageDocument::scale() const {
  DCHECK_EQ(m_shrinkToFitMode, Desktop);
  if (!m_imageElement || m_imageElement->document() != this)
    return 1.0f;

  FrameView* view = frame()->view();
  if (!view)
    return 1.0f;

  DCHECK(m_imageElement->cachedImage());
  const float zoom = pageZoomFactor(this);
  LayoutSize imageSize = m_imageElement->cachedImage()->imageSize(
      LayoutObject::shouldRespectImageOrientation(
          m_imageElement->layoutObject()),
      zoom);

  // We want to pretend the viewport is larger when the user has zoomed the
  // page in (but not when the zoom is coming from device scale).
  const float manualZoom =
      zoom / view->getHostWindow()->windowToViewportScalar(1.f);
  float widthScale = view->width() * manualZoom / imageSize.width().toFloat();
  float heightScale =
      view->height() * manualZoom / imageSize.height().toFloat();

  return min(widthScale, heightScale);
}

void ImageDocument::resizeImageToFit() {
  DCHECK_EQ(m_shrinkToFitMode, Desktop);
  if (!m_imageElement || m_imageElement->document() != this)
    return;

  LayoutSize imageSize = cachedImageSize(m_imageElement);

  const float scale = this->scale();
  m_imageElement->setWidth(static_cast<int>(imageSize.width() * scale));
  m_imageElement->setHeight(static_cast<int>(imageSize.height() * scale));

  updateImageStyle();
}

void ImageDocument::imageClicked(int x, int y) {
  DCHECK_EQ(m_shrinkToFitMode, Desktop);

  if (!m_imageSizeIsKnown || imageFitsInWindow())
    return;

  m_shouldShrinkImage = !m_shouldShrinkImage;

  if (m_shouldShrinkImage) {
    windowSizeChanged();
  } else {
    // Adjust the coordinates to account for the fact that the image was
    // centered on the screen.
    float imageX = x - m_imageElement->offsetLeft();
    float imageY = y - m_imageElement->offsetTop();

    restoreImageSize();

    updateStyleAndLayout();

    double scale = this->scale();

    float scrollX =
        imageX / scale - static_cast<float>(frame()->view()->width()) / 2;
    float scrollY =
        imageY / scale - static_cast<float>(frame()->view()->height()) / 2;

    frame()->view()->layoutViewportScrollableArea()->setScrollOffset(
        ScrollOffset(scrollX, scrollY), ProgrammaticScroll);
  }
}

void ImageDocument::imageLoaded() {
  m_imageIsLoaded = true;

  if (shouldShrinkToFit()) {
    // The checkerboard background needs to be inserted.
    updateImageStyle();
  }
}

void ImageDocument::updateImageStyle() {
  StringBuilder imageStyle;
  imageStyle.append("-webkit-user-select: none;");

  if (shouldShrinkToFit()) {
    if (m_shrinkToFitMode == Viewport)
      imageStyle.append("max-width: 100%;");

    // Once the image has fully loaded, it is displayed atop a checkerboard to
    // show transparency more faithfully.  The pattern is generated via CSS.
    if (m_imageIsLoaded) {
      int newCheckerSize = kBaseCheckerSize;
      MouseCursorMode newCursorMode = Default;

      if (m_shrinkToFitMode == Viewport) {
        double scale;

        if (hasFinishedParsing()) {
          // To ensure the checker pattern is visible for large images, the
          // checker size is dynamically adjusted to account for how much the
          // page is currently being scaled.
          scale = frame()->host()->visualViewport().scale();
        } else {
          // The checker pattern is initialized based on how large the image is
          // relative to the viewport.
          int viewportWidth = frame()->host()->visualViewport().size().width();
          scale = viewportWidth / static_cast<double>(calculateDivWidth());
        }

        newCheckerSize = round(std::max(1.0, newCheckerSize / scale));
      } else {
        // In desktop mode, the user can click on the image to zoom in or out.
        DCHECK_EQ(m_shrinkToFitMode, Desktop);
        if (imageFitsInWindow()) {
          newCursorMode = Default;
        } else {
          newCursorMode = m_shouldShrinkImage ? ZoomIn : ZoomOut;
        }
      }

      // The only things that can differ between updates are checker size and
      // the type of cursor being displayed.
      if (newCheckerSize == m_styleCheckerSize &&
          newCursorMode == m_styleMouseCursorMode) {
        return;
      }
      m_styleCheckerSize = newCheckerSize;
      m_styleMouseCursorMode = newCursorMode;

      imageStyle.append("background-position: 0px 0px, ");
      imageStyle.append(AtomicString::number(m_styleCheckerSize));
      imageStyle.append("px ");
      imageStyle.append(AtomicString::number(m_styleCheckerSize));
      imageStyle.append("px;");

      int tileSize = m_styleCheckerSize * 2;
      imageStyle.append("background-size: ");
      imageStyle.append(AtomicString::number(tileSize));
      imageStyle.append("px ");
      imageStyle.append(AtomicString::number(tileSize));
      imageStyle.append("px;");

      imageStyle.append(
          "background-color: white;"
          "background-image:"
          "linear-gradient(45deg, #eee 25%, transparent 25%, transparent 75%, "
          "#eee 75%, #eee 100%),"
          "linear-gradient(45deg, #eee 25%, transparent 25%, transparent 75%, "
          "#eee 75%, #eee 100%);");

      if (m_shrinkToFitMode == Desktop) {
        if (m_styleMouseCursorMode == ZoomIn)
          imageStyle.append("cursor: zoom-in;");
        else if (m_styleMouseCursorMode == ZoomOut)
          imageStyle.append("cursor: zoom-out;");
      }
    }
  }

  m_imageElement->setAttribute(styleAttr, imageStyle.toAtomicString());
}

void ImageDocument::imageUpdated() {
  DCHECK(m_imageElement);

  if (m_imageSizeIsKnown)
    return;

  updateStyleAndLayoutTree();
  if (!m_imageElement->cachedImage() ||
      m_imageElement->cachedImage()
          ->imageSize(LayoutObject::shouldRespectImageOrientation(
                          m_imageElement->layoutObject()),
                      pageZoomFactor(this))
          .isEmpty())
    return;

  m_imageSizeIsKnown = true;

  if (shouldShrinkToFit()) {
    // Force resizing of the image
    windowSizeChanged();
  }
}

void ImageDocument::restoreImageSize() {
  DCHECK_EQ(m_shrinkToFitMode, Desktop);

  if (!m_imageElement || !m_imageSizeIsKnown ||
      m_imageElement->document() != this)
    return;

  DCHECK(m_imageElement->cachedImage());
  LayoutSize imageSize = cachedImageSize(m_imageElement);
  m_imageElement->setWidth(imageSize.width().toInt());
  m_imageElement->setHeight(imageSize.height().toInt());
  updateImageStyle();

  m_didShrinkImage = false;
}

bool ImageDocument::imageFitsInWindow() const {
  DCHECK_EQ(m_shrinkToFitMode, Desktop);
  return this->scale() >= 1;
}

int ImageDocument::calculateDivWidth() {
  // Zooming in and out of an image being displayed within a viewport is done
  // by changing the page scale factor of the page instead of changing the
  // size of the image.  The size of the image is set so that:
  // * Images wider than the viewport take the full width of the screen.
  // * Images taller than the viewport are initially aligned with the top of
  //   of the frame.
  // * Images smaller in either dimension are centered along that axis.
  LayoutSize imageSize = cachedImageSize(m_imageElement);
  int viewportWidth = frame()->host()->visualViewport().size().width();

  // For huge images, minimum-scale=0.1 is still too big on small screens.
  // Set the <div> width so that the image will shrink to fit the width of the
  // screen when the scale is minimum.
  int maxWidth = std::min(imageSize.width().toInt(), viewportWidth * 10);
  return std::max(viewportWidth, maxWidth);
}

void ImageDocument::windowSizeChanged() {
  if (!m_imageElement || !m_imageSizeIsKnown ||
      m_imageElement->document() != this)
    return;

  if (m_shrinkToFitMode == Viewport) {
    LayoutSize imageSize = cachedImageSize(m_imageElement);
    int divWidth = calculateDivWidth();
    m_divElement->setInlineStyleProperty(CSSPropertyWidth, divWidth,
                                         CSSPrimitiveValue::UnitType::Pixels);

    // Explicitly set the height of the <div> containing the <img> so that it
    // can display the full image without shrinking it, allowing a full-width
    // reading mode for normal-width-huge-height images.
    float viewportAspectRatio =
        frame()->host()->visualViewport().size().aspectRatio();
    int divHeight = std::max(imageSize.height().toInt(),
                             static_cast<int>(divWidth / viewportAspectRatio));
    m_divElement->setInlineStyleProperty(CSSPropertyHeight, divHeight,
                                         CSSPrimitiveValue::UnitType::Pixels);
    return;
  }

  bool fitsInWindow = imageFitsInWindow();

  // If the image has been explicitly zoomed in, restore the cursor if the image
  // fits and set it to a zoom out cursor if the image doesn't fit
  if (!m_shouldShrinkImage) {
    updateImageStyle();
    return;
  }

  if (m_didShrinkImage) {
    // If the window has been resized so that the image fits, restore the image
    // size otherwise update the restored image size.
    if (fitsInWindow)
      restoreImageSize();
    else
      resizeImageToFit();
  } else {
    // If the image isn't resized but needs to be, then resize it.
    if (!fitsInWindow) {
      resizeImageToFit();
      m_didShrinkImage = true;
    }
  }
}

ImageResource* ImageDocument::cachedImage() {
  if (!m_imageElement) {
    createDocumentStructure();
    if (isStopped()) {
      m_imageElement = nullptr;
      return nullptr;
    }
  }

  return m_imageElement->cachedImage();
}

bool ImageDocument::shouldShrinkToFit() const {
  // WebView automatically resizes to match the contents, causing an infinite
  // loop as the contents then resize to match the window. To prevent this,
  // disallow images from shrinking to fit for WebViews.
  bool isWrapContentWebView =
      page() ? page()->settings().forceZeroLayoutHeight() : false;
  return frame()->isMainFrame() && !isWrapContentWebView;
}

DEFINE_TRACE(ImageDocument) {
  visitor->trace(m_divElement);
  visitor->trace(m_imageElement);
  HTMLDocument::trace(visitor);
}

// --------

void ImageEventListener::handleEvent(ExecutionContext*, Event* event) {
  if (event->type() == EventTypeNames::resize) {
    m_doc->windowSizeChanged();
  } else if (event->type() == EventTypeNames::click && event->isMouseEvent()) {
    MouseEvent* mouseEvent = toMouseEvent(event);
    m_doc->imageClicked(mouseEvent->x(), mouseEvent->y());
  } else if ((event->type() == EventTypeNames::touchend ||
              event->type() == EventTypeNames::touchcancel) &&
             event->isTouchEvent()) {
    m_doc->updateImageStyle();
  }
}

bool ImageEventListener::operator==(const EventListener& listener) const {
  if (const ImageEventListener* imageEventListener =
          ImageEventListener::cast(&listener))
    return m_doc == imageEventListener->m_doc;
  return false;
}

}  // namespace blink

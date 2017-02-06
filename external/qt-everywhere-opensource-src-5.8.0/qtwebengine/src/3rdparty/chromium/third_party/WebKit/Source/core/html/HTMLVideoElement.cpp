/*
 * Copyright (C) 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/html/HTMLVideoElement.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/CSSPropertyNames.h"
#include "core/HTMLNames.h"
#include "core/dom/Attribute.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/Fullscreen.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/frame/ImageBitmap.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/Settings.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/imagebitmap/ImageBitmapOptions.h"
#include "core/layout/LayoutImage.h"
#include "core/layout/LayoutVideo.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/UserGestureIndicator.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/graphics/gpu/Extensions3DUtil.h"
#include "public/platform/WebCanvas.h"
#include <memory>

namespace blink {

using namespace HTMLNames;

inline HTMLVideoElement::HTMLVideoElement(Document& document)
    : HTMLMediaElement(videoTag, document)
{
    if (document.settings())
        m_defaultPosterURL = AtomicString(document.settings()->defaultVideoPosterURL());
}

HTMLVideoElement* HTMLVideoElement::create(Document& document)
{
    HTMLVideoElement* video = new HTMLVideoElement(document);
    video->ensureUserAgentShadowRoot();
    video->suspendIfNeeded();
    return video;
}

DEFINE_TRACE(HTMLVideoElement)
{
    visitor->trace(m_imageLoader);
    HTMLMediaElement::trace(visitor);
}

bool HTMLVideoElement::layoutObjectIsNeeded(const ComputedStyle& style)
{
    return HTMLElement::layoutObjectIsNeeded(style);
}

LayoutObject* HTMLVideoElement::createLayoutObject(const ComputedStyle&)
{
    return new LayoutVideo(this);
}

void HTMLVideoElement::attach(const AttachContext& context)
{
    HTMLMediaElement::attach(context);

    updateDisplayState();
    if (shouldDisplayPosterImage()) {
        if (!m_imageLoader)
            m_imageLoader = HTMLImageLoader::create(this);
        m_imageLoader->updateFromElement();
        if (layoutObject())
            toLayoutImage(layoutObject())->imageResource()->setImageResource(m_imageLoader->image());
    }
}

void HTMLVideoElement::collectStyleForPresentationAttribute(const QualifiedName& name, const AtomicString& value, MutableStylePropertySet* style)
{
    if (name == widthAttr)
        addHTMLLengthToStyle(style, CSSPropertyWidth, value);
    else if (name == heightAttr)
        addHTMLLengthToStyle(style, CSSPropertyHeight, value);
    else
        HTMLMediaElement::collectStyleForPresentationAttribute(name, value, style);
}

bool HTMLVideoElement::isPresentationAttribute(const QualifiedName& name) const
{
    if (name == widthAttr || name == heightAttr)
        return true;
    return HTMLMediaElement::isPresentationAttribute(name);
}

void HTMLVideoElement::parseAttribute(const QualifiedName& name, const AtomicString& oldValue, const AtomicString& value)
{
    if (name == posterAttr) {
        // In case the poster attribute is set after playback, don't update the
        // display state, post playback the correct state will be picked up.
        if (getDisplayMode() < Video || !hasAvailableVideoFrame()) {
            // Force a poster recalc by setting m_displayMode to Unknown directly before calling updateDisplayState.
            HTMLMediaElement::setDisplayMode(Unknown);
            updateDisplayState();
        }
        if (!posterImageURL().isEmpty()) {
            if (!m_imageLoader)
                m_imageLoader = HTMLImageLoader::create(this);
            m_imageLoader->updateFromElement(ImageLoader::UpdateIgnorePreviousError);
        } else {
            if (layoutObject())
                toLayoutImage(layoutObject())->imageResource()->setImageResource(0);
        }
        // Notify the player when the poster image URL changes.
        if (webMediaPlayer())
            webMediaPlayer()->setPoster(posterImageURL());
    } else {
        HTMLMediaElement::parseAttribute(name, oldValue, value);
    }
}

unsigned HTMLVideoElement::videoWidth() const
{
    if (!webMediaPlayer())
        return 0;
    return webMediaPlayer()->naturalSize().width;
}

unsigned HTMLVideoElement::videoHeight() const
{
    if (!webMediaPlayer())
        return 0;
    return webMediaPlayer()->naturalSize().height;
}

bool HTMLVideoElement::isURLAttribute(const Attribute& attribute) const
{
    return attribute.name() == posterAttr || HTMLMediaElement::isURLAttribute(attribute);
}

const AtomicString HTMLVideoElement::imageSourceURL() const
{
    const AtomicString& url = getAttribute(posterAttr);
    if (!stripLeadingAndTrailingHTMLSpaces(url).isEmpty())
        return url;
    return m_defaultPosterURL;
}

void HTMLVideoElement::setDisplayMode(DisplayMode mode)
{
    DisplayMode oldMode = getDisplayMode();
    KURL poster = posterImageURL();

    if (!poster.isEmpty()) {
        // We have a poster path, but only show it until the user triggers display by playing or seeking and the
        // media engine has something to display.
        // Don't show the poster if there is a seek operation or
        // the video has restarted because of loop attribute
        if (mode == Video && oldMode == Poster && !hasAvailableVideoFrame())
            return;
    }

    HTMLMediaElement::setDisplayMode(mode);

    if (layoutObject() && getDisplayMode() != oldMode)
        layoutObject()->updateFromElement();
}

void HTMLVideoElement::updateDisplayState()
{
    if (posterImageURL().isEmpty())
        setDisplayMode(Video);
    else if (getDisplayMode() < Poster)
        setDisplayMode(Poster);
}

void HTMLVideoElement::paintCurrentFrame(SkCanvas* canvas, const IntRect& destRect, const SkPaint* paint) const
{
    if (!webMediaPlayer())
        return;

    SkXfermode::Mode mode;
    if (!paint || !SkXfermode::AsMode(paint->getXfermode(), &mode))
        mode = SkXfermode::kSrcOver_Mode;

    // TODO(junov, foolip): crbug.com/456529 Pass the whole SkPaint instead of only alpha and xfermode
    webMediaPlayer()->paint(canvas, destRect, paint ? paint->getAlpha() : 0xFF, mode);
}

bool HTMLVideoElement::copyVideoTextureToPlatformTexture(gpu::gles2::GLES2Interface* gl, GLuint texture, GLenum internalFormat, GLenum type, bool premultiplyAlpha, bool flipY)
{
    if (!webMediaPlayer())
        return false;

    DCHECK(Extensions3DUtil::canUseCopyTextureCHROMIUM(GL_TEXTURE_2D, internalFormat, type, 0));
    return webMediaPlayer()->copyVideoTextureToPlatformTexture(gl, texture, internalFormat, type, premultiplyAlpha, flipY);
}

bool HTMLVideoElement::hasAvailableVideoFrame() const
{
    if (!webMediaPlayer())
        return false;

    return webMediaPlayer()->hasVideo() && webMediaPlayer()->getReadyState() >= WebMediaPlayer::ReadyStateHaveCurrentData;
}

void HTMLVideoElement::webkitEnterFullscreen()
{
    if (!isFullscreen())
        enterFullscreen();
}

void HTMLVideoElement::webkitExitFullscreen()
{
    if (isFullscreen())
        exitFullscreen();
}

bool HTMLVideoElement::webkitSupportsFullscreen()
{
    return Fullscreen::fullscreenEnabled(document());
}

bool HTMLVideoElement::webkitDisplayingFullscreen()
{
    return isFullscreen();
}

bool HTMLVideoElement::usesOverlayFullscreenVideo() const
{
    if (RuntimeEnabledFeatures::forceOverlayFullscreenVideoEnabled())
        return true;

    return webMediaPlayer() && webMediaPlayer()->supportsOverlayFullscreenVideo();
}

void HTMLVideoElement::didMoveToNewDocument(Document& oldDocument)
{
    if (m_imageLoader)
        m_imageLoader->elementDidMoveToNewDocument();
    HTMLMediaElement::didMoveToNewDocument(oldDocument);
}

unsigned HTMLVideoElement::webkitDecodedFrameCount() const
{
    if (!webMediaPlayer())
        return 0;

    return webMediaPlayer()->decodedFrameCount();
}

unsigned HTMLVideoElement::webkitDroppedFrameCount() const
{
    if (!webMediaPlayer())
        return 0;

    return webMediaPlayer()->droppedFrameCount();
}

KURL HTMLVideoElement::posterImageURL() const
{
    String url = stripLeadingAndTrailingHTMLSpaces(imageSourceURL());
    if (url.isEmpty())
        return KURL();
    return document().completeURL(url);
}

PassRefPtr<Image> HTMLVideoElement::getSourceImageForCanvas(SourceImageStatus* status, AccelerationHint, SnapshotReason, const FloatSize&) const
{
    if (!hasAvailableVideoFrame()) {
        *status = InvalidSourceImageStatus;
        return nullptr;
    }

    IntSize intrinsicSize(videoWidth(), videoHeight());
    // FIXME: Not sure if we dhould we be doing anything with the AccelerationHint argument here?
    std::unique_ptr<ImageBuffer> imageBuffer = ImageBuffer::create(intrinsicSize);
    if (!imageBuffer) {
        *status = InvalidSourceImageStatus;
        return nullptr;
    }

    paintCurrentFrame(imageBuffer->canvas(), IntRect(IntPoint(0, 0), intrinsicSize), nullptr);
    RefPtr<Image> snapshot = imageBuffer->newImageSnapshot();
    if (!snapshot) {
        *status = InvalidSourceImageStatus;
        return nullptr;
    }

    *status = NormalSourceImageStatus;
    return snapshot.release();
}

bool HTMLVideoElement::wouldTaintOrigin(SecurityOrigin* destinationSecurityOrigin) const
{
    return !isMediaDataCORSSameOrigin(destinationSecurityOrigin);
}

FloatSize HTMLVideoElement::elementSize(const FloatSize&) const
{
    return FloatSize(videoWidth(), videoHeight());
}

IntSize HTMLVideoElement::bitmapSourceSize() const
{
    return IntSize(videoWidth(), videoHeight());
}

ScriptPromise HTMLVideoElement::createImageBitmap(ScriptState* scriptState, EventTarget& eventTarget, int sx, int sy, int sw, int sh, const ImageBitmapOptions& options, ExceptionState& exceptionState)
{
    DCHECK(eventTarget.toLocalDOMWindow());
    if (getNetworkState() == HTMLMediaElement::NETWORK_EMPTY) {
        exceptionState.throwDOMException(InvalidStateError, "The provided element has not retrieved data.");
        return ScriptPromise();
    }
    if (getReadyState() <= HTMLMediaElement::HAVE_METADATA) {
        exceptionState.throwDOMException(InvalidStateError, "The provided element's player has no current data.");
        return ScriptPromise();
    }
    if (!sw || !sh) {
        exceptionState.throwDOMException(IndexSizeError, String::format("The source %s provided is 0.", sw ? "height" : "width"));
        return ScriptPromise();
    }
    return ImageBitmapSource::fulfillImageBitmap(scriptState, ImageBitmap::create(this, IntRect(sx, sy, sw, sh), eventTarget.toLocalDOMWindow()->document(), options));
}

} // namespace blink

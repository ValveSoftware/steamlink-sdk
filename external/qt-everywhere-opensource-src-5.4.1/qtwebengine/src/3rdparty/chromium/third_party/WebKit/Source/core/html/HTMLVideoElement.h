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

#ifndef HTMLVideoElement_h
#define HTMLVideoElement_h

#include "core/html/HTMLMediaElement.h"
#include "core/html/canvas/CanvasImageSource.h"

namespace blink {
class WebGraphicsContext3D;
}

namespace WebCore {

class ExceptionState;
class HTMLImageLoader;

class HTMLVideoElement FINAL : public HTMLMediaElement, public CanvasImageSource {
public:
    static PassRefPtrWillBeRawPtr<HTMLVideoElement> create(Document&);
    virtual void trace(Visitor*) OVERRIDE;

    unsigned videoWidth() const;
    unsigned videoHeight() const;

    // Fullscreen
    void webkitEnterFullscreen(ExceptionState&);
    void webkitExitFullscreen();
    bool webkitSupportsFullscreen();
    bool webkitDisplayingFullscreen();

    // Statistics
    unsigned webkitDecodedFrameCount() const;
    unsigned webkitDroppedFrameCount() const;

    // Used by canvas to gain raw pixel access
    void paintCurrentFrameInContext(GraphicsContext*, const IntRect&) const;

    // Used by WebGL to do GPU-GPU textures copy if possible.
    // See more details at MediaPlayer::copyVideoTextureToPlatformTexture() defined in Source/WebCore/platform/graphics/MediaPlayer.h.
    bool copyVideoTextureToPlatformTexture(blink::WebGraphicsContext3D*, Platform3DObject texture, GC3Dint level, GC3Denum type, GC3Denum internalFormat, bool premultiplyAlpha, bool flipY);

    bool shouldDisplayPosterImage() const { return displayMode() == Poster || displayMode() == PosterWaitingForVideo; }

    KURL posterImageURL() const;

    // FIXME: Remove this when WebMediaPlayerClientImpl::loadInternal does not depend on it.
    virtual KURL mediaPlayerPosterURL() OVERRIDE;

    // CanvasImageSource implementation
    virtual PassRefPtr<Image> getSourceImageForCanvas(SourceImageMode, SourceImageStatus*) const OVERRIDE;
    virtual bool isVideoElement() const OVERRIDE { return true; }
    virtual bool wouldTaintOrigin(SecurityOrigin*) const OVERRIDE;
    virtual FloatSize sourceSize() const OVERRIDE;
    virtual const KURL& sourceURL() const OVERRIDE { return currentSrc(); }

private:
    HTMLVideoElement(Document&);

    virtual bool rendererIsNeeded(const RenderStyle&) OVERRIDE;
    virtual RenderObject* createRenderer(RenderStyle*) OVERRIDE;
    virtual void attach(const AttachContext& = AttachContext()) OVERRIDE;
    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;
    virtual bool isPresentationAttribute(const QualifiedName&) const OVERRIDE;
    virtual void collectStyleForPresentationAttribute(const QualifiedName&, const AtomicString&, MutableStylePropertySet*) OVERRIDE;
    virtual bool hasVideo() const OVERRIDE { return webMediaPlayer() && webMediaPlayer()->hasVideo(); }
    bool supportsFullscreen() const;
    virtual bool isURLAttribute(const Attribute&) const OVERRIDE;
    virtual const AtomicString imageSourceURL() const OVERRIDE;

    bool hasAvailableVideoFrame() const;
    virtual void updateDisplayState() OVERRIDE;
    virtual void didMoveToNewDocument(Document& oldDocument) OVERRIDE;
    virtual void setDisplayMode(DisplayMode) OVERRIDE;

    OwnPtrWillBeMember<HTMLImageLoader> m_imageLoader;

    AtomicString m_defaultPosterURL;
};

} //namespace

#endif

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

#include "core/CoreExport.h"
#include "core/html/HTMLImageLoader.h"
#include "core/html/HTMLMediaElement.h"
#include "core/html/canvas/CanvasImageSource.h"
#include "core/imagebitmap/ImageBitmapSource.h"
#include "third_party/khronos/GLES2/gl2.h"

class SkPaint;

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
}

namespace blink {
class ExceptionState;
class GraphicsContext;
class ImageBitmapOptions;

class CORE_EXPORT HTMLVideoElement final : public HTMLMediaElement, public CanvasImageSource, public ImageBitmapSource {
    DEFINE_WRAPPERTYPEINFO();
public:
    static HTMLVideoElement* create(Document&);
    DECLARE_VIRTUAL_TRACE();

    unsigned videoWidth() const;
    unsigned videoHeight() const;

    // Fullscreen
    void webkitEnterFullscreen();
    void webkitExitFullscreen();
    bool webkitSupportsFullscreen();
    bool webkitDisplayingFullscreen();
    bool usesOverlayFullscreenVideo() const override;

    // Statistics
    unsigned webkitDecodedFrameCount() const;
    unsigned webkitDroppedFrameCount() const;

    // Used by canvas to gain raw pixel access
    void paintCurrentFrame(SkCanvas*, const IntRect&, const SkPaint*) const;

    // Used by WebGL to do GPU-GPU textures copy if possible.
    bool copyVideoTextureToPlatformTexture(gpu::gles2::GLES2Interface*, GLuint texture, GLenum internalFormat, GLenum type, bool premultiplyAlpha, bool flipY);

    bool shouldDisplayPosterImage() const { return getDisplayMode() == Poster; }

    bool hasAvailableVideoFrame() const;

    KURL posterImageURL() const override;

    // CanvasImageSource implementation
    PassRefPtr<Image> getSourceImageForCanvas(SourceImageStatus*, AccelerationHint, SnapshotReason, const FloatSize&) const override;
    bool isVideoElement() const override { return true; }
    bool wouldTaintOrigin(SecurityOrigin*) const override;
    FloatSize elementSize(const FloatSize&) const override;
    const KURL& sourceURL() const override { return currentSrc(); }
    bool isHTMLVideoElement() const override { return true; }
    int sourceWidth() override { return videoWidth(); }
    int sourceHeight() override { return videoHeight(); }

    // ImageBitmapSource implementation
    IntSize bitmapSourceSize() const override;
    ScriptPromise createImageBitmap(ScriptState*, EventTarget&, int sx, int sy, int sw, int sh, const ImageBitmapOptions&, ExceptionState&) override;

private:
    HTMLVideoElement(Document&);

    bool layoutObjectIsNeeded(const ComputedStyle&) override;
    LayoutObject* createLayoutObject(const ComputedStyle&) override;
    void attach(const AttachContext& = AttachContext()) override;
    void parseAttribute(const QualifiedName&, const AtomicString&, const AtomicString&) override;
    bool isPresentationAttribute(const QualifiedName&) const override;
    void collectStyleForPresentationAttribute(const QualifiedName&, const AtomicString&, MutableStylePropertySet*) override;
    bool isURLAttribute(const Attribute&) const override;
    const AtomicString imageSourceURL() const override;

    void updateDisplayState() override;
    void didMoveToNewDocument(Document& oldDocument) override;
    void setDisplayMode(DisplayMode) override;

    Member<HTMLImageLoader> m_imageLoader;

    AtomicString m_defaultPosterURL;
};

} // namespace blink

#endif // HTMLVideoElement_h

/*
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2008-2009 Torch Mobile, Inc.
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

#ifndef BitmapImage_h
#define BitmapImage_h

#include "platform/geometry/IntSize.h"
#include "platform/graphics/Color.h"
#include "platform/graphics/FrameData.h"
#include "platform/graphics/Image.h"
#include "platform/graphics/ImageAnimationPolicy.h"
#include "platform/graphics/ImageOrientation.h"
#include "platform/graphics/ImageSource.h"
#include "platform/image-decoders/ImageAnimation.h"
#include "wtf/Forward.h"
#include <memory>

namespace blink {

template <typename T> class Timer;

class PLATFORM_EXPORT BitmapImage final : public Image {
    friend class BitmapImageTest;
    friend class CrossfadeGeneratedImage;
    friend class GeneratedImage;
    friend class GradientGeneratedImage;
    friend class GraphicsContext;
public:
    static PassRefPtr<BitmapImage> create(ImageObserver* observer = 0)
    {
        return adoptRef(new BitmapImage(observer));
    }

    ~BitmapImage() override;

    bool isBitmapImage() const override { return true; }

    bool currentFrameHasSingleSecurityOrigin() const override;

    IntSize size() const override;
    IntSize sizeRespectingOrientation() const;
    bool getHotSpot(IntPoint&) const override;
    String filenameExtension() const override;
    bool dataChanged(bool allDataReceived) override;

    bool isAllDataReceived() const { return m_allDataReceived; }
    bool hasColorProfile() const;

    void resetAnimation() override;
    bool maybeAnimated() override;

    void setAnimationPolicy(ImageAnimationPolicy policy) override { m_animationPolicy = policy; }
    ImageAnimationPolicy animationPolicy() override { return m_animationPolicy; }
    void advanceTime(double deltaTimeInSeconds) override;

    PassRefPtr<SkImage> imageForCurrentFrame() override;
    PassRefPtr<Image> imageForDefaultFrame() override;

    bool currentFrameKnownToBeOpaque(MetadataMode = UseCurrentMetadata) override;
    bool currentFrameIsComplete() override;
    bool currentFrameIsLazyDecoded() override;

    ImageOrientation currentFrameOrientation();

    // Construct a BitmapImage with the given orientation.
    static PassRefPtr<BitmapImage> createWithOrientationForTesting(const SkBitmap&, ImageOrientation);
    // Advance the image animation by one frame.
    void advanceAnimationForTesting() override { internalAdvanceAnimation(false); }

private:
    enum RepetitionCountStatus {
      Unknown,    // We haven't checked the source's repetition count.
      Uncertain,  // We have a repetition count, but it might be wrong (some GIFs have a count after the image data, and will report "loop once" until all data has been decoded).
      Certain     // The repetition count is known to be correct.
    };

    BitmapImage(const SkBitmap &, ImageObserver* = 0);
    BitmapImage(ImageObserver* = 0);

    void draw(SkCanvas*, const SkPaint&, const FloatRect& dstRect, const FloatRect& srcRect, RespectImageOrientationEnum, ImageClampingMode) override;

    size_t currentFrame() const { return m_currentFrame; }
    size_t frameCount();

    PassRefPtr<SkImage> frameAtIndex(size_t);

    bool frameIsCompleteAtIndex(size_t);
    float frameDurationAtIndex(size_t);
    bool frameHasAlphaAtIndex(size_t);
    ImageOrientation frameOrientationAtIndex(size_t);

    PassRefPtr<SkImage> decodeAndCacheFrame(size_t index);
    void updateSize() const;

    // Returns the total number of bytes allocated for all framebuffers, i.e.
    // the sum of m_source.frameBytesAtIndex(...) for all frames.
    size_t totalFrameBytes();

    // Called to wipe out the entire frame buffer cache and tell the image
    // source to destroy everything; this is used when e.g. we want to free
    // some room in the image cache.
    void destroyDecodedData() override;

    // Notifies observers that the memory footprint has changed.
    void notifyMemoryChanged();

    // Whether or not size is available yet.
    bool isSizeAvailable();

    // Animation.
    // We start and stop animating lazily.  Animation starts when the image is
    // rendered, and automatically stops once no observer wants to render the
    // image.
    int repetitionCount(bool imageKnownToBeComplete);  // |imageKnownToBeComplete| should be set if the caller knows the entire image has been decoded.
    bool shouldAnimate();
    void startAnimation(CatchUpAnimation = CatchUp) override;
    void stopAnimation();
    void advanceAnimation(Timer<BitmapImage>*);
    // Advance the animation and let the next frame get scheduled without
    // catch-up logic. For large images with slow or heavily-loaded systems,
    // throwing away data as we go (see destroyDecodedData()) means we can spend
    // so much time re-decoding data that we are always behind. To prevent this,
    // we force the next animation to skip the catch up logic.
    void advanceAnimationWithoutCatchUp(Timer<BitmapImage>*);

    // Function that does the real work of advancing the animation.  When
    // skippingFrames is true, we're in the middle of a loop trying to skip over
    // a bunch of animation frames, so we should not do things like decode each
    // one or notify our observers.
    // Returns whether the animation was advanced.
    bool internalAdvanceAnimation(bool skippingFrames);

    ImageSource m_source;
    mutable IntSize m_size; // The size to use for the overall image (will just be the size of the first image).
    mutable IntSize m_sizeRespectingOrientation;

    size_t m_currentFrame; // The index of the current frame of animation.
    Vector<FrameData, 1> m_frames; // An array of the cached frames of the animation. We have to ref frames to pin them in the cache.

    RefPtr<SkImage> m_cachedFrame; // A cached copy of the most recently-accessed frame.
    size_t m_cachedFrameIndex; // Index of the frame that is cached.

    std::unique_ptr<Timer<BitmapImage>> m_frameTimer;
    int m_repetitionCount; // How many total animation loops we should do.  This will be cAnimationNone if this image type is incapable of animation.
    RepetitionCountStatus m_repetitionCountStatus;
    int m_repetitionsComplete;  // How many repetitions we've finished.
    double m_desiredFrameStartTime;  // The system time at which we hope to see the next call to startAnimation().

    size_t m_frameCount;

    ImageAnimationPolicy m_animationPolicy; // Whether or not we can play animation.

    bool m_animationFinished : 1; // Whether or not we've completed the entire animation.

    bool m_allDataReceived : 1; // Whether or not we've received all our data.
    mutable bool m_haveSize : 1; // Whether or not our |m_size| member variable has the final overall image size yet.
    bool m_sizeAvailable : 1; // Whether or not we can obtain the size of the first image frame yet from ImageIO.
    mutable bool m_haveFrameCount : 1;
};

DEFINE_IMAGE_TYPE_CASTS(BitmapImage);

} // namespace blink

#endif

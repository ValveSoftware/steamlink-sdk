/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "platform/image-decoders/webp/WEBPImageDecoder.h"

#include "platform/PlatformInstrumentation.h"
#include "platform/RuntimeEnabledFeatures.h"

#if USE(QCMSLIB)
#include "qcms.h"
#endif

#if CPU(BIG_ENDIAN) || CPU(MIDDLE_ENDIAN)
#error Blink assumes a little-endian target.
#endif

#if SK_B32_SHIFT // Output little-endian RGBA pixels (Android).
inline WEBP_CSP_MODE outputMode(bool hasAlpha) { return hasAlpha ? MODE_rgbA : MODE_RGBA; }
#else // Output little-endian BGRA pixels.
inline WEBP_CSP_MODE outputMode(bool hasAlpha) { return hasAlpha ? MODE_bgrA : MODE_BGRA; }
#endif

inline uint8_t blendChannel(uint8_t src, uint8_t srcA, uint8_t dst, uint8_t dstA, unsigned scale)
{
    unsigned blendUnscaled = src * srcA + dst * dstA;
    ASSERT(blendUnscaled < (1ULL << 32) / scale);
    return (blendUnscaled * scale) >> 24;
}

inline uint32_t blendSrcOverDstNonPremultiplied(uint32_t src, uint32_t dst)
{
    uint8_t srcA = SkGetPackedA32(src);
    if (srcA == 0)
        return dst;

    uint8_t dstA = SkGetPackedA32(dst);
    uint8_t dstFactorA = (dstA * SkAlpha255To256(255 - srcA)) >> 8;
    ASSERT(srcA + dstFactorA < (1U << 8));
    uint8_t blendA = srcA + dstFactorA;
    unsigned scale = (1UL << 24) / blendA;

    uint8_t blendR = blendChannel(SkGetPackedR32(src), srcA, SkGetPackedR32(dst), dstFactorA, scale);
    uint8_t blendG = blendChannel(SkGetPackedG32(src), srcA, SkGetPackedG32(dst), dstFactorA, scale);
    uint8_t blendB = blendChannel(SkGetPackedB32(src), srcA, SkGetPackedB32(dst), dstFactorA, scale);

    return SkPackARGB32NoCheck(blendA, blendR, blendG, blendB);
}

// Returns two point ranges (<left, width> pairs) at row 'canvasY', that belong to 'src' but not 'dst'.
// A point range is empty if the corresponding width is 0.
inline void findBlendRangeAtRow(const WebCore::IntRect& src, const WebCore::IntRect& dst, int canvasY, int& left1, int& width1, int& left2, int& width2)
{
    ASSERT_WITH_SECURITY_IMPLICATION(canvasY >= src.y() && canvasY < src.maxY());
    left1 = -1;
    width1 = 0;
    left2 = -1;
    width2 = 0;

    if (canvasY < dst.y() || canvasY >= dst.maxY() || src.x() >= dst.maxX() || src.maxX() <= dst.x()) {
        left1 = src.x();
        width1 = src.width();
        return;
    }

    if (src.x() < dst.x()) {
        left1 = src.x();
        width1 = dst.x() - src.x();
    }

    if (src.maxX() > dst.maxX()) {
        left2 = dst.maxX();
        width2 = src.maxX() - dst.maxX();
    }
}

void alphaBlendPremultiplied(WebCore::ImageFrame& src, WebCore::ImageFrame& dst, int canvasY, int left, int width)
{
    for (int x = 0; x < width; ++x) {
        int canvasX = left + x;
        WebCore::ImageFrame::PixelData& pixel = *src.getAddr(canvasX, canvasY);
        if (SkGetPackedA32(pixel) != 0xff) {
            WebCore::ImageFrame::PixelData prevPixel = *dst.getAddr(canvasX, canvasY);
            pixel = SkPMSrcOver(pixel, prevPixel);
        }
    }
}

void alphaBlendNonPremultiplied(WebCore::ImageFrame& src, WebCore::ImageFrame& dst, int canvasY, int left, int width)
{
    for (int x = 0; x < width; ++x) {
        int canvasX = left + x;
        WebCore::ImageFrame::PixelData& pixel = *src.getAddr(canvasX, canvasY);
        if (SkGetPackedA32(pixel) != 0xff) {
            WebCore::ImageFrame::PixelData prevPixel = *dst.getAddr(canvasX, canvasY);
            pixel = blendSrcOverDstNonPremultiplied(pixel, prevPixel);
        }
    }
}

namespace WebCore {

WEBPImageDecoder::WEBPImageDecoder(ImageSource::AlphaOption alphaOption,
    ImageSource::GammaAndColorProfileOption gammaAndColorProfileOption,
    size_t maxDecodedBytes)
    : ImageDecoder(alphaOption, gammaAndColorProfileOption, maxDecodedBytes)
    , m_decoder(0)
    , m_formatFlags(0)
    , m_frameBackgroundHasAlpha(false)
    , m_hasColorProfile(false)
#if USE(QCMSLIB)
    , m_haveReadProfile(false)
    , m_transform(0)
#endif
    , m_demux(0)
    , m_demuxState(WEBP_DEMUX_PARSING_HEADER)
    , m_haveAlreadyParsedThisData(false)
    , m_haveReadAnimationParameters(false)
    , m_repetitionCount(cAnimationLoopOnce)
    , m_decodedHeight(0)
{
    m_blendFunction = (alphaOption == ImageSource::AlphaPremultiplied) ? alphaBlendPremultiplied : alphaBlendNonPremultiplied;
}

WEBPImageDecoder::~WEBPImageDecoder()
{
    clear();
}

void WEBPImageDecoder::clear()
{
#if USE(QCMSLIB)
    if (m_transform)
        qcms_transform_release(m_transform);
    m_transform = 0;
#endif
    WebPDemuxDelete(m_demux);
    m_demux = 0;
    clearDecoder();
}

void WEBPImageDecoder::clearDecoder()
{
    WebPIDelete(m_decoder);
    m_decoder = 0;
    m_decodedHeight = 0;
    m_frameBackgroundHasAlpha = false;
}

bool WEBPImageDecoder::isSizeAvailable()
{
    if (!ImageDecoder::isSizeAvailable())
        updateDemuxer();

    return ImageDecoder::isSizeAvailable();
}

size_t WEBPImageDecoder::frameCount()
{
    if (!updateDemuxer())
        return 0;

    return m_frameBufferCache.size();
}

ImageFrame* WEBPImageDecoder::frameBufferAtIndex(size_t index)
{
    if (index >= frameCount())
        return 0;

    ImageFrame& frame = m_frameBufferCache[index];
    if (frame.status() == ImageFrame::FrameComplete)
        return &frame;

    Vector<size_t> framesToDecode;
    size_t frameToDecode = index;
    do {
        framesToDecode.append(frameToDecode);
        frameToDecode = m_frameBufferCache[frameToDecode].requiredPreviousFrameIndex();
    } while (frameToDecode != kNotFound && m_frameBufferCache[frameToDecode].status() != ImageFrame::FrameComplete);

    ASSERT(m_demux);
    for (size_t i = framesToDecode.size(); i > 0; --i) {
        size_t frameIndex = framesToDecode[i - 1];
        if ((m_formatFlags & ANIMATION_FLAG) && !initFrameBuffer(frameIndex))
            return 0;
        WebPIterator webpFrame;
        if (!WebPDemuxGetFrame(m_demux, frameIndex + 1, &webpFrame))
            return 0;
        PlatformInstrumentation::willDecodeImage("WEBP");
        decode(webpFrame.fragment.bytes, webpFrame.fragment.size, false, frameIndex);
        PlatformInstrumentation::didDecodeImage();
        WebPDemuxReleaseIterator(&webpFrame);

        if (failed())
            return 0;

        // We need more data to continue decoding.
        if (m_frameBufferCache[frameIndex].status() != ImageFrame::FrameComplete)
            break;
    }

    // It is also a fatal error if all data is received and we have decoded all
    // frames available but the file is truncated.
    if (index >= m_frameBufferCache.size() - 1 && isAllDataReceived() && m_demux && m_demuxState != WEBP_DEMUX_DONE)
        setFailed();

    frame.notifyBitmapIfPixelsChanged();
    return &frame;
}

void WEBPImageDecoder::setData(SharedBuffer* data, bool allDataReceived)
{
    if (failed())
        return;
    ImageDecoder::setData(data, allDataReceived);
    m_haveAlreadyParsedThisData = false;
}

int WEBPImageDecoder::repetitionCount() const
{
    return failed() ? cAnimationLoopOnce : m_repetitionCount;
}

bool WEBPImageDecoder::frameIsCompleteAtIndex(size_t index) const
{
    if (!m_demux || m_demuxState <= WEBP_DEMUX_PARSING_HEADER)
        return false;
    if (!(m_formatFlags & ANIMATION_FLAG))
        return ImageDecoder::frameIsCompleteAtIndex(index);
    bool frameIsLoadedAtIndex = index < m_frameBufferCache.size();
    return frameIsLoadedAtIndex;
}

float WEBPImageDecoder::frameDurationAtIndex(size_t index) const
{
    return index < m_frameBufferCache.size() ? m_frameBufferCache[index].duration() : 0;
}

bool WEBPImageDecoder::updateDemuxer()
{
    if (failed())
        return false;

    if (m_haveAlreadyParsedThisData)
        return true;

    m_haveAlreadyParsedThisData = true;

    const unsigned webpHeaderSize = 20;
    if (m_data->size() < webpHeaderSize)
        return false; // Wait for headers so that WebPDemuxPartial doesn't return null.

    WebPDemuxDelete(m_demux);
    WebPData inputData = { reinterpret_cast<const uint8_t*>(m_data->data()), m_data->size() };
    m_demux = WebPDemuxPartial(&inputData, &m_demuxState);
    if (!m_demux || (isAllDataReceived() && m_demuxState != WEBP_DEMUX_DONE))
        return setFailed();

    if (m_demuxState <= WEBP_DEMUX_PARSING_HEADER)
        return false; // Not enough data for parsing canvas width/height yet.

    bool hasAnimation = (m_formatFlags & ANIMATION_FLAG);
    if (!ImageDecoder::isSizeAvailable()) {
        m_formatFlags = WebPDemuxGetI(m_demux, WEBP_FF_FORMAT_FLAGS);
        hasAnimation = (m_formatFlags & ANIMATION_FLAG);
        if (!hasAnimation)
            m_repetitionCount = cAnimationNone;
        else
            m_formatFlags &= ~ICCP_FLAG; // FIXME: Implement ICC profile support for animated images.
#if USE(QCMSLIB)
        if ((m_formatFlags & ICCP_FLAG) && !ignoresGammaAndColorProfile())
            m_hasColorProfile = true;
#endif
        if (!setSize(WebPDemuxGetI(m_demux, WEBP_FF_CANVAS_WIDTH), WebPDemuxGetI(m_demux, WEBP_FF_CANVAS_HEIGHT)))
            return setFailed();
    }

    ASSERT(ImageDecoder::isSizeAvailable());
    const size_t newFrameCount = WebPDemuxGetI(m_demux, WEBP_FF_FRAME_COUNT);
    if (hasAnimation && !m_haveReadAnimationParameters && newFrameCount) {
        // As we have parsed at least one frame (even if partially),
        // we must already have parsed the animation properties.
        // This is because ANIM chunk always precedes ANMF chunks.
        m_repetitionCount = WebPDemuxGetI(m_demux, WEBP_FF_LOOP_COUNT);
        ASSERT(m_repetitionCount == (m_repetitionCount & 0xffff)); // Loop count is always <= 16 bits.
        // |m_repetitionCount| is the total number of animation cycles to show,
        // with 0 meaning "infinite". But ImageSource::repetitionCount()
        // returns -1 for "infinite", and 0 and up for "show the animation one
        // cycle more than this value". By subtracting one here, we convert
        // both finite and infinite cases correctly.
        --m_repetitionCount;
        m_haveReadAnimationParameters = true;
    }

    const size_t oldFrameCount = m_frameBufferCache.size();
    if (newFrameCount > oldFrameCount) {
        m_frameBufferCache.resize(newFrameCount);
        for (size_t i = oldFrameCount; i < newFrameCount; ++i) {
            m_frameBufferCache[i].setPremultiplyAlpha(m_premultiplyAlpha);
            if (!hasAnimation) {
                ASSERT(!i);
                m_frameBufferCache[i].setRequiredPreviousFrameIndex(kNotFound);
                continue;
            }
            WebPIterator animatedFrame;
            WebPDemuxGetFrame(m_demux, i + 1, &animatedFrame);
            ASSERT(animatedFrame.complete == 1);
            m_frameBufferCache[i].setDuration(animatedFrame.duration);
            m_frameBufferCache[i].setDisposalMethod(animatedFrame.dispose_method == WEBP_MUX_DISPOSE_BACKGROUND ? ImageFrame::DisposeOverwriteBgcolor : ImageFrame::DisposeKeep);
            m_frameBufferCache[i].setAlphaBlendSource(animatedFrame.blend_method == WEBP_MUX_BLEND ? ImageFrame::BlendAtopPreviousFrame : ImageFrame::BlendAtopBgcolor);
            IntRect frameRect(animatedFrame.x_offset, animatedFrame.y_offset, animatedFrame.width, animatedFrame.height);
            // Make sure the frameRect doesn't extend outside the buffer.
            if (frameRect.maxX() > size().width())
                frameRect.setWidth(size().width() - animatedFrame.x_offset);
            if (frameRect.maxY() > size().height())
                frameRect.setHeight(size().height() - animatedFrame.y_offset);
            m_frameBufferCache[i].setOriginalFrameRect(frameRect);
            m_frameBufferCache[i].setRequiredPreviousFrameIndex(findRequiredPreviousFrame(i, !animatedFrame.has_alpha));
            WebPDemuxReleaseIterator(&animatedFrame);
        }
    }

    return true;
}

bool WEBPImageDecoder::initFrameBuffer(size_t frameIndex)
{
    ImageFrame& buffer = m_frameBufferCache[frameIndex];
    if (buffer.status() != ImageFrame::FrameEmpty) // Already initialized.
        return true;

    const size_t requiredPreviousFrameIndex = buffer.requiredPreviousFrameIndex();
    if (requiredPreviousFrameIndex == kNotFound) {
        // This frame doesn't rely on any previous data.
        if (!buffer.setSize(size().width(), size().height()))
            return setFailed();
        m_frameBackgroundHasAlpha = !buffer.originalFrameRect().contains(IntRect(IntPoint(), size()));
    } else {
        const ImageFrame& prevBuffer = m_frameBufferCache[requiredPreviousFrameIndex];
        ASSERT(prevBuffer.status() == ImageFrame::FrameComplete);

        // Preserve the last frame as the starting state for this frame.
        if (!buffer.copyBitmapData(prevBuffer))
            return setFailed();

        if (prevBuffer.disposalMethod() == ImageFrame::DisposeOverwriteBgcolor) {
            // We want to clear the previous frame to transparent, without
            // affecting pixels in the image outside of the frame.
            const IntRect& prevRect = prevBuffer.originalFrameRect();
            ASSERT(!prevRect.contains(IntRect(IntPoint(), size())));
            buffer.zeroFillFrameRect(prevRect);
        }

        m_frameBackgroundHasAlpha = prevBuffer.hasAlpha() || (prevBuffer.disposalMethod() == ImageFrame::DisposeOverwriteBgcolor);
    }

    buffer.setStatus(ImageFrame::FramePartial);
    // The buffer is transparent outside the decoded area while the image is loading.
    // The correct value of 'hasAlpha' for the frame will be set when it is fully decoded.
    buffer.setHasAlpha(true);
    return true;
}

size_t WEBPImageDecoder::clearCacheExceptFrame(size_t clearExceptFrame)
{
    // If |clearExceptFrame| has status FrameComplete, we preserve that frame.
    // Otherwise, we preserve a previous frame with status FrameComplete whose data is required
    // to decode |clearExceptFrame|, either in initFrameBuffer() or ApplyPostProcessing().
    // All other frames can be cleared.
    while ((clearExceptFrame < m_frameBufferCache.size()) && (m_frameBufferCache[clearExceptFrame].status() != ImageFrame::FrameComplete))
        clearExceptFrame = m_frameBufferCache[clearExceptFrame].requiredPreviousFrameIndex();

    return ImageDecoder::clearCacheExceptFrame(clearExceptFrame);
}

void WEBPImageDecoder::clearFrameBuffer(size_t frameIndex)
{
    if (m_demux && m_demuxState >= WEBP_DEMUX_PARSED_HEADER && m_frameBufferCache[frameIndex].status() == ImageFrame::FramePartial) {
        // Clear the decoder state so that this partial frame can be decoded again when requested.
        clearDecoder();
    }
    ImageDecoder::clearFrameBuffer(frameIndex);
}

#if USE(QCMSLIB)

void WEBPImageDecoder::createColorTransform(const char* data, size_t size)
{
    if (m_transform)
        qcms_transform_release(m_transform);
    m_transform = 0;

    qcms_profile* deviceProfile = ImageDecoder::qcmsOutputDeviceProfile();
    if (!deviceProfile)
        return;
    qcms_profile* inputProfile = qcms_profile_from_memory(data, size);
    if (!inputProfile)
        return;

    // We currently only support color profiles for RGB profiled images.
    ASSERT(icSigRgbData == qcms_profile_get_color_space(inputProfile));
    // The input image pixels are RGBA format.
    qcms_data_type format = QCMS_DATA_RGBA_8;
    // FIXME: Don't force perceptual intent if the image profile contains an intent.
    m_transform = qcms_transform_create(inputProfile, format, deviceProfile, QCMS_DATA_RGBA_8, QCMS_INTENT_PERCEPTUAL);

    qcms_profile_release(inputProfile);
}

void WEBPImageDecoder::readColorProfile()
{
    WebPChunkIterator chunkIterator;
    if (!WebPDemuxGetChunk(m_demux, "ICCP", 1, &chunkIterator)) {
        WebPDemuxReleaseChunkIterator(&chunkIterator);
        return;
    }

    const char* profileData = reinterpret_cast<const char*>(chunkIterator.chunk.bytes);
    size_t profileSize = chunkIterator.chunk.size;

    // Only accept RGB color profiles from input class devices.
    bool ignoreProfile = false;
    if (profileSize < ImageDecoder::iccColorProfileHeaderLength)
        ignoreProfile = true;
    else if (!ImageDecoder::rgbColorProfile(profileData, profileSize))
        ignoreProfile = true;
    else if (!ImageDecoder::inputDeviceColorProfile(profileData, profileSize))
        ignoreProfile = true;

    if (!ignoreProfile)
        createColorTransform(profileData, profileSize);

    WebPDemuxReleaseChunkIterator(&chunkIterator);
}

#endif // USE(QCMSLIB)

void WEBPImageDecoder::applyPostProcessing(size_t frameIndex)
{
    ImageFrame& buffer = m_frameBufferCache[frameIndex];
    int width;
    int decodedHeight;
    if (!WebPIDecGetRGB(m_decoder, &decodedHeight, &width, 0, 0))
        return; // See also https://bugs.webkit.org/show_bug.cgi?id=74062
    if (decodedHeight <= 0)
        return;

    const IntRect& frameRect = buffer.originalFrameRect();
    ASSERT_WITH_SECURITY_IMPLICATION(width == frameRect.width());
    ASSERT_WITH_SECURITY_IMPLICATION(decodedHeight <= frameRect.height());
    const int left = frameRect.x();
    const int top = frameRect.y();

#if USE(QCMSLIB)
    if ((m_formatFlags & ICCP_FLAG) && !ignoresGammaAndColorProfile()) {
        if (!m_haveReadProfile) {
            readColorProfile();
            m_haveReadProfile = true;
        }
        for (int y = m_decodedHeight; y < decodedHeight; ++y) {
            const int canvasY = top + y;
            uint8_t* row = reinterpret_cast<uint8_t*>(buffer.getAddr(left, canvasY));
            if (qcms_transform* transform = colorTransform())
                qcms_transform_data_type(transform, row, row, width, QCMS_OUTPUT_RGBX);
            uint8_t* pixel = row;
            for (int x = 0; x < width; ++x, pixel += 4) {
                const int canvasX = left + x;
                buffer.setRGBA(canvasX, canvasY, pixel[0], pixel[1], pixel[2], pixel[3]);
            }
        }
    }
#endif // USE(QCMSLIB)

    // During the decoding of current frame, we may have set some pixels to be transparent (i.e. alpha < 255).
    // However, the value of each of these pixels should have been determined by blending it against the value
    // of that pixel in the previous frame if alpha blend source was 'BlendAtopPreviousFrame'. So, we correct these
    // pixels based on disposal method of the previous frame and the previous frame buffer.
    // FIXME: This could be avoided if libwebp decoder had an API that used the previous required frame
    // to do the alpha-blending by itself.
    if ((m_formatFlags & ANIMATION_FLAG) && frameIndex && buffer.alphaBlendSource() == ImageFrame::BlendAtopPreviousFrame && buffer.requiredPreviousFrameIndex() != kNotFound) {
        ImageFrame& prevBuffer = m_frameBufferCache[frameIndex - 1];
        ASSERT(prevBuffer.status() == ImageFrame::FrameComplete);
        ImageFrame::DisposalMethod prevDisposalMethod = prevBuffer.disposalMethod();
        if (prevDisposalMethod == ImageFrame::DisposeKeep) { // Blend transparent pixels with pixels in previous canvas.
            for (int y = m_decodedHeight; y < decodedHeight; ++y) {
                m_blendFunction(buffer, prevBuffer, top + y, left, width);
            }
        } else if (prevDisposalMethod == ImageFrame::DisposeOverwriteBgcolor) {
            const IntRect& prevRect = prevBuffer.originalFrameRect();
            // We need to blend a transparent pixel with its value just after initFrame() call. That is:
            //   * Blend with fully transparent pixel if it belongs to prevRect <-- This is a no-op.
            //   * Blend with the pixel in the previous canvas otherwise <-- Needs alpha-blending.
            for (int y = m_decodedHeight; y < decodedHeight; ++y) {
                int canvasY = top + y;
                int left1, width1, left2, width2;
                findBlendRangeAtRow(frameRect, prevRect, canvasY, left1, width1, left2, width2);
                if (width1 > 0)
                    m_blendFunction(buffer, prevBuffer, canvasY, left1, width1);
                if (width2 > 0)
                    m_blendFunction(buffer, prevBuffer, canvasY, left2, width2);
            }
        }
    }

    m_decodedHeight = decodedHeight;
    buffer.setPixelsChanged(true);
}

bool WEBPImageDecoder::decode(const uint8_t* dataBytes, size_t dataSize, bool onlySize, size_t frameIndex)
{
    if (failed())
        return false;

    if (!ImageDecoder::isSizeAvailable()) {
        static const size_t imageHeaderSize = 30;
        if (dataSize < imageHeaderSize)
            return false;
        int width, height;
        WebPBitstreamFeatures features;
        if (WebPGetFeatures(dataBytes, dataSize, &features) != VP8_STATUS_OK)
            return setFailed();
        width = features.width;
        height = features.height;
        m_formatFlags = features.has_alpha ? ALPHA_FLAG : 0;
        if (!setSize(width, height))
            return setFailed();
    }

    ASSERT(ImageDecoder::isSizeAvailable());
    if (onlySize)
        return true;

    ASSERT(m_frameBufferCache.size() > frameIndex);
    ImageFrame& buffer = m_frameBufferCache[frameIndex];
    ASSERT(buffer.status() != ImageFrame::FrameComplete);

    if (buffer.status() == ImageFrame::FrameEmpty) {
        if (!buffer.setSize(size().width(), size().height()))
            return setFailed();
        buffer.setStatus(ImageFrame::FramePartial);
        // The buffer is transparent outside the decoded area while the image is loading.
        // The correct value of 'hasAlpha' for the frame will be set when it is fully decoded.
        buffer.setHasAlpha(true);
        buffer.setOriginalFrameRect(IntRect(IntPoint(), size()));
    }

    const IntRect& frameRect = buffer.originalFrameRect();
    if (!m_decoder) {
        WEBP_CSP_MODE mode = outputMode(m_formatFlags & ALPHA_FLAG);
        if (!m_premultiplyAlpha)
            mode = outputMode(false);
#if USE(QCMSLIB)
        if ((m_formatFlags & ICCP_FLAG) && !ignoresGammaAndColorProfile())
            mode = MODE_RGBA; // Decode to RGBA for input to libqcms.
#endif
        WebPInitDecBuffer(&m_decoderBuffer);
        m_decoderBuffer.colorspace = mode;
        m_decoderBuffer.u.RGBA.stride = size().width() * sizeof(ImageFrame::PixelData);
        m_decoderBuffer.u.RGBA.size = m_decoderBuffer.u.RGBA.stride * frameRect.height();
        m_decoderBuffer.is_external_memory = 1;
        m_decoder = WebPINewDecoder(&m_decoderBuffer);
        if (!m_decoder)
            return setFailed();
    }

    m_decoderBuffer.u.RGBA.rgba = reinterpret_cast<uint8_t*>(buffer.getAddr(frameRect.x(), frameRect.y()));

    switch (WebPIUpdate(m_decoder, dataBytes, dataSize)) {
    case VP8_STATUS_OK:
        applyPostProcessing(frameIndex);
        buffer.setHasAlpha((m_formatFlags & ALPHA_FLAG) || m_frameBackgroundHasAlpha);
        buffer.setStatus(ImageFrame::FrameComplete);
        clearDecoder();
        return true;
    case VP8_STATUS_SUSPENDED:
        if (!isAllDataReceived() && !frameIsCompleteAtIndex(frameIndex)) {
            applyPostProcessing(frameIndex);
            return false;
        }
        // FALLTHROUGH
    default:
        clear();
        return setFailed();
    }
}

} // namespace WebCore

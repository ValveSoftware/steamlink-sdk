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

#include "platform/image-decoders/webp/WEBPImageDecoder.h"

#if CPU(BIG_ENDIAN) || CPU(MIDDLE_ENDIAN)
#error Blink assumes a little-endian target.
#endif

#if SK_B32_SHIFT  // Output little-endian RGBA pixels (Android).
inline WEBP_CSP_MODE outputMode(bool hasAlpha) {
  return hasAlpha ? MODE_rgbA : MODE_RGBA;
}
#else  // Output little-endian BGRA pixels.
inline WEBP_CSP_MODE outputMode(bool hasAlpha) {
  return hasAlpha ? MODE_bgrA : MODE_BGRA;
}
#endif

namespace {

// Returns two point ranges (<left, width> pairs) at row |canvasY| which belong
// to |src| but not |dst|. A range is empty if its width is 0.
inline void findBlendRangeAtRow(const blink::IntRect& src,
                                const blink::IntRect& dst,
                                int canvasY,
                                int& left1,
                                int& width1,
                                int& left2,
                                int& width2) {
  SECURITY_DCHECK(canvasY >= src.y() && canvasY < src.maxY());
  left1 = -1;
  width1 = 0;
  left2 = -1;
  width2 = 0;

  if (canvasY < dst.y() || canvasY >= dst.maxY() || src.x() >= dst.maxX() ||
      src.maxX() <= dst.x()) {
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

// alphaBlendPremultiplied and alphaBlendNonPremultiplied are separate methods,
// even though they only differ by one line. This is done so that the compiler
// can inline blendSrcOverDstPremultiplied() and blensSrcOverDstRaw() calls.
// For GIF images, this optimization reduces decoding time by 15% for 3MB
// images.
void alphaBlendPremultiplied(blink::ImageFrame& src,
                             blink::ImageFrame& dst,
                             int canvasY,
                             int left,
                             int width) {
  for (int x = 0; x < width; ++x) {
    int canvasX = left + x;
    blink::ImageFrame::PixelData* pixel = src.getAddr(canvasX, canvasY);
    if (SkGetPackedA32(*pixel) != 0xff) {
      blink::ImageFrame::PixelData prevPixel = *dst.getAddr(canvasX, canvasY);
      blink::ImageFrame::blendSrcOverDstPremultiplied(pixel, prevPixel);
    }
  }
}

void alphaBlendNonPremultiplied(blink::ImageFrame& src,
                                blink::ImageFrame& dst,
                                int canvasY,
                                int left,
                                int width) {
  for (int x = 0; x < width; ++x) {
    int canvasX = left + x;
    blink::ImageFrame::PixelData* pixel = src.getAddr(canvasX, canvasY);
    if (SkGetPackedA32(*pixel) != 0xff) {
      blink::ImageFrame::PixelData prevPixel = *dst.getAddr(canvasX, canvasY);
      blink::ImageFrame::blendSrcOverDstRaw(pixel, prevPixel);
    }
  }
}

}  // namespace

namespace blink {

WEBPImageDecoder::WEBPImageDecoder(AlphaOption alphaOption,
                                   ColorSpaceOption colorOptions,
                                   size_t maxDecodedBytes)
    : ImageDecoder(alphaOption, colorOptions, maxDecodedBytes),
      m_decoder(0),
      m_formatFlags(0),
      m_frameBackgroundHasAlpha(false),
      m_demux(0),
      m_demuxState(WEBP_DEMUX_PARSING_HEADER),
      m_haveAlreadyParsedThisData(false),
      m_repetitionCount(cAnimationLoopOnce),
      m_decodedHeight(0) {
  m_blendFunction = (alphaOption == AlphaPremultiplied)
                        ? alphaBlendPremultiplied
                        : alphaBlendNonPremultiplied;
}

WEBPImageDecoder::~WEBPImageDecoder() {
  clear();
}

void WEBPImageDecoder::clear() {
  WebPDemuxDelete(m_demux);
  m_demux = 0;
  m_consolidatedData.reset();
  clearDecoder();
}

void WEBPImageDecoder::clearDecoder() {
  WebPIDelete(m_decoder);
  m_decoder = 0;
  m_decodedHeight = 0;
  m_frameBackgroundHasAlpha = false;
}

void WEBPImageDecoder::onSetData(SegmentReader*) {
  m_haveAlreadyParsedThisData = false;
}

int WEBPImageDecoder::repetitionCount() const {
  return failed() ? cAnimationLoopOnce : m_repetitionCount;
}

bool WEBPImageDecoder::frameIsCompleteAtIndex(size_t index) const {
  if (!m_demux || m_demuxState <= WEBP_DEMUX_PARSING_HEADER)
    return false;
  if (!(m_formatFlags & ANIMATION_FLAG))
    return ImageDecoder::frameIsCompleteAtIndex(index);
  bool frameIsLoadedAtIndex = index < m_frameBufferCache.size();
  return frameIsLoadedAtIndex;
}

float WEBPImageDecoder::frameDurationAtIndex(size_t index) const {
  return index < m_frameBufferCache.size()
             ? m_frameBufferCache[index].duration()
             : 0;
}

bool WEBPImageDecoder::updateDemuxer() {
  if (failed())
    return false;

  if (m_haveAlreadyParsedThisData)
    return true;

  m_haveAlreadyParsedThisData = true;

  const unsigned webpHeaderSize = 30;
  if (m_data->size() < webpHeaderSize)
    return false;  // Await VP8X header so WebPDemuxPartial succeeds.

  WebPDemuxDelete(m_demux);
  m_consolidatedData = m_data->getAsSkData();
  WebPData inputData = {
      reinterpret_cast<const uint8_t*>(m_consolidatedData->data()),
      m_consolidatedData->size()};
  m_demux = WebPDemuxPartial(&inputData, &m_demuxState);
  if (!m_demux || (isAllDataReceived() && m_demuxState != WEBP_DEMUX_DONE)) {
    if (!m_demux)
      m_consolidatedData.reset();
    return setFailed();
  }

  ASSERT(m_demuxState > WEBP_DEMUX_PARSING_HEADER);
  if (!WebPDemuxGetI(m_demux, WEBP_FF_FRAME_COUNT))
    return false;  // Wait until the encoded image frame data arrives.

  if (!isDecodedSizeAvailable()) {
    int width = WebPDemuxGetI(m_demux, WEBP_FF_CANVAS_WIDTH);
    int height = WebPDemuxGetI(m_demux, WEBP_FF_CANVAS_HEIGHT);
    if (!setSize(width, height))
      return setFailed();

    m_formatFlags = WebPDemuxGetI(m_demux, WEBP_FF_FORMAT_FLAGS);
    if (!(m_formatFlags & ANIMATION_FLAG)) {
      m_repetitionCount = cAnimationNone;
    } else {
      // Since we have parsed at least one frame, even if partially,
      // the global animation (ANIM) properties have been read since
      // an ANIM chunk must precede the ANMF frame chunks.
      m_repetitionCount = WebPDemuxGetI(m_demux, WEBP_FF_LOOP_COUNT);
      // Repetition count is always <= 16 bits.
      ASSERT(m_repetitionCount == (m_repetitionCount & 0xffff));
      if (!m_repetitionCount)
        m_repetitionCount = cAnimationLoopInfinite;
      // FIXME: Implement ICC profile support for animated images.
      m_formatFlags &= ~ICCP_FLAG;
    }

    if ((m_formatFlags & ICCP_FLAG) && !ignoresColorSpace())
      readColorProfile();
  }

  ASSERT(isDecodedSizeAvailable());

  size_t frameCount = WebPDemuxGetI(m_demux, WEBP_FF_FRAME_COUNT);
  updateAggressivePurging(frameCount);

  return true;
}

bool WEBPImageDecoder::initFrameBuffer(size_t frameIndex) {
  ImageFrame& buffer = m_frameBufferCache[frameIndex];
  if (buffer.getStatus() != ImageFrame::FrameEmpty)  // Already initialized.
    return true;

  const size_t requiredPreviousFrameIndex = buffer.requiredPreviousFrameIndex();
  if (requiredPreviousFrameIndex == kNotFound) {
    // This frame doesn't rely on any previous data.
    if (!buffer.setSizeAndColorSpace(size().width(), size().height(),
                                     colorSpace()))
      return setFailed();
    m_frameBackgroundHasAlpha =
        !buffer.originalFrameRect().contains(IntRect(IntPoint(), size()));
  } else {
    ImageFrame& prevBuffer = m_frameBufferCache[requiredPreviousFrameIndex];
    ASSERT(prevBuffer.getStatus() == ImageFrame::FrameComplete);

    // Preserve the last frame as the starting state for this frame. We try
    // to reuse |prevBuffer| as starting state to avoid copying.
    // For BlendAtopPreviousFrame, both frames are required, so we can't
    // take over its image data using takeBitmapDataIfWritable.
    if ((buffer.getAlphaBlendSource() == ImageFrame::BlendAtopPreviousFrame ||
         !buffer.takeBitmapDataIfWritable(&prevBuffer)) &&
        !buffer.copyBitmapData(prevBuffer))
      return setFailed();

    if (prevBuffer.getDisposalMethod() == ImageFrame::DisposeOverwriteBgcolor) {
      // We want to clear the previous frame to transparent, without
      // affecting pixels in the image outside of the frame.
      const IntRect& prevRect = prevBuffer.originalFrameRect();
      ASSERT(!prevRect.contains(IntRect(IntPoint(), size())));
      buffer.zeroFillFrameRect(prevRect);
    }

    m_frameBackgroundHasAlpha =
        prevBuffer.hasAlpha() ||
        (prevBuffer.getDisposalMethod() == ImageFrame::DisposeOverwriteBgcolor);
  }

  buffer.setStatus(ImageFrame::FramePartial);
  // The buffer is transparent outside the decoded area while the image is
  // loading. The correct alpha value for the frame will be set when it is fully
  // decoded.
  buffer.setHasAlpha(true);
  return true;
}

size_t WEBPImageDecoder::clearCacheExceptFrame(size_t clearExceptFrame) {
  // Don't clear if there are no frames, or only one.
  if (m_frameBufferCache.size() <= 1)
    return 0;

  // If |clearExceptFrame| has status FrameComplete, we only preserve that
  // frame.  Otherwise, we *also* preserve the most recent previous frame with
  // status FrameComplete whose data will be required to decode
  // |clearExceptFrame|, either in initFrameBuffer() or ApplyPostProcessing().
  // This frame index is stored in |clearExceptFrame2|.  All other frames can
  // be cleared.
  size_t clearExceptFrame2 = kNotFound;
  if (clearExceptFrame < m_frameBufferCache.size() &&
      m_frameBufferCache[clearExceptFrame].getStatus() !=
          ImageFrame::FrameComplete) {
    clearExceptFrame2 =
        m_frameBufferCache[clearExceptFrame].requiredPreviousFrameIndex();
  }

  while ((clearExceptFrame2 < m_frameBufferCache.size()) &&
         (m_frameBufferCache[clearExceptFrame2].getStatus() !=
          ImageFrame::FrameComplete)) {
    clearExceptFrame2 =
        m_frameBufferCache[clearExceptFrame2].requiredPreviousFrameIndex();
  }

  return clearCacheExceptTwoFrames(clearExceptFrame, clearExceptFrame2);
}

void WEBPImageDecoder::clearFrameBuffer(size_t frameIndex) {
  if (m_demux && m_demuxState >= WEBP_DEMUX_PARSED_HEADER &&
      m_frameBufferCache[frameIndex].getStatus() == ImageFrame::FramePartial) {
    // Clear the decoder state so that this partial frame can be decoded again
    // when requested.
    clearDecoder();
  }
  ImageDecoder::clearFrameBuffer(frameIndex);
}

void WEBPImageDecoder::readColorProfile() {
  WebPChunkIterator chunkIterator;
  if (!WebPDemuxGetChunk(m_demux, "ICCP", 1, &chunkIterator)) {
    WebPDemuxReleaseChunkIterator(&chunkIterator);
    return;
  }

  const char* profileData =
      reinterpret_cast<const char*>(chunkIterator.chunk.bytes);
  size_t profileSize = chunkIterator.chunk.size;

  setColorProfileAndComputeTransform(profileData, profileSize);

  WebPDemuxReleaseChunkIterator(&chunkIterator);
}

void WEBPImageDecoder::applyPostProcessing(size_t frameIndex) {
  ImageFrame& buffer = m_frameBufferCache[frameIndex];
  int width;
  int decodedHeight;
  if (!WebPIDecGetRGB(m_decoder, &decodedHeight, &width, 0, 0))
    return;  // See also https://bugs.webkit.org/show_bug.cgi?id=74062
  if (decodedHeight <= 0)
    return;

  const IntRect& frameRect = buffer.originalFrameRect();
  SECURITY_DCHECK(width == frameRect.width());
  SECURITY_DCHECK(decodedHeight <= frameRect.height());
  const int left = frameRect.x();
  const int top = frameRect.y();

  // TODO (msarett):
  // Here we apply the color space transformation to the dst space.
  // It does not really make sense to transform to a gamma-encoded
  // space and then immediately after, perform a linear premultiply
  // and linear blending.  Can we find a way to perform the
  // premultiplication and blending in a linear space?
  SkColorSpaceXform* xform = colorTransform();
  if (xform) {
    const SkColorSpaceXform::ColorFormat srcFormat =
        SkColorSpaceXform::kBGRA_8888_ColorFormat;
    const SkColorSpaceXform::ColorFormat dstFormat =
        SkColorSpaceXform::kRGBA_8888_ColorFormat;
    for (int y = m_decodedHeight; y < decodedHeight; ++y) {
      const int canvasY = top + y;
      uint8_t* row = reinterpret_cast<uint8_t*>(buffer.getAddr(left, canvasY));
      xform->apply(dstFormat, row, srcFormat, row, width,
                   kUnpremul_SkAlphaType);

      uint8_t* pixel = row;
      for (int x = 0; x < width; ++x, pixel += 4) {
        const int canvasX = left + x;
        buffer.setRGBA(canvasX, canvasY, pixel[0], pixel[1], pixel[2],
                       pixel[3]);
      }
    }
  }

  // During the decoding of the current frame, we may have set some pixels to be
  // transparent (i.e. alpha < 255). If the alpha blend source was
  // 'BlendAtopPreviousFrame', the values of these pixels should be determined
  // by blending them against the pixels of the corresponding previous frame.
  // Compute the correct opaque values now.
  // FIXME: This could be avoided if libwebp decoder had an API that used the
  // previous required frame to do the alpha-blending by itself.
  if ((m_formatFlags & ANIMATION_FLAG) && frameIndex &&
      buffer.getAlphaBlendSource() == ImageFrame::BlendAtopPreviousFrame &&
      buffer.requiredPreviousFrameIndex() != kNotFound) {
    ImageFrame& prevBuffer = m_frameBufferCache[frameIndex - 1];
    ASSERT(prevBuffer.getStatus() == ImageFrame::FrameComplete);
    ImageFrame::DisposalMethod prevDisposalMethod =
        prevBuffer.getDisposalMethod();
    if (prevDisposalMethod == ImageFrame::DisposeKeep) {
      // Blend transparent pixels with pixels in previous canvas.
      for (int y = m_decodedHeight; y < decodedHeight; ++y) {
        m_blendFunction(buffer, prevBuffer, top + y, left, width);
      }
    } else if (prevDisposalMethod == ImageFrame::DisposeOverwriteBgcolor) {
      const IntRect& prevRect = prevBuffer.originalFrameRect();
      // We need to blend a transparent pixel with the starting value (from just
      // after the initFrame() call). If the pixel belongs to prevRect, the
      // starting value was fully transparent, so this is a no-op. Otherwise, we
      // need to blend against the pixel from the previous canvas.
      for (int y = m_decodedHeight; y < decodedHeight; ++y) {
        int canvasY = top + y;
        int left1, width1, left2, width2;
        findBlendRangeAtRow(frameRect, prevRect, canvasY, left1, width1, left2,
                            width2);
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

size_t WEBPImageDecoder::decodeFrameCount() {
  // If updateDemuxer() fails, return the existing number of frames.  This way
  // if we get halfway through the image before decoding fails, we won't
  // suddenly start reporting that the image has zero frames.
  return updateDemuxer() ? WebPDemuxGetI(m_demux, WEBP_FF_FRAME_COUNT)
                         : m_frameBufferCache.size();
}

void WEBPImageDecoder::initializeNewFrame(size_t index) {
  if (!(m_formatFlags & ANIMATION_FLAG)) {
    ASSERT(!index);
    return;
  }
  WebPIterator animatedFrame;
  WebPDemuxGetFrame(m_demux, index + 1, &animatedFrame);
  ASSERT(animatedFrame.complete == 1);
  ImageFrame* buffer = &m_frameBufferCache[index];
  IntRect frameRect(animatedFrame.x_offset, animatedFrame.y_offset,
                    animatedFrame.width, animatedFrame.height);
  buffer->setOriginalFrameRect(
      intersection(frameRect, IntRect(IntPoint(), size())));
  buffer->setDuration(animatedFrame.duration);
  buffer->setDisposalMethod(animatedFrame.dispose_method ==
                                    WEBP_MUX_DISPOSE_BACKGROUND
                                ? ImageFrame::DisposeOverwriteBgcolor
                                : ImageFrame::DisposeKeep);
  buffer->setAlphaBlendSource(animatedFrame.blend_method == WEBP_MUX_BLEND
                                  ? ImageFrame::BlendAtopPreviousFrame
                                  : ImageFrame::BlendAtopBgcolor);
  buffer->setRequiredPreviousFrameIndex(
      findRequiredPreviousFrame(index, !animatedFrame.has_alpha));
  WebPDemuxReleaseIterator(&animatedFrame);
}

void WEBPImageDecoder::decode(size_t index) {
  if (failed())
    return;

  Vector<size_t> framesToDecode = findFramesToDecode(index);

  ASSERT(m_demux);
  for (auto i = framesToDecode.rbegin(); i != framesToDecode.rend(); ++i) {
    if ((m_formatFlags & ANIMATION_FLAG) && !initFrameBuffer(*i))
      return;
    WebPIterator webpFrame;
    if (!WebPDemuxGetFrame(m_demux, *i + 1, &webpFrame)) {
      setFailed();
    } else {
      decodeSingleFrame(webpFrame.fragment.bytes, webpFrame.fragment.size, *i);
      WebPDemuxReleaseIterator(&webpFrame);
    }
    if (failed())
      return;

    // If this returns false, we need more data to continue decoding.
    if (!postDecodeProcessing(*i))
      break;
  }

  // It is also a fatal error if all data is received and we have decoded all
  // frames available but the file is truncated.
  if (index >= m_frameBufferCache.size() - 1 && isAllDataReceived() &&
      m_demux && m_demuxState != WEBP_DEMUX_DONE)
    setFailed();
}

bool WEBPImageDecoder::decodeSingleFrame(const uint8_t* dataBytes,
                                         size_t dataSize,
                                         size_t frameIndex) {
  if (failed())
    return false;

  ASSERT(isDecodedSizeAvailable());

  ASSERT(m_frameBufferCache.size() > frameIndex);
  ImageFrame& buffer = m_frameBufferCache[frameIndex];
  ASSERT(buffer.getStatus() != ImageFrame::FrameComplete);

  if (buffer.getStatus() == ImageFrame::FrameEmpty) {
    if (!buffer.setSizeAndColorSpace(size().width(), size().height(),
                                     colorSpace()))
      return setFailed();
    buffer.setStatus(ImageFrame::FramePartial);
    // The buffer is transparent outside the decoded area while the image is
    // loading. The correct alpha value for the frame will be set when it is
    // fully decoded.
    buffer.setHasAlpha(true);
    buffer.setOriginalFrameRect(IntRect(IntPoint(), size()));
  }

  const IntRect& frameRect = buffer.originalFrameRect();
  if (!m_decoder) {
    WEBP_CSP_MODE mode = outputMode(m_formatFlags & ALPHA_FLAG);
    if (!m_premultiplyAlpha)
      mode = outputMode(false);
    if (colorTransform()) {
      // Swizzling between RGBA and BGRA is zero cost in a color transform.
      // So when we have a color transform, we should decode to whatever is
      // easiest for libwebp, and then let the color transform swizzle if
      // necessary.
      // Lossy webp is encoded as YUV (so RGBA and BGRA are the same cost).
      // Lossless webp is encoded as BGRA. This means decoding to BGRA is
      // either faster or the same cost as RGBA.
      mode = MODE_BGRA;
    }
    WebPInitDecBuffer(&m_decoderBuffer);
    m_decoderBuffer.colorspace = mode;
    m_decoderBuffer.u.RGBA.stride =
        size().width() * sizeof(ImageFrame::PixelData);
    m_decoderBuffer.u.RGBA.size =
        m_decoderBuffer.u.RGBA.stride * frameRect.height();
    m_decoderBuffer.is_external_memory = 1;
    m_decoder = WebPINewDecoder(&m_decoderBuffer);
    if (!m_decoder)
      return setFailed();
  }

  m_decoderBuffer.u.RGBA.rgba =
      reinterpret_cast<uint8_t*>(buffer.getAddr(frameRect.x(), frameRect.y()));

  switch (WebPIUpdate(m_decoder, dataBytes, dataSize)) {
    case VP8_STATUS_OK:
      applyPostProcessing(frameIndex);
      buffer.setHasAlpha((m_formatFlags & ALPHA_FLAG) ||
                         m_frameBackgroundHasAlpha);
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

}  // namespace blink

/*
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#include "platform/image-decoders/ImageDecoder.h"

#include "platform/PlatformInstrumentation.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/BitmapImageMetrics.h"
#include "platform/image-decoders/FastSharedBufferReader.h"
#include "platform/image-decoders/bmp/BMPImageDecoder.h"
#include "platform/image-decoders/gif/GIFImageDecoder.h"
#include "platform/image-decoders/ico/ICOImageDecoder.h"
#include "platform/image-decoders/jpeg/JPEGImageDecoder.h"
#include "platform/image-decoders/png/PNGImageDecoder.h"
#include "platform/image-decoders/webp/WEBPImageDecoder.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

inline bool matchesJPEGSignature(const char* contents) {
  return !memcmp(contents, "\xFF\xD8\xFF", 3);
}

inline bool matchesPNGSignature(const char* contents) {
  return !memcmp(contents, "\x89PNG\r\n\x1A\n", 8);
}

inline bool matchesGIFSignature(const char* contents) {
  return !memcmp(contents, "GIF87a", 6) || !memcmp(contents, "GIF89a", 6);
}

inline bool matchesWebPSignature(const char* contents) {
  return !memcmp(contents, "RIFF", 4) && !memcmp(contents + 8, "WEBPVP", 6);
}

inline bool matchesICOSignature(const char* contents) {
  return !memcmp(contents, "\x00\x00\x01\x00", 4);
}

inline bool matchesCURSignature(const char* contents) {
  return !memcmp(contents, "\x00\x00\x02\x00", 4);
}

inline bool matchesBMPSignature(const char* contents) {
  return !memcmp(contents, "BM", 2);
}

// This needs to be updated if we ever add a matches*Signature() which requires
// more characters.
static constexpr size_t kLongestSignatureLength = sizeof("RIFF????WEBPVP") - 1;

std::unique_ptr<ImageDecoder> ImageDecoder::create(
    PassRefPtr<SegmentReader> passData,
    bool dataComplete,
    AlphaOption alphaOption,
    ColorSpaceOption colorOptions) {
  RefPtr<SegmentReader> data = passData;

  // We need at least kLongestSignatureLength bytes to run the signature
  // matcher.
  if (data->size() < kLongestSignatureLength)
    return nullptr;

  const size_t maxDecodedBytes =
      Platform::current() ? Platform::current()->maxDecodedImageBytes()
                          : noDecodedImageByteLimit;

  // Access the first kLongestSignatureLength chars to sniff the signature.
  // (note: FastSharedBufferReader only makes a copy if the bytes are segmented)
  char buffer[kLongestSignatureLength];
  const FastSharedBufferReader fastReader(data);
  const ImageDecoder::SniffResult sniffResult = determineImageType(
      fastReader.getConsecutiveData(0, kLongestSignatureLength, buffer),
      kLongestSignatureLength);

  std::unique_ptr<ImageDecoder> decoder;
  switch (sniffResult) {
    case SniffResult::JPEG:
      decoder.reset(
          new JPEGImageDecoder(alphaOption, colorOptions, maxDecodedBytes));
      break;
    case SniffResult::PNG:
      decoder.reset(
          new PNGImageDecoder(alphaOption, colorOptions, maxDecodedBytes));
      break;
    case SniffResult::GIF:
      decoder.reset(
          new GIFImageDecoder(alphaOption, colorOptions, maxDecodedBytes));
      break;
    case SniffResult::WEBP:
      decoder.reset(
          new WEBPImageDecoder(alphaOption, colorOptions, maxDecodedBytes));
      break;
    case SniffResult::ICO:
      decoder.reset(
          new ICOImageDecoder(alphaOption, colorOptions, maxDecodedBytes));
      break;
    case SniffResult::BMP:
      decoder.reset(
          new BMPImageDecoder(alphaOption, colorOptions, maxDecodedBytes));
      break;
    case SniffResult::Invalid:
      break;
  }

  if (decoder)
    decoder->setData(data.release(), dataComplete);

  return decoder;
}

bool ImageDecoder::hasSufficientDataToSniffImageType(const SharedBuffer& data) {
  return data.size() >= kLongestSignatureLength;
}

ImageDecoder::SniffResult ImageDecoder::determineImageType(const char* contents,
                                                           size_t length) {
  DCHECK_GE(length, kLongestSignatureLength);

  if (matchesJPEGSignature(contents))
    return SniffResult::JPEG;
  if (matchesPNGSignature(contents))
    return SniffResult::PNG;
  if (matchesGIFSignature(contents))
    return SniffResult::GIF;
  if (matchesWebPSignature(contents))
    return SniffResult::WEBP;
  if (matchesICOSignature(contents) || matchesCURSignature(contents))
    return SniffResult::ICO;
  if (matchesBMPSignature(contents))
    return SniffResult::BMP;
  return SniffResult::Invalid;
}

size_t ImageDecoder::frameCount() {
  const size_t oldSize = m_frameBufferCache.size();
  const size_t newSize = decodeFrameCount();
  if (oldSize != newSize) {
    m_frameBufferCache.resize(newSize);
    for (size_t i = oldSize; i < newSize; ++i) {
      m_frameBufferCache[i].setPremultiplyAlpha(m_premultiplyAlpha);
      initializeNewFrame(i);
    }
  }
  return newSize;
}

ImageFrame* ImageDecoder::frameBufferAtIndex(size_t index) {
  if (index >= frameCount())
    return 0;

  ImageFrame* frame = &m_frameBufferCache[index];
  if (frame->getStatus() != ImageFrame::FrameComplete) {
    PlatformInstrumentation::willDecodeImage(filenameExtension());
    decode(index);
    PlatformInstrumentation::didDecodeImage();
  }

  if (!m_hasHistogrammedColorSpace) {
    BitmapImageMetrics::countImageGamma(m_embeddedColorSpace.get());
    m_hasHistogrammedColorSpace = true;
  }

  frame->notifyBitmapIfPixelsChanged();
  return frame;
}

bool ImageDecoder::frameHasAlphaAtIndex(size_t index) const {
  return !frameIsCompleteAtIndex(index) || m_frameBufferCache[index].hasAlpha();
}

bool ImageDecoder::frameIsCompleteAtIndex(size_t index) const {
  return (index < m_frameBufferCache.size()) &&
         (m_frameBufferCache[index].getStatus() == ImageFrame::FrameComplete);
}

size_t ImageDecoder::frameBytesAtIndex(size_t index) const {
  if (index >= m_frameBufferCache.size() ||
      m_frameBufferCache[index].getStatus() == ImageFrame::FrameEmpty)
    return 0;

  struct ImageSize {
    explicit ImageSize(IntSize size) {
      area = static_cast<uint64_t>(size.width()) * size.height();
    }

    uint64_t area;
  };

  return ImageSize(frameSizeAtIndex(index)).area *
         sizeof(ImageFrame::PixelData);
}

size_t ImageDecoder::clearCacheExceptFrame(size_t clearExceptFrame) {
  // Don't clear if there are no frames or only one frame.
  if (m_frameBufferCache.size() <= 1)
    return 0;

  return clearCacheExceptTwoFrames(clearExceptFrame, kNotFound);
}

size_t ImageDecoder::clearCacheExceptTwoFrames(size_t clearExceptFrame1,
                                               size_t clearExceptFrame2) {
  size_t frameBytesCleared = 0;
  for (size_t i = 0; i < m_frameBufferCache.size(); ++i) {
    if (m_frameBufferCache[i].getStatus() != ImageFrame::FrameEmpty &&
        i != clearExceptFrame1 && i != clearExceptFrame2) {
      frameBytesCleared += frameBytesAtIndex(i);
      clearFrameBuffer(i);
    }
  }
  return frameBytesCleared;
}

void ImageDecoder::clearFrameBuffer(size_t frameIndex) {
  m_frameBufferCache[frameIndex].clearPixelData();
}

Vector<size_t> ImageDecoder::findFramesToDecode(size_t index) const {
  DCHECK(index < m_frameBufferCache.size());

  Vector<size_t> framesToDecode;
  do {
    framesToDecode.append(index);
    index = m_frameBufferCache[index].requiredPreviousFrameIndex();
  } while (index != kNotFound &&
           m_frameBufferCache[index].getStatus() != ImageFrame::FrameComplete);
  return framesToDecode;
}

bool ImageDecoder::postDecodeProcessing(size_t index) {
  DCHECK(index < m_frameBufferCache.size());

  if (m_frameBufferCache[index].getStatus() != ImageFrame::FrameComplete)
    return false;

  if (m_purgeAggressively)
    clearCacheExceptFrame(index);

  return true;
}

void ImageDecoder::updateAggressivePurging(size_t index) {
  if (m_purgeAggressively)
    return;

  // We don't want to cache so much that we cause a memory issue.
  //
  // If we used a LRU cache we would fill it and then on next animation loop
  // we would need to decode all the frames again -- the LRU would give no
  // benefit and would consume more memory.
  // So instead, simply purge unused frames if caching all of the frames of
  // the image would use more memory than the image decoder is allowed
  // (m_maxDecodedBytes) or would overflow 32 bits..
  //
  // As we decode we will learn the total number of frames, and thus total
  // possible image memory used.

  const uint64_t frameArea = decodedSize().area();
  const uint64_t frameMemoryUsage = frameArea * 4;  // 4 bytes per pixel
  if (frameMemoryUsage / 4 != frameArea) {          // overflow occurred
    m_purgeAggressively = true;
    return;
  }

  const uint64_t totalMemoryUsage = frameMemoryUsage * index;
  if (totalMemoryUsage / frameMemoryUsage != index) {  // overflow occurred
    m_purgeAggressively = true;
    return;
  }

  if (totalMemoryUsage > m_maxDecodedBytes) {
    m_purgeAggressively = true;
  }
}

size_t ImageDecoder::findRequiredPreviousFrame(size_t frameIndex,
                                               bool frameRectIsOpaque) {
  ASSERT(frameIndex <= m_frameBufferCache.size());
  if (!frameIndex) {
    // The first frame doesn't rely on any previous data.
    return kNotFound;
  }

  const ImageFrame* currBuffer = &m_frameBufferCache[frameIndex];
  if ((frameRectIsOpaque ||
       currBuffer->getAlphaBlendSource() == ImageFrame::BlendAtopBgcolor) &&
      currBuffer->originalFrameRect().contains(IntRect(IntPoint(), size())))
    return kNotFound;

  // The starting state for this frame depends on the previous frame's
  // disposal method.
  size_t prevFrame = frameIndex - 1;
  const ImageFrame* prevBuffer = &m_frameBufferCache[prevFrame];

  switch (prevBuffer->getDisposalMethod()) {
    case ImageFrame::DisposeNotSpecified:
    case ImageFrame::DisposeKeep:
      // prevFrame will be used as the starting state for this frame.
      // FIXME: Be even smarter by checking the frame sizes and/or
      // alpha-containing regions.
      return prevFrame;
    case ImageFrame::DisposeOverwritePrevious:
      // Frames that use the DisposeOverwritePrevious method are effectively
      // no-ops in terms of changing the starting state of a frame compared to
      // the starting state of the previous frame, so skip over them and
      // return the required previous frame of it.
      return prevBuffer->requiredPreviousFrameIndex();
    case ImageFrame::DisposeOverwriteBgcolor:
      // If the previous frame fills the whole image, then the current frame
      // can be decoded alone. Likewise, if the previous frame could be
      // decoded without reference to any prior frame, the starting state for
      // this frame is a blank frame, so it can again be decoded alone.
      // Otherwise, the previous frame contributes to this frame.
      return (prevBuffer->originalFrameRect().contains(
                  IntRect(IntPoint(), size())) ||
              (prevBuffer->requiredPreviousFrameIndex() == kNotFound))
                 ? kNotFound
                 : prevFrame;
    default:
      ASSERT_NOT_REACHED();
      return kNotFound;
  }
}

ImagePlanes::ImagePlanes() {
  for (int i = 0; i < 3; ++i) {
    m_planes[i] = 0;
    m_rowBytes[i] = 0;
  }
}

ImagePlanes::ImagePlanes(void* planes[3], const size_t rowBytes[3]) {
  for (int i = 0; i < 3; ++i) {
    m_planes[i] = planes[i];
    m_rowBytes[i] = rowBytes[i];
  }
}

void* ImagePlanes::plane(int i) {
  ASSERT((i >= 0) && i < 3);
  return m_planes[i];
}

size_t ImagePlanes::rowBytes(int i) const {
  ASSERT((i >= 0) && i < 3);
  return m_rowBytes[i];
}

namespace {

// The output device color space is global and shared across multiple threads.
SpinLock gTargetColorSpaceLock;
SkColorSpace* gTargetColorSpace = nullptr;

}  // namespace

// static
void ImageDecoder::setTargetColorProfile(const WebVector<char>& profile) {
  if (profile.isEmpty())
    return;

  // Take a lock around initializing and accessing the global device color
  // profile.
  SpinLock::Guard guard(gTargetColorSpaceLock);

  // Layout tests expect that only the first call will take effect.
  if (gTargetColorSpace)
    return;

  gTargetColorSpace =
      SkColorSpace::MakeICC(profile.data(), profile.size()).release();

  // UMA statistics.
  BitmapImageMetrics::countOutputGamma(gTargetColorSpace);
}

sk_sp<SkColorSpace> ImageDecoder::colorSpace() const {
  // TODO(ccameron): This should always return a non-null SkColorSpace. This is
  // disabled for now because specifying a non-renderable color space results in
  // errors.
  // https://bugs.chromium.org/p/skia/issues/detail?id=5907
  if (!RuntimeEnabledFeatures::colorCorrectRenderingEnabled())
    return nullptr;

  if (m_embeddedColorSpace)
    return m_embeddedColorSpace;
  return SkColorSpace::MakeNamed(SkColorSpace::kSRGB_Named);
}

void ImageDecoder::setColorProfileAndComputeTransform(const char* iccData,
                                                      unsigned iccLength) {
  sk_sp<SkColorSpace> colorSpace = SkColorSpace::MakeICC(iccData, iccLength);
  if (!colorSpace)
    DLOG(ERROR) << "Failed to parse image ICC profile";
  setColorSpaceAndComputeTransform(std::move(colorSpace));
}

void ImageDecoder::setColorSpaceAndComputeTransform(
    sk_sp<SkColorSpace> colorSpace) {
  DCHECK(!m_ignoreColorSpace);
  DCHECK(!m_hasHistogrammedColorSpace);

  m_embeddedColorSpace = colorSpace;

  m_sourceToOutputDeviceColorTransform = nullptr;

  // With color correct rendering, we do not transform to the output color space
  // at decode time.  Instead, we tag the raw image pixels and pass the tagged
  // SkImage to Skia.
  if (RuntimeEnabledFeatures::colorCorrectRenderingEnabled())
    return;

  if (!m_embeddedColorSpace)
    return;

  // Take a lock around initializing and accessing the global device color
  // profile.
  SpinLock::Guard guard(gTargetColorSpaceLock);

  // Initialize the output device profile to sRGB if it has not yet been
  // initialized.
  if (!gTargetColorSpace) {
    gTargetColorSpace =
        SkColorSpace::MakeNamed(SkColorSpace::kSRGB_Named).release();
  }

  if (SkColorSpace::Equals(m_embeddedColorSpace.get(), gTargetColorSpace)) {
    return;
  }

  m_sourceToOutputDeviceColorTransform =
      SkColorSpaceXform::New(m_embeddedColorSpace.get(), gTargetColorSpace);
}

}  // namespace blink

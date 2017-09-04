/*
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 *
 * Portions are Copyright (C) 2001 mozilla.org
 *
 * Other contributors:
 *   Stuart Parmenter <stuart@mozilla.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * Alternatively, the contents of this file may be used under the terms
 * of either the Mozilla Public License Version 1.1, found at
 * http://www.mozilla.org/MPL/ (the "MPL") or the GNU General Public
 * License Version 2.0, found at http://www.fsf.org/copyleft/gpl.html
 * (the "GPL"), in which case the provisions of the MPL or the GPL are
 * applicable instead of those above.  If you wish to allow use of your
 * version of this file only under the terms of one of those two
 * licenses (the MPL or the GPL) and not to allow others to use your
 * version of this file under the LGPL, indicate your decision by
 * deletingthe provisions above and replace them with the notice and
 * other provisions required by the MPL or the GPL, as the case may be.
 * If you do not delete the provisions above, a recipient may use your
 * version of this file under any of the LGPL, the MPL or the GPL.
 */

#include "platform/image-decoders/png/PNGImageDecoder.h"

#include "platform/image-decoders/png/PNGImageReader.h"
#include "png.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

PNGImageDecoder::PNGImageDecoder(AlphaOption alphaOption,
                                 ColorSpaceOption colorOptions,
                                 size_t maxDecodedBytes,
                                 size_t offset)
    : ImageDecoder(alphaOption, colorOptions, maxDecodedBytes),
      m_offset(offset) {}

PNGImageDecoder::~PNGImageDecoder() {}

inline float pngFixedToFloat(png_fixed_point x) {
  return ((float)x) * 0.00001f;
}

inline sk_sp<SkColorSpace> readColorSpace(png_structp png, png_infop info) {
  if (png_get_valid(png, info, PNG_INFO_sRGB)) {
    return SkColorSpace::MakeNamed(SkColorSpace::kSRGB_Named);
  }

#ifdef PNG_iCCP_SUPPORTED
  png_charp name = nullptr;
  int compression = 0;
#if (PNG_LIBPNG_VER < 10500) 
  png_charp profile = nullptr;
#else
  png_bytep profile = nullptr;
#endif
  png_uint_32 length = 0;
  if (png_get_iCCP(png, info, &name, &compression, &profile, &length)) {
    return SkColorSpace::MakeICC(profile, length);
  }
#endif // #ifdef PNG_iCCP_SUPPORTED 

  png_fixed_point chrm[8];
  if (png_get_cHRM_fixed(png, info, &chrm[0], &chrm[1], &chrm[2], &chrm[3],
                         &chrm[4], &chrm[5], &chrm[6], &chrm[7])) {
    SkColorSpacePrimaries primaries;
    primaries.fRX = pngFixedToFloat(chrm[2]);
    primaries.fRY = pngFixedToFloat(chrm[3]);
    primaries.fGX = pngFixedToFloat(chrm[4]);
    primaries.fGY = pngFixedToFloat(chrm[5]);
    primaries.fBX = pngFixedToFloat(chrm[6]);
    primaries.fBY = pngFixedToFloat(chrm[7]);
    primaries.fWX = pngFixedToFloat(chrm[0]);
    primaries.fWY = pngFixedToFloat(chrm[1]);

    SkMatrix44 toXYZD50(SkMatrix44::kUninitialized_Constructor);
    if (primaries.toXYZD50(&toXYZD50)) {
      png_fixed_point gammaFixed;
      if (PNG_INFO_gAMA == png_get_gAMA_fixed(png, info, &gammaFixed)) {
        SkColorSpaceTransferFn fn;
        fn.fA = 1.0f;
        fn.fB = fn.fC = fn.fD = fn.fE = fn.fF = 0.0f;
        // This is necessary because the gAMA chunk actually stores 1/gamma.
        fn.fG = 1.0f / pngFixedToFloat(gammaFixed);
        return SkColorSpace::MakeRGB(fn, toXYZD50);
      }

      // Note that we only use the cHRM tag when gAMA is present.  The
      // specification states that the cHRM is valid even without a gAMA
      // tag, but we cannot apply the cHRM without guessing a transfer
      // function.  It's possible that we should guess sRGB transfer
      // function, given that unmarked PNGs should be treated as sRGB.
      // However, the current behavior matches Safari and Firefox.
    }
  }

  return nullptr;
}

void PNGImageDecoder::headerAvailable() {
  png_structp png = m_reader->pngPtr();
  png_infop info = m_reader->infoPtr();
  png_uint_32 width = png_get_image_width(png, info);
  png_uint_32 height = png_get_image_height(png, info);

  // Protect against large PNGs. See http://bugzil.la/251381 for more details.
  const unsigned long maxPNGSize = 1000000UL;
  if (width > maxPNGSize || height > maxPNGSize) {
    longjmp(JMPBUF(png), 1);
    return;
  }

  // Set the image size now that the image header is available.
  if (!setSize(width, height)) {
    longjmp(JMPBUF(png), 1);
    return;
  }

  int bitDepth, colorType, interlaceType, compressionType, filterType, channels;
  png_get_IHDR(png, info, &width, &height, &bitDepth, &colorType,
               &interlaceType, &compressionType, &filterType);

  // The options we set here match what Mozilla does.

  // Expand to ensure we use 24-bit for RGB and 32-bit for RGBA.
  if (colorType == PNG_COLOR_TYPE_PALETTE ||
      (colorType == PNG_COLOR_TYPE_GRAY && bitDepth < 8))
    png_set_expand(png);

  png_bytep trns = 0;
  int trnsCount = 0;
  if (png_get_valid(png, info, PNG_INFO_tRNS)) {
    png_get_tRNS(png, info, &trns, &trnsCount, 0);
    png_set_expand(png);
  }

  if (bitDepth == 16)
    png_set_strip_16(png);

  if (colorType == PNG_COLOR_TYPE_GRAY ||
      colorType == PNG_COLOR_TYPE_GRAY_ALPHA)
    png_set_gray_to_rgb(png);

  if ((colorType & PNG_COLOR_MASK_COLOR) && !m_ignoreColorSpace) {
    // We only support color profiles for color PALETTE and RGB[A] PNG.
    // Supporting color profiles for gray-scale images is slightly tricky, at
    // least using the CoreGraphics ICC library, because we expand gray-scale
    // images to RGB but we do not similarly transform the color profile. We'd
    // either need to transform the color profile or we'd need to decode into a
    // gray-scale image buffer and hand that to CoreGraphics.
    sk_sp<SkColorSpace> colorSpace = readColorSpace(png, info);
    if (colorSpace) {
      setColorSpaceAndComputeTransform(colorSpace);
    }
  }

  if (!hasEmbeddedColorSpace()) {
    // TODO (msarett):
    // Applying the transfer function (gamma) should be handled by
    // SkColorSpaceXform.  Here we always convert to a transfer function that
    // is a 2.2 exponential.  This is a little strange given that the dst
    // transfer function is not necessarily a 2.2 exponential.
    // TODO (msarett):
    // Often, PNGs that specify their transfer function with the gAMA tag will
    // also specify their gamut with the cHRM tag.  We should read this tag
    // and do a full color space transformation if it is present.
    const double inverseGamma = 0.45455;
    const double defaultGamma = 2.2;
    double gamma;
    if (!m_ignoreColorSpace && png_get_gAMA(png, info, &gamma)) {
      const double maxGamma = 21474.83;
      if ((gamma <= 0.0) || (gamma > maxGamma)) {
        gamma = inverseGamma;
        png_set_gAMA(png, info, gamma);
      }
      png_set_gamma(png, defaultGamma, gamma);
    } else {
      png_set_gamma(png, defaultGamma, inverseGamma);
    }
  }

  // Tell libpng to send us rows for interlaced pngs.
  if (interlaceType == PNG_INTERLACE_ADAM7)
    png_set_interlace_handling(png);

  // Update our info now.
  png_read_update_info(png, info);
  channels = png_get_channels(png, info);
  ASSERT(channels == 3 || channels == 4);

  m_reader->setHasAlpha(channels == 4);

  if (m_reader->decodingSizeOnly()) {
// If we only needed the size, halt the reader.
#if PNG_LIBPNG_VER_MAJOR > 1 || \
    (PNG_LIBPNG_VER_MAJOR == 1 && PNG_LIBPNG_VER_MINOR >= 5)
    // Passing '0' tells png_process_data_pause() not to cache unprocessed data.
    m_reader->setReadOffset(m_reader->currentBufferSize() -
                            png_process_data_pause(png, 0));
#else
    m_reader->setReadOffset(m_reader->currentBufferSize() - png->buffer_size);
    png->buffer_size = 0;
#endif
  }
}

void PNGImageDecoder::rowAvailable(unsigned char* rowBuffer,
                                   unsigned rowIndex,
                                   int) {
  if (m_frameBufferCache.isEmpty())
    return;

  // Initialize the framebuffer if needed.
  ImageFrame& buffer = m_frameBufferCache[0];
  if (buffer.getStatus() == ImageFrame::FrameEmpty) {
    png_structp png = m_reader->pngPtr();
    if (!buffer.setSizeAndColorSpace(size().width(), size().height(),
                                     colorSpace())) {
      longjmp(JMPBUF(png), 1);
      return;
    }

    unsigned colorChannels = m_reader->hasAlpha() ? 4 : 3;
    if (PNG_INTERLACE_ADAM7 ==
        png_get_interlace_type(png, m_reader->infoPtr())) {
      m_reader->createInterlaceBuffer(colorChannels * size().width() *
                                      size().height());
      if (!m_reader->interlaceBuffer()) {
        longjmp(JMPBUF(png), 1);
        return;
      }
    }

    buffer.setStatus(ImageFrame::FramePartial);
    buffer.setHasAlpha(false);

    // For PNGs, the frame always fills the entire image.
    buffer.setOriginalFrameRect(IntRect(IntPoint(), size()));
  }

  /* libpng comments (here to explain what follows).
     *
     * this function is called for every row in the image. If the
     * image is interlacing, and you turned on the interlace handler,
     * this function will be called for every row in every pass.
     * Some of these rows will not be changed from the previous pass.
     * When the row is not changed, the new_row variable will be NULL.
     * The rows and passes are called in order, so you don't really
     * need the row_num and pass, but I'm supplying them because it
     * may make your life easier.
     */

  // Nothing to do if the row is unchanged, or the row is outside
  // the image bounds: libpng may send extra rows, ignore them to
  // make our lives easier.
  if (!rowBuffer)
    return;
  int y = rowIndex;
  if (y < 0 || y >= size().height())
    return;

  /* libpng comments (continued).
     *
     * For the non-NULL rows of interlaced images, you must call
     * png_progressive_combine_row() passing in the row and the
     * old row.  You can call this function for NULL rows (it will
     * just return) and for non-interlaced images (it just does the
     * memcpy for you) if it will make the code easier. Thus, you
     * can just do this for all cases:
     *
     *    png_progressive_combine_row(png_ptr, old_row, new_row);
     *
     * where old_row is what was displayed for previous rows. Note
     * that the first pass (pass == 0 really) will completely cover
     * the old row, so the rows do not have to be initialized. After
     * the first pass (and only for interlaced images), you will have
     * to pass the current row, and the function will combine the
     * old row and the new row.
     */

  bool hasAlpha = m_reader->hasAlpha();
  png_bytep row = rowBuffer;

  if (png_bytep interlaceBuffer = m_reader->interlaceBuffer()) {
    unsigned colorChannels = hasAlpha ? 4 : 3;
    row = interlaceBuffer + (rowIndex * colorChannels * size().width());
    png_progressive_combine_row(m_reader->pngPtr(), row, rowBuffer);
  }

  // Write the decoded row pixels to the frame buffer. The repetitive
  // form of the row write loops is for speed.
  ImageFrame::PixelData* const dstRow = buffer.getAddr(0, y);
  unsigned alphaMask = 255;
  int width = size().width();

  png_bytep srcPtr = row;
  if (hasAlpha) {
    // Here we apply the color space transformation to the dst space.
    // It does not really make sense to transform to a gamma-encoded
    // space and then immediately after, perform a linear premultiply.
    // Ideally we would pass kPremul_SkAlphaType to xform->apply(),
    // instructing SkColorSpaceXform to perform the linear premultiply
    // while the pixels are a linear space.
    // We cannot do this because when we apply the gamma encoding after
    // the premultiply, we will very likely end up with valid pixels
    // where R, G, and/or B are greater than A.  The legacy drawing
    // pipeline does not know how to handle this.
    if (SkColorSpaceXform* xform = colorTransform()) {
      SkColorSpaceXform::ColorFormat colorFormat =
          SkColorSpaceXform::kRGBA_8888_ColorFormat;
      xform->apply(colorFormat, dstRow, colorFormat, srcPtr, size().width(),
                   kUnpremul_SkAlphaType);
      srcPtr = (png_bytep)dstRow;
    }

    if (buffer.premultiplyAlpha()) {
      for (auto *dstPixel = dstRow; dstPixel < dstRow + width;
           dstPixel++, srcPtr += 4) {
        buffer.setRGBAPremultiply(dstPixel, srcPtr[0], srcPtr[1], srcPtr[2],
                                  srcPtr[3]);
        alphaMask &= srcPtr[3];
      }
    } else {
      for (auto *dstPixel = dstRow; dstPixel < dstRow + width;
           dstPixel++, srcPtr += 4) {
        buffer.setRGBARaw(dstPixel, srcPtr[0], srcPtr[1], srcPtr[2], srcPtr[3]);
        alphaMask &= srcPtr[3];
      }
    }
  } else {
    for (auto *dstPixel = dstRow; dstPixel < dstRow + width;
         dstPixel++, srcPtr += 3) {
      buffer.setRGBARaw(dstPixel, srcPtr[0], srcPtr[1], srcPtr[2], 255);
    }

    // We'll apply the color space xform to opaque pixels after they have been
    // written to the ImageFrame, purely because SkColorSpaceXform supports
    // RGBA (and not RGB).
    if (SkColorSpaceXform* xform = colorTransform()) {
      xform->apply(xformColorFormat(), dstRow, xformColorFormat(), dstRow,
                   size().width(), kOpaque_SkAlphaType);
    }
  }

  if (alphaMask != 255 && !buffer.hasAlpha())
    buffer.setHasAlpha(true);

  buffer.setPixelsChanged(true);
}

void PNGImageDecoder::complete() {
  if (m_frameBufferCache.isEmpty())
    return;

  m_frameBufferCache[0].setStatus(ImageFrame::FrameComplete);
}

inline bool isComplete(const PNGImageDecoder* decoder) {
  return decoder->frameIsCompleteAtIndex(0);
}

void PNGImageDecoder::decode(bool onlySize) {
  if (failed())
    return;

  if (!m_reader)
    m_reader = makeUnique<PNGImageReader>(this, m_offset);

  // If we couldn't decode the image but have received all the data, decoding
  // has failed.
  if (!m_reader->decode(*m_data, onlySize) && isAllDataReceived())
    setFailed();

  // If decoding is done or failed, we don't need the PNGImageReader anymore.
  if (isComplete(this) || failed())
    m_reader.reset();
}

}  // namespace blink

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

#include "platform/image-decoders/png/PNGImageReader.h"

#include "platform/image-decoders/SegmentReader.h"
#include "png.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace {

inline blink::PNGImageDecoder* imageDecoder(png_structp png) {
  return static_cast<blink::PNGImageDecoder*>(png_get_progressive_ptr(png));
}

void PNGAPI pngHeaderAvailable(png_structp png, png_infop) {
  imageDecoder(png)->headerAvailable();
}

void PNGAPI pngRowAvailable(png_structp png,
                            png_bytep row,
                            png_uint_32 rowIndex,
                            int state) {
  imageDecoder(png)->rowAvailable(row, rowIndex, state);
}

void PNGAPI pngComplete(png_structp png, png_infop) {
  imageDecoder(png)->complete();
}

void PNGAPI pngFailed(png_structp png, png_const_charp) {
  longjmp(JMPBUF(png), 1);
}

}  // namespace

namespace blink {

PNGImageReader::PNGImageReader(PNGImageDecoder* decoder, size_t readOffset)
    : m_decoder(decoder),
      m_readOffset(readOffset),
      m_currentBufferSize(0),
      m_decodingSizeOnly(false),
      m_hasAlpha(false) {
  m_png = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, pngFailed, 0);
  m_info = png_create_info_struct(m_png);
  png_set_progressive_read_fn(m_png, m_decoder, pngHeaderAvailable,
                              pngRowAvailable, pngComplete);
}

PNGImageReader::~PNGImageReader() {
  png_destroy_read_struct(m_png ? &m_png : 0, m_info ? &m_info : 0, 0);
  DCHECK(!m_png && !m_info);

  m_readOffset = 0;
}

bool PNGImageReader::decode(const SegmentReader& data, bool sizeOnly) {
  m_decodingSizeOnly = sizeOnly;

  // We need to do the setjmp here. Otherwise bad things will happen.
  if (setjmp(JMPBUF(m_png)))
    return m_decoder->setFailed();

  const char* segment;
  while (size_t segmentLength = data.getSomeData(segment, m_readOffset)) {
    m_readOffset += segmentLength;
    m_currentBufferSize = m_readOffset;
    png_process_data(m_png, m_info,
                     reinterpret_cast<png_bytep>(const_cast<char*>(segment)),
                     segmentLength);
    if (sizeOnly ? m_decoder->isDecodedSizeAvailable()
                 : m_decoder->frameIsCompleteAtIndex(0))
      return true;
  }

  return false;
}

}  // namespace blink

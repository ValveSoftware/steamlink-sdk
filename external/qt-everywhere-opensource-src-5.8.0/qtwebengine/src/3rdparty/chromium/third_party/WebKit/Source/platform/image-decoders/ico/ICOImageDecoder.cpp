/*
 * Copyright (c) 2008, 2009, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/image-decoders/ico/ICOImageDecoder.h"

#include "platform/Histogram.h"
#include "platform/image-decoders/png/PNGImageDecoder.h"
#include "wtf/PtrUtil.h"
#include "wtf/Threading.h"
#include <algorithm>

namespace blink {

// Number of bits in .ICO/.CUR used to store the directory and its entries,
// respectively (doesn't match sizeof values for member structs since we omit
// some fields).
static const size_t sizeOfDirectory = 6;
static const size_t sizeOfDirEntry = 16;

ICOImageDecoder::ICOImageDecoder(AlphaOption alphaOption, GammaAndColorProfileOption colorOptions, size_t maxDecodedBytes)
    : ImageDecoder(alphaOption, colorOptions, maxDecodedBytes)
    , m_fastReader(nullptr)
    , m_decodedOffset(0)
    , m_dirEntriesCount(0)
{
}

ICOImageDecoder::~ICOImageDecoder()
{
}

void ICOImageDecoder::onSetData(SegmentReader* data)
{
    m_fastReader.setData(data);

    for (BMPReaders::iterator i(m_bmpReaders.begin()); i != m_bmpReaders.end(); ++i) {
        if (*i)
            (*i)->setData(data);
    }
    for (size_t i = 0; i < m_pngDecoders.size(); ++i)
        setDataForPNGDecoderAtIndex(i);
}

IntSize ICOImageDecoder::size() const
{
    return m_frameSize.isEmpty() ? ImageDecoder::size() : m_frameSize;
}

IntSize ICOImageDecoder::frameSizeAtIndex(size_t index) const
{
    return (index && (index < m_dirEntries.size())) ? m_dirEntries[index].m_size : size();
}

bool ICOImageDecoder::setSize(unsigned width, unsigned height)
{
    // The size calculated inside the BMPImageReader had better match the one in
    // the icon directory.
    return m_frameSize.isEmpty() ? ImageDecoder::setSize(width, height) : ((IntSize(width, height) == m_frameSize) || setFailed());
}

bool ICOImageDecoder::frameIsCompleteAtIndex(size_t index) const
{
    if (index >= m_dirEntries.size())
        return false;
    const IconDirectoryEntry& dirEntry = m_dirEntries[index];
    return (dirEntry.m_imageOffset + dirEntry.m_byteSize) <= m_data->size();
}

bool ICOImageDecoder::setFailed()
{
    m_bmpReaders.clear();
    m_pngDecoders.clear();
    return ImageDecoder::setFailed();
}

bool ICOImageDecoder::hotSpot(IntPoint& hotSpot) const
{
    // When unspecified, the default frame is always frame 0. This is consistent with
    // BitmapImage where currentFrame() starts at 0 and only increases when animation is
    // requested.
    return hotSpotAtIndex(0, hotSpot);
}

bool ICOImageDecoder::hotSpotAtIndex(size_t index, IntPoint& hotSpot) const
{
    if (index >= m_dirEntries.size() || m_fileType != CURSOR)
        return false;

    hotSpot = m_dirEntries[index].m_hotSpot;
    return true;
}


// static
bool ICOImageDecoder::compareEntries(const IconDirectoryEntry& a, const IconDirectoryEntry& b)
{
    // Larger icons are better.  After that, higher bit-depth icons are better.
    const int aEntryArea = a.m_size.width() * a.m_size.height();
    const int bEntryArea = b.m_size.width() * b.m_size.height();
    return (aEntryArea == bEntryArea) ? (a.m_bitCount > b.m_bitCount) : (aEntryArea > bEntryArea);
}

size_t ICOImageDecoder::decodeFrameCount()
{
    decodeSize();

    // If decodeSize() fails, return the existing number of frames.  This way
    // if we get halfway through the image before decoding fails, we won't
    // suddenly start reporting that the image has zero frames.
    if (failed())
        return m_frameBufferCache.size();

    // Length of sequence of completely received frames.
    for (size_t i = 0; i < m_dirEntries.size(); ++i) {
        const IconDirectoryEntry& dirEntry = m_dirEntries[i];
        if ((dirEntry.m_imageOffset + dirEntry.m_byteSize) > m_data->size())
            return i;
    }
    return m_dirEntries.size();
}

void ICOImageDecoder::setDataForPNGDecoderAtIndex(size_t index)
{
    if (!m_pngDecoders[index])
        return;

    m_pngDecoders[index]->setData(m_data.get(), isAllDataReceived());
}

void ICOImageDecoder::decode(size_t index, bool onlySize)
{
    if (failed())
        return;

    // Defensively clear the FastSharedBufferReader's cache, as another caller
    // may have called SharedBuffer::mergeSegmentsIntoBuffer().
    m_fastReader.clearCache();

    // If we couldn't decode the image but we've received all the data, decoding
    // has failed.
    if ((!decodeDirectory() || (!onlySize && !decodeAtIndex(index))) && isAllDataReceived()) {
        setFailed();
    // If we're done decoding this frame, we don't need the BMPImageReader or
    // PNGImageDecoder anymore.  (If we failed, these have already been
    // cleared.)
    } else if ((m_frameBufferCache.size() > index) && (m_frameBufferCache[index].getStatus() == ImageFrame::FrameComplete)) {
        m_bmpReaders[index].reset();
        m_pngDecoders[index].reset();
    }
}

bool ICOImageDecoder::decodeDirectory()
{
    // Read and process directory.
    if ((m_decodedOffset < sizeOfDirectory) && !processDirectory())
        return false;

    // Read and process directory entries.
    return (m_decodedOffset >= (sizeOfDirectory + (m_dirEntriesCount * sizeOfDirEntry))) || processDirectoryEntries();
}

bool ICOImageDecoder::decodeAtIndex(size_t index)
{
    ASSERT_WITH_SECURITY_IMPLICATION(index < m_dirEntries.size());
    const IconDirectoryEntry& dirEntry = m_dirEntries[index];
    const ImageType imageType = imageTypeAtIndex(index);
    if (imageType == Unknown)
        return false; // Not enough data to determine image type yet.

    if (imageType == BMP) {
        if (!m_bmpReaders[index]) {
            m_bmpReaders[index] = wrapUnique(new BMPImageReader(this, dirEntry.m_imageOffset, 0, true));
            m_bmpReaders[index]->setData(m_data.get());
        }
        // Update the pointer to the buffer as it could change after
        // m_frameBufferCache.resize().
        m_bmpReaders[index]->setBuffer(&m_frameBufferCache[index]);
        m_frameSize = dirEntry.m_size;
        bool result = m_bmpReaders[index]->decodeBMP(false);
        m_frameSize = IntSize();
        return result;
    }

    if (!m_pngDecoders[index]) {
        AlphaOption alphaOption = m_premultiplyAlpha ? AlphaPremultiplied : AlphaNotPremultiplied;
        GammaAndColorProfileOption colorOptions = m_ignoreGammaAndColorProfile ? GammaAndColorProfileIgnored : GammaAndColorProfileApplied;
        m_pngDecoders[index] = wrapUnique(new PNGImageDecoder(alphaOption, colorOptions, m_maxDecodedBytes, dirEntry.m_imageOffset));
        setDataForPNGDecoderAtIndex(index);
    }
    // Fail if the size the PNGImageDecoder calculated does not match the size
    // in the directory.
    if (m_pngDecoders[index]->isSizeAvailable() && (m_pngDecoders[index]->size() != dirEntry.m_size))
        return setFailed();
    m_frameBufferCache[index] = *m_pngDecoders[index]->frameBufferAtIndex(0);
    m_frameBufferCache[index].setPremultiplyAlpha(m_premultiplyAlpha);
    return !m_pngDecoders[index]->failed() || setFailed();
}

bool ICOImageDecoder::processDirectory()
{
    // Read directory.
    ASSERT(!m_decodedOffset);
    if (m_data->size() < sizeOfDirectory)
        return false;
    const uint16_t fileType = readUint16(2);
    m_dirEntriesCount = readUint16(4);
    m_decodedOffset = sizeOfDirectory;

    // See if this is an icon filetype we understand, and make sure we have at
    // least one entry in the directory.
    if (((fileType != ICON) && (fileType != CURSOR)) || (!m_dirEntriesCount))
        return setFailed();

    m_fileType = static_cast<FileType>(fileType);
    return true;
}

bool ICOImageDecoder::processDirectoryEntries()
{
    // Read directory entries.
    ASSERT(m_decodedOffset == sizeOfDirectory);
    if ((m_decodedOffset > m_data->size()) || ((m_data->size() - m_decodedOffset) < (m_dirEntriesCount * sizeOfDirEntry)))
        return false;

    // Enlarge member vectors to hold all the entries.
    m_dirEntries.resize(m_dirEntriesCount);
    m_bmpReaders.resize(m_dirEntriesCount);
    m_pngDecoders.resize(m_dirEntriesCount);

    for (IconDirectoryEntries::iterator i(m_dirEntries.begin()); i != m_dirEntries.end(); ++i)
        *i = readDirectoryEntry();  // Updates m_decodedOffset.

    // Make sure the specified image offsets are past the end of the directory
    // entries.
    for (IconDirectoryEntries::iterator i(m_dirEntries.begin()); i != m_dirEntries.end(); ++i) {
        if (i->m_imageOffset < m_decodedOffset)
            return setFailed();
    }

    DEFINE_THREAD_SAFE_STATIC_LOCAL(blink::CustomCountHistogram,
        dimensionsLocationHistogram,
        new blink::CustomCountHistogram("Blink.DecodedImage.EffectiveDimensionsLocation.ICO", 0, 50000, 50));
    dimensionsLocationHistogram.count(m_decodedOffset - 1);

    // Arrange frames in decreasing quality order.
    std::sort(m_dirEntries.begin(), m_dirEntries.end(), compareEntries);

    // The image size is the size of the largest entry.
    const IconDirectoryEntry& dirEntry = m_dirEntries.first();
    // Technically, this next call shouldn't be able to fail, since the width
    // and height here are each <= 256, and |m_frameSize| is empty.
    return setSize(dirEntry.m_size.width(), dirEntry.m_size.height());
}

ICOImageDecoder::IconDirectoryEntry ICOImageDecoder::readDirectoryEntry()
{
    // Read icon data.
    // The following calls to readUint8() return a uint8_t, which is appropriate
    // because that's the on-disk type of the width and height values.  Storing
    // them in ints (instead of matching uint8_ts) is so we can record dimensions
    // of size 256 (which is what a zero byte really means).
    int width = readUint8(0);
    if (!width)
        width = 256;
    int height = readUint8(1);
    if (!height)
        height = 256;
    IconDirectoryEntry entry;
    entry.m_size = IntSize(width, height);
    if (m_fileType == CURSOR) {
        entry.m_bitCount = 0;
        entry.m_hotSpot = IntPoint(readUint16(4), readUint16(6));
    } else {
        entry.m_bitCount = readUint16(6);
        entry.m_hotSpot = IntPoint();
    }
    entry.m_byteSize = readUint32(8);
    entry.m_imageOffset = readUint32(12);

    // Some icons don't have a bit depth, only a color count.  Convert the
    // color count to the minimum necessary bit depth.  It doesn't matter if
    // this isn't quite what the bitmap info header says later, as we only use
    // this value to determine which icon entry is best.
    if (!entry.m_bitCount) {
        int colorCount = readUint8(2);
        if (!colorCount)
            colorCount = 256;  // Vague in the spec, needed by real-world icons.
        for (--colorCount; colorCount; colorCount >>= 1)
            ++entry.m_bitCount;
    }

    m_decodedOffset += sizeOfDirEntry;
    return entry;
}

ICOImageDecoder::ImageType ICOImageDecoder::imageTypeAtIndex(size_t index)
{
    // Check if this entry is a BMP or a PNG; we need 4 bytes to check the magic
    // number.
    ASSERT_WITH_SECURITY_IMPLICATION(index < m_dirEntries.size());
    const uint32_t imageOffset = m_dirEntries[index].m_imageOffset;
    if ((imageOffset > m_data->size()) || ((m_data->size() - imageOffset) < 4))
        return Unknown;
    char buffer[4];
    const char* data = m_fastReader.getConsecutiveData(imageOffset, 4, buffer);
    return strncmp(data, "\x89PNG", 4) ? BMP : PNG;
}

} // namespace blink

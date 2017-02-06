/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#include "platform/image-decoders/gif/GIFImageDecoder.h"

#include "platform/image-decoders/gif/GIFImageReader.h"
#include "wtf/NotFound.h"
#include "wtf/PtrUtil.h"
#include <limits>

namespace blink {

GIFImageDecoder::GIFImageDecoder(AlphaOption alphaOption, GammaAndColorProfileOption colorOptions, size_t maxDecodedBytes)
    : ImageDecoder(alphaOption, colorOptions, maxDecodedBytes)
    , m_purgeAggressively(false)
    , m_repetitionCount(cAnimationLoopOnce)
{
}

GIFImageDecoder::~GIFImageDecoder()
{
}

void GIFImageDecoder::onSetData(SegmentReader* data)
{
    if (m_reader)
        m_reader->setData(data);
}

int GIFImageDecoder::repetitionCount() const
{
    // This value can arrive at any point in the image data stream.  Most GIFs
    // in the wild declare it near the beginning of the file, so it usually is
    // set by the time we've decoded the size, but (depending on the GIF and the
    // packets sent back by the webserver) not always.  If the reader hasn't
    // seen a loop count yet, it will return cLoopCountNotSeen, in which case we
    // should default to looping once (the initial value for
    // |m_repetitionCount|).
    //
    // There are some additional wrinkles here. First, ImageSource::clear()
    // may destroy the reader, making the result from the reader _less_
    // authoritative on future calls if the recreated reader hasn't seen the
    // loop count.  We don't need to special-case this because in this case the
    // new reader will once again return cLoopCountNotSeen, and we won't
    // overwrite the cached correct value.
    //
    // Second, a GIF might never set a loop count at all, in which case we
    // should continue to treat it as a "loop once" animation.  We don't need
    // special code here either, because in this case we'll never change
    // |m_repetitionCount| from its default value.
    //
    // Third, we use the same GIFImageReader for counting frames and we might
    // see the loop count and then encounter a decoding error which happens
    // later in the stream. It is also possible that no frames are in the
    // stream. In these cases we should just loop once.
    if (isAllDataReceived() && parseCompleted() && m_reader->imagesCount() == 1)
        m_repetitionCount = cAnimationNone;
    else if (failed() || (m_reader && (!m_reader->imagesCount())))
        m_repetitionCount = cAnimationLoopOnce;
    else if (m_reader && m_reader->loopCount() != cLoopCountNotSeen)
        m_repetitionCount = m_reader->loopCount();
    return m_repetitionCount;
}

bool GIFImageDecoder::frameIsCompleteAtIndex(size_t index) const
{
    return m_reader && (index < m_reader->imagesCount()) && m_reader->frameContext(index)->isComplete();
}

float GIFImageDecoder::frameDurationAtIndex(size_t index) const
{
    return (m_reader && (index < m_reader->imagesCount()) &&
        m_reader->frameContext(index)->isHeaderDefined()) ?
        m_reader->frameContext(index)->delayTime() : 0;
}

bool GIFImageDecoder::setFailed()
{
    m_reader.reset();
    return ImageDecoder::setFailed();
}

bool GIFImageDecoder::haveDecodedRow(size_t frameIndex, GIFRow::const_iterator rowBegin, size_t width, size_t rowNumber, unsigned repeatCount, bool writeTransparentPixels)
{
    const GIFFrameContext* frameContext = m_reader->frameContext(frameIndex);
    // The pixel data and coordinates supplied to us are relative to the frame's
    // origin within the entire image size, i.e.
    // (frameContext->xOffset, frameContext->yOffset). There is no guarantee
    // that width == (size().width() - frameContext->xOffset), so
    // we must ensure we don't run off the end of either the source data or the
    // row's X-coordinates.
    const int xBegin = frameContext->xOffset();
    const int yBegin = frameContext->yOffset() + rowNumber;
    const int xEnd = std::min(static_cast<int>(frameContext->xOffset() + width), size().width());
    const int yEnd = std::min(static_cast<int>(frameContext->yOffset() + rowNumber + repeatCount), size().height());
    if (!width || (xBegin < 0) || (yBegin < 0) || (xEnd <= xBegin) || (yEnd <= yBegin))
        return true;

    const GIFColorMap::Table& colorTable = frameContext->localColorMap().isDefined() ? frameContext->localColorMap().getTable() : m_reader->globalColorMap().getTable();

    if (colorTable.isEmpty())
        return true;

    GIFColorMap::Table::const_iterator colorTableIter = colorTable.begin();

    // Initialize the frame if necessary.
    ImageFrame& buffer = m_frameBufferCache[frameIndex];
    if ((buffer.getStatus() == ImageFrame::FrameEmpty) && !initFrameBuffer(frameIndex))
        return false;

    const size_t transparentPixel = frameContext->transparentPixel();
    GIFRow::const_iterator rowEnd = rowBegin + (xEnd - xBegin);
    ImageFrame::PixelData* currentAddress = buffer.getAddr(xBegin, yBegin);

    // We may or may not need to write transparent pixels to the buffer.
    // If we're compositing against a previous image, it's wrong, and if
    // we're writing atop a cleared, fully transparent buffer, it's
    // unnecessary; but if we're decoding an interlaced gif and
    // displaying it "Haeberli"-style, we must write these for passes
    // beyond the first, or the initial passes will "show through" the
    // later ones.
    //
    // The loops below are almost identical. One writes a transparent pixel
    // and one doesn't based on the value of |writeTransparentPixels|.
    // The condition check is taken out of the loop to enhance performance.
    // This optimization reduces decoding time by about 15% for a 3MB image.
    if (writeTransparentPixels) {
        for (; rowBegin != rowEnd; ++rowBegin, ++currentAddress) {
            const size_t sourceValue = *rowBegin;
            if ((sourceValue != transparentPixel) && (sourceValue < colorTable.size())) {
                *currentAddress = colorTableIter[sourceValue];
            } else {
                *currentAddress = 0;
                m_currentBufferSawAlpha = true;
            }
        }
    } else {
        for (; rowBegin != rowEnd; ++rowBegin, ++currentAddress) {
            const size_t sourceValue = *rowBegin;
            if ((sourceValue != transparentPixel) && (sourceValue < colorTable.size()))
                *currentAddress = colorTableIter[sourceValue];
            else
                m_currentBufferSawAlpha = true;
        }
    }

    // Tell the frame to copy the row data if need be.
    if (repeatCount > 1)
        buffer.copyRowNTimes(xBegin, xEnd, yBegin, yEnd);

    buffer.setPixelsChanged(true);
    return true;
}

bool GIFImageDecoder::parseCompleted() const
{
    return m_reader && m_reader->parseCompleted();
}

bool GIFImageDecoder::frameComplete(size_t frameIndex)
{
    // Initialize the frame if necessary.  Some GIFs insert do-nothing frames,
    // in which case we never reach haveDecodedRow() before getting here.
    ImageFrame& buffer = m_frameBufferCache[frameIndex];
    if ((buffer.getStatus() == ImageFrame::FrameEmpty) && !initFrameBuffer(frameIndex))
        return false; // initFrameBuffer() has already called setFailed().

    buffer.setStatus(ImageFrame::FrameComplete);

    if (!m_currentBufferSawAlpha) {
        // The whole frame was non-transparent, so it's possible that the entire
        // resulting buffer was non-transparent, and we can setHasAlpha(false).
        if (buffer.originalFrameRect().contains(IntRect(IntPoint(), size()))) {
            buffer.setHasAlpha(false);
            buffer.setRequiredPreviousFrameIndex(kNotFound);
        } else if (buffer.requiredPreviousFrameIndex() != kNotFound) {
            // Tricky case.  This frame does not have alpha only if everywhere
            // outside its rect doesn't have alpha.  To know whether this is
            // true, we check the start state of the frame -- if it doesn't have
            // alpha, we're safe.
            const ImageFrame* prevBuffer = &m_frameBufferCache[buffer.requiredPreviousFrameIndex()];
            ASSERT(prevBuffer->getDisposalMethod() != ImageFrame::DisposeOverwritePrevious);

            // Now, if we're at a DisposeNotSpecified or DisposeKeep frame, then
            // we can say we have no alpha if that frame had no alpha.  But
            // since in initFrameBuffer() we already copied that frame's alpha
            // state into the current frame's, we need do nothing at all here.
            //
            // The only remaining case is a DisposeOverwriteBgcolor frame.  If
            // it had no alpha, and its rect is contained in the current frame's
            // rect, we know the current frame has no alpha.
            if ((prevBuffer->getDisposalMethod() == ImageFrame::DisposeOverwriteBgcolor) && !prevBuffer->hasAlpha() && buffer.originalFrameRect().contains(prevBuffer->originalFrameRect()))
                buffer.setHasAlpha(false);
        }
    }

    return true;
}

size_t GIFImageDecoder::clearCacheExceptFrame(size_t clearExceptFrame)
{
    // We expect that after this call, we'll be asked to decode frames after
    // this one.  So we want to avoid clearing frames such that those requests
    // would force re-decoding from the beginning of the image.
    //
    // When |clearExceptFrame| is e.g. DisposeKeep, simply not clearing that
    // frame is sufficient, as the next frame will be based on it, and in
    // general future frames can't be based on anything previous.
    //
    // However, if this frame is DisposeOverwritePrevious, then subsequent
    // frames will depend on this frame's required previous frame.  In this
    // case, we need to preserve both this frame and that one.
    size_t clearExceptFrame2 = kNotFound;
    if (clearExceptFrame < m_frameBufferCache.size()) {
        const ImageFrame& frame = m_frameBufferCache[clearExceptFrame];
        if ((frame.getStatus() != ImageFrame::FrameEmpty) && (frame.getDisposalMethod() == ImageFrame::DisposeOverwritePrevious)) {
            clearExceptFrame2 = clearExceptFrame;
            clearExceptFrame = frame.requiredPreviousFrameIndex();
        }
    }

    // Now |clearExceptFrame| indicates the frame that future frames will
    // depend on.  But if decoding is skipping forward past intermediate frames,
    // this frame may be FrameEmpty.  So we need to keep traversing back through
    // the required previous frames until we find the nearest non-empty
    // ancestor.  Preserving that will minimize the amount of future decoding
    // needed.
    while ((clearExceptFrame < m_frameBufferCache.size()) && (m_frameBufferCache[clearExceptFrame].getStatus() == ImageFrame::FrameEmpty))
        clearExceptFrame = m_frameBufferCache[clearExceptFrame].requiredPreviousFrameIndex();
    return clearCacheExceptTwoFrames(clearExceptFrame, clearExceptFrame2);
}


size_t GIFImageDecoder::clearCacheExceptTwoFrames(size_t clearExceptFrame1, size_t clearExceptFrame2)
{
    size_t frameBytesCleared = 0;
    for (size_t i = 0; i < m_frameBufferCache.size(); ++i) {
        if (m_frameBufferCache[i].getStatus() != ImageFrame::FrameEmpty && i != clearExceptFrame1 && i != clearExceptFrame2) {
            frameBytesCleared += frameBytesAtIndex(i);
            clearFrameBuffer(i);
        }
    }
    return frameBytesCleared;
}

void GIFImageDecoder::clearFrameBuffer(size_t frameIndex)
{
    if (m_reader && m_frameBufferCache[frameIndex].getStatus() == ImageFrame::FramePartial) {
        // Reset the state of the partial frame in the reader so that the frame
        // can be decoded again when requested.
        m_reader->clearDecodeState(frameIndex);
    }
    ImageDecoder::clearFrameBuffer(frameIndex);
}

size_t GIFImageDecoder::decodeFrameCount()
{
    parse(GIFFrameCountQuery);
    // If decoding fails, |m_reader| will have been destroyed.  Instead of
    // returning 0 in this case, return the existing number of frames.  This way
    // if we get halfway through the image before decoding fails, we won't
    // suddenly start reporting that the image has zero frames.
    return failed() ? m_frameBufferCache.size() : m_reader->imagesCount();
}

void GIFImageDecoder::initializeNewFrame(size_t index)
{
    ImageFrame* buffer = &m_frameBufferCache[index];
    const GIFFrameContext* frameContext = m_reader->frameContext(index);
    buffer->setOriginalFrameRect(intersection(frameContext->frameRect(), IntRect(IntPoint(), size())));
    buffer->setDuration(frameContext->delayTime());
    buffer->setDisposalMethod(frameContext->getDisposalMethod());
    buffer->setRequiredPreviousFrameIndex(findRequiredPreviousFrame(index, false));
}

void GIFImageDecoder::decode(size_t index)
{
    parse(GIFFrameCountQuery);

    if (failed())
        return;

    updateAggressivePurging(index);

    Vector<size_t> framesToDecode;
    size_t frameToDecode = index;
    do {
        framesToDecode.append(frameToDecode);
        frameToDecode = m_frameBufferCache[frameToDecode].requiredPreviousFrameIndex();
    } while (frameToDecode != kNotFound && m_frameBufferCache[frameToDecode].getStatus() != ImageFrame::FrameComplete);

    for (auto i = framesToDecode.rbegin(); i != framesToDecode.rend(); ++i) {
        if (!m_reader->decode(*i)) {
            setFailed();
            return;
        }

        if (m_purgeAggressively)
            clearCacheExceptFrame(*i);

        // We need more data to continue decoding.
        if (m_frameBufferCache[*i].getStatus() != ImageFrame::FrameComplete)
            break;
    }

    // It is also a fatal error if all data is received and we have decoded all
    // frames available but the file is truncated.
    if (index >= m_frameBufferCache.size() - 1 && isAllDataReceived() && m_reader && !m_reader->parseCompleted())
        setFailed();
}

void GIFImageDecoder::parse(GIFParseQuery query)
{
    if (failed())
        return;

    if (!m_reader) {
        m_reader = wrapUnique(new GIFImageReader(this));
        m_reader->setData(m_data);
    }

    if (!m_reader->parse(query))
        setFailed();
}

bool GIFImageDecoder::initFrameBuffer(size_t frameIndex)
{
    // Initialize the frame rect in our buffer.
    ImageFrame* const buffer = &m_frameBufferCache[frameIndex];

    size_t requiredPreviousFrameIndex = buffer->requiredPreviousFrameIndex();
    if (requiredPreviousFrameIndex == kNotFound) {
        // This frame doesn't rely on any previous data.
        if (!buffer->setSize(size().width(), size().height()))
            return setFailed();
    } else {
        const ImageFrame* prevBuffer = &m_frameBufferCache[requiredPreviousFrameIndex];
        ASSERT(prevBuffer->getStatus() == ImageFrame::FrameComplete);

        // Preserve the last frame as the starting state for this frame.
        if (!buffer->copyBitmapData(*prevBuffer))
            return setFailed();

        if (prevBuffer->getDisposalMethod() == ImageFrame::DisposeOverwriteBgcolor) {
            // We want to clear the previous frame to transparent, without
            // affecting pixels in the image outside of the frame.
            const IntRect& prevRect = prevBuffer->originalFrameRect();
            ASSERT(!prevRect.contains(IntRect(IntPoint(), size())));
            buffer->zeroFillFrameRect(prevRect);
        }
    }

    // Update our status to be partially complete.
    buffer->setStatus(ImageFrame::FramePartial);

    // Reset the alpha pixel tracker for this frame.
    m_currentBufferSawAlpha = false;
    return true;
}

void GIFImageDecoder::updateAggressivePurging(size_t index)
{
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
    // We are about to multiply by 4, which may require an extra bit of storage
    bool wouldOverflow = frameArea > (UINT64_C(1) << 62);
    if (wouldOverflow) {
        m_purgeAggressively = true;
        return;
    }

    const uint64_t frameMemoryUsage = frameArea * 4; // 4 bytes per pixel
    // We are about to multiply by a size_t, which does not have a fixed
    // size.
    // To simplify things, let's make sure our per-frame memory usage and
    // index can be stored in 32 bits and store the multiplicand in a 64-bit
    // number.
    wouldOverflow = (frameMemoryUsage > (UINT32_C(1) << 31))
        || (index > (UINT32_C(1) << 31));
    if (wouldOverflow) {
        m_purgeAggressively = true;
        return;
    }

    const uint64_t totalMemoryUsage = frameMemoryUsage * index;
    if (totalMemoryUsage > m_maxDecodedBytes) {
        m_purgeAggressively = true;
    }
}
} // namespace blink

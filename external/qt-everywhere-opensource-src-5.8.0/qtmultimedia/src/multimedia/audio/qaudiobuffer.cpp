/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qaudiobuffer.h"
#include "qaudiobuffer_p.h"

#include <QObject>
#include <QDebug>

QT_BEGIN_NAMESPACE


static void qRegisterAudioBufferMetaTypes()
{
    qRegisterMetaType<QAudioBuffer>();
}

Q_CONSTRUCTOR_FUNCTION(qRegisterAudioBufferMetaTypes)


class QAudioBufferPrivate : public QSharedData
{
public:
    QAudioBufferPrivate(QAbstractAudioBuffer *provider)
        : mProvider(provider)
        , mCount(1)
    {
    }

    ~QAudioBufferPrivate()
    {
        if (mProvider)
            mProvider->release();
    }

    void ref()
    {
        mCount.ref();
    }

    void deref()
    {
        if (!mCount.deref())
            delete this;
    }

    QAudioBufferPrivate *clone();

    static QAudioBufferPrivate *acquire(QAudioBufferPrivate *other)
    {
        if (!other)
            return 0;

        // Ref the other (if there are extant data() pointers, they will
        // also point here - it's a feature, not a bug, like QByteArray)
        other->ref();
        return other;
    }

    QAbstractAudioBuffer *mProvider;
    QAtomicInt mCount;
};

// Private class to go in .cpp file
class QMemoryAudioBufferProvider : public QAbstractAudioBuffer {
public:
    QMemoryAudioBufferProvider(const void *data, int frameCount, const QAudioFormat &format, qint64 startTime)
        : mStartTime(startTime)
        , mFrameCount(frameCount)
        , mFormat(format)
    {
        int numBytes = format.bytesForFrames(frameCount);
        if (numBytes > 0) {
            mBuffer = malloc(numBytes);
            if (!mBuffer) {
                // OOM, if that's likely
                mStartTime = -1;
                mFrameCount = 0;
                mFormat = QAudioFormat();
            } else {
                // Allocated, see if we have data to copy
                if (data) {
                    memcpy(mBuffer, data, numBytes);
                } else {
                    // We have to fill with the zero value..
                    switch (format.sampleType()) {
                        case QAudioFormat::SignedInt:
                            // Signed int means 0x80, 0x8000 is zero
                            // XXX this is not right for > 8 bits(0x8080 vs 0x8000)
                            memset(mBuffer, 0x80, numBytes);
                            break;
                        default:
                            memset(mBuffer, 0x0, numBytes);
                    }
                }
            }
        } else
            mBuffer = 0;
    }

    ~QMemoryAudioBufferProvider()
    {
        if (mBuffer)
            free(mBuffer);
    }

    void release() {delete this;}
    QAudioFormat format() const {return mFormat;}
    qint64 startTime() const {return mStartTime;}
    int frameCount() const {return mFrameCount;}

    void *constData() const {return mBuffer;}

    void *writableData() {return mBuffer;}
    QAbstractAudioBuffer *clone() const
    {
        return new QMemoryAudioBufferProvider(mBuffer, mFrameCount, mFormat, mStartTime);
    }

    void *mBuffer;
    qint64 mStartTime;
    int mFrameCount;
    QAudioFormat mFormat;
};

QAudioBufferPrivate *QAudioBufferPrivate::clone()
{
    // We want to create a single bufferprivate with a
    // single qaab
    // This should only be called when the count is > 1
    Q_ASSERT(mCount.load() > 1);

    if (mProvider) {
        QAbstractAudioBuffer *abuf = mProvider->clone();

        if (!abuf) {
            abuf = new QMemoryAudioBufferProvider(mProvider->constData(), mProvider->frameCount(), mProvider->format(), mProvider->startTime());
        }

        if (abuf) {
            return new QAudioBufferPrivate(abuf);
        }
    }

    return 0;
}

/*!
    \class QAbstractAudioBuffer
    \internal
*/

/*!
    \class QAudioBuffer
    \inmodule QtMultimedia
    \ingroup multimedia
    \ingroup multimedia_audio
    \brief The QAudioBuffer class represents a collection of audio samples with a specific format and sample rate.
*/
// ^ Mostly useful with probe or decoder

/*!
    Create a new, empty, invalid buffer.
 */
QAudioBuffer::QAudioBuffer()
    : d(0)
{
}

/*!
    \internal
    Create a new audio buffer from the supplied \a provider.  This
    constructor is typically only used when handling certain hardware
    or media framework specific buffers, and generally isn't useful
    in application code.
 */
QAudioBuffer::QAudioBuffer(QAbstractAudioBuffer *provider)
    : d(new QAudioBufferPrivate(provider))
{
}
/*!
    Creates a new audio buffer from \a other.  Generally
    this will have copy-on-write semantics - a copy will
    only be made when it has to be.
 */
QAudioBuffer::QAudioBuffer(const QAudioBuffer &other)
{
    d = QAudioBufferPrivate::acquire(other.d);
}

/*!
    Creates a new audio buffer from the supplied \a data, in the
    given \a format.  The format will determine how the number
    and sizes of the samples are interpreted from the \a data.

    If the supplied \a data is not an integer multiple of the
    calculated frame size, the excess data will not be used.

    This audio buffer will copy the contents of \a data.

    \a startTime (in microseconds) indicates when this buffer
    starts in the stream.
    If this buffer is not part of a stream, set it to -1.
 */
QAudioBuffer::QAudioBuffer(const QByteArray &data, const QAudioFormat &format, qint64 startTime)
{
    if (format.isValid()) {
        int frameCount = format.framesForBytes(data.size());
        d = new QAudioBufferPrivate(new QMemoryAudioBufferProvider(data.constData(), frameCount, format, startTime));
    } else
        d = 0;
}

/*!
    Creates a new audio buffer with space for \a numFrames frames of
    the given \a format.  The individual samples will be initialized
    to the default for the format.

    \a startTime (in microseconds) indicates when this buffer
    starts in the stream.
    If this buffer is not part of a stream, set it to -1.
 */
QAudioBuffer::QAudioBuffer(int numFrames, const QAudioFormat &format, qint64 startTime)
{
    if (format.isValid())
        d = new QAudioBufferPrivate(new QMemoryAudioBufferProvider(0, numFrames, format, startTime));
    else
        d = 0;
}

/*!
    Assigns the \a other buffer to this.
 */
QAudioBuffer &QAudioBuffer::operator =(const QAudioBuffer &other)
{
    if (this->d != other.d) {
        if (d)
            d->deref();
        d = QAudioBufferPrivate::acquire(other.d);
    }
    return *this;
}

/*!
    Destroys this audio buffer.
 */
QAudioBuffer::~QAudioBuffer()
{
    if (d)
        d->deref();
}

/*!
    Returns true if this is a valid buffer.  A valid buffer
    has more than zero frames in it and a valid format.
 */
bool QAudioBuffer::isValid() const
{
    if (!d || !d->mProvider)
        return false;
    return d->mProvider->format().isValid() && (d->mProvider->frameCount() > 0);
}

/*!
    Returns the \l {QAudioFormat}{format} of this buffer.

    Several properties of this format influence how
    the \l duration() or \l byteCount() are calculated
    from the \l frameCount().
 */
QAudioFormat QAudioBuffer::format() const
{
    if (!isValid())
        return QAudioFormat();
    return d->mProvider->format();
}

/*!
    Returns the number of complete audio frames in this buffer.

    An audio frame is an interleaved set of one sample per channel
    for the same instant in time.
*/
int QAudioBuffer::frameCount() const
{
    if (!isValid())
        return 0;
    return d->mProvider->frameCount();
}

/*!
    Returns the number of samples in this buffer.

    If the format of this buffer has multiple channels,
    then this count includes all channels.  This means
    that a stereo buffer with 1000 samples in total will
    have 500 left samples and 500 right samples (interleaved),
    and this function will return 1000.

    \sa frameCount()
*/
int QAudioBuffer::sampleCount() const
{
    if (!isValid())
        return 0;

    return frameCount() * format().channelCount();
}

/*!
    Returns the size of this buffer, in bytes.
 */
int QAudioBuffer::byteCount() const
{
    const QAudioFormat f(format());
    return format().bytesForFrames(frameCount());
}

/*!
    Returns the duration of audio in this buffer, in microseconds.

    This depends on the /l format(), and the \l frameCount().
*/
qint64 QAudioBuffer::duration() const
{
    return format().durationForFrames(frameCount());
}

/*!
    Returns the time in a stream that this buffer starts at (in microseconds).

    If this buffer is not part of a stream, this will return -1.
 */
qint64 QAudioBuffer::startTime() const
{
    if (!isValid())
        return -1;
    return d->mProvider->startTime();
}

/*!
    Returns a pointer to this buffer's data.  You can only read it.

    This method is preferred over the const version of \l data() to
    prevent unnecessary copying.

    There is also a templatized version of this constData() function that
    allows you to retrieve a specific type of read-only pointer to
    the data.  Note that there is no checking done on the format of
    the audio buffer - this is simply a convenience function.

    \code
    // With a 16bit sample buffer:
    const quint16 *data = buffer->constData<quint16>();
    \endcode

*/
const void* QAudioBuffer::constData() const
{
    if (!isValid())
        return 0;
    return d->mProvider->constData();
}

/*!
    Returns a pointer to this buffer's data.  You can only read it.

    You should use the \l constData() function rather than this
    to prevent accidental deep copying.

    There is also a templatized version of this data() function that
    allows you to retrieve a specific type of read-only pointer to
    the data.  Note that there is no checking done on the format of
    the audio buffer - this is simply a convenience function.

    \code
    // With a 16bit sample const buffer:
    const quint16 *data = buffer->data<quint16>();
    \endcode
*/
const void* QAudioBuffer::data() const
{
    if (!isValid())
        return 0;
    return d->mProvider->constData();
}


/*
    Template data/constData functions caused override problems with qdoc,
    so moved their docs into the non template versions.
*/

/*!
    Returns a pointer to this buffer's data.  You can modify the
    data through the returned pointer.

    Since QAudioBuffers can share the actual sample data, calling
    this function will result in a deep copy being made if there
    are any other buffers using the sample.  You should avoid calling
    this unless you really need to modify the data.

    This pointer will remain valid until the underlying storage is
    detached.  In particular, if you obtain a pointer, and then
    copy this audio buffer, changing data through this pointer may
    change both buffer instances.  Calling \l data() on either instance
    will again cause a deep copy to be made, which may invalidate
    the pointers returned from this function previously.

    There is also a templatized version of data() allows you to retrieve
    a specific type of pointer to the data.  Note that there is no
    checking done on the format of the audio buffer - this is
    simply a convenience function.

    \code
    // With a 16bit sample buffer:
    quint16 *data = buffer->data<quint16>(); // May cause deep copy
    \endcode
*/
void *QAudioBuffer::data()
{
    if (!isValid())
        return 0;

    if (d->mCount.load() != 1) {
        // Can't share a writable buffer
        // so we need to detach
        QAudioBufferPrivate *newd = d->clone();

        // This shouldn't happen
        if (!newd)
            return 0;

        d->deref();
        d = newd;
    }

    // We're (now) the only user of this qaab, so
    // see if it's writable directly
    void *buffer = d->mProvider->writableData();
    if (buffer) {
        return buffer;
    }

    // Wasn't writable, so turn it into a memory provider
    QAbstractAudioBuffer *memBuffer = new QMemoryAudioBufferProvider(constData(), frameCount(), format(), startTime());

    if (memBuffer) {
        d->mProvider->release();
        d->mCount.store(1);
        d->mProvider = memBuffer;

        return memBuffer->writableData();
    }

    return 0;
}

// Template helper classes worth documenting

/*!
    \class QAudioBuffer::StereoFrameDefault
    \internal

    Just a trait class for the default value.
*/

/*!
    \class QAudioBuffer::StereoFrame
    \brief The StereoFrame class provides a simple wrapper for a stereo audio frame.
    \inmodule QtMultimedia
    \ingroup multimedia
    \ingroup multimedia_audio

    This templatized structure lets you treat a block of individual samples as an
    interleaved stereo stream frame.  This is most useful when used with the templatized
    \l {QAudioBuffer::data()}{data()} functions of QAudioBuffer.  Generally the data
    is accessed as a pointer, so no copying should occur.

    There are some predefined instantiations of this template for working with common
    stereo sample depths in a convenient way.

    This frame structure has \e left and \e right members for accessing individual channel data.

    For example:
    \code
    // Assuming 'buffer' is an unsigned 16 bit stereo buffer..
    QAudioBuffer::S16U *frames = buffer->data<QAudioBuffer::S16U>();
    for (int i=0; i < buffer->frameCount(); i++) {
        qSwap(frames[i].left, frames[i].right);
    }
    \endcode

    \sa QAudioBuffer::S8U, QAudioBuffer::S8S, QAudioBuffer::S16S, QAudioBuffer::S16U, QAudioBuffer::S32F
*/

/*!
    \fn QAudioBuffer::StereoFrame::StereoFrame()

    Constructs a new frame with the "silent" value for this
    sample format (0 for signed formats and floats, 0x8* for unsigned formats).
*/

/*!
    \fn QAudioBuffer::StereoFrame::StereoFrame(T leftSample, T rightSample)

    Constructs a new frame with the supplied \a leftSample and \a rightSample values.
*/

/*!
    \fn QAudioBuffer::StereoFrame::operator=(const StereoFrame &other)

    Assigns \a other to this frame.
 */


/*!
    \fn QAudioBuffer::StereoFrame::average() const

    Returns the arithmetic average of the left and right samples.
 */

/*! \fn QAudioBuffer::StereoFrame::clear()

    Sets the values of this frame to the "silent" value.
*/

/*!
    \variable QAudioBuffer::StereoFrame::left
    \brief the left sample
*/

/*!
    \variable QAudioBuffer::StereoFrame::right
    \brief the right sample
*/

/*!
    \typedef QAudioBuffer::S8U
    \relates QAudioBuffer::StereoFrame

    This is a predefined specialization for an unsigned stereo 8 bit sample.  Each
    channel is an \e {unsigned char}.
*/
/*!
    \typedef QAudioBuffer::S8S
    \relates QAudioBuffer::StereoFrame

    This is a predefined specialization for a signed stereo 8 bit sample.  Each
    channel is a \e {signed char}.
*/
/*!
    \typedef QAudioBuffer::S16U
    \relates QAudioBuffer::StereoFrame

    This is a predefined specialization for an unsigned stereo 16 bit sample.  Each
    channel is an \e {unsigned short}.
*/
/*!
    \typedef QAudioBuffer::S16S
    \relates QAudioBuffer::StereoFrame

    This is a predefined specialization for a signed stereo 16 bit sample.  Each
    channel is a \e {signed short}.
*/
/*!
    \typedef QAudioBuffer::S32F
    \relates QAudioBuffer::StereoFrame

    This is a predefined specialization for an 32 bit float sample.  Each
    channel is a \e float.
*/

QT_END_NAMESPACE

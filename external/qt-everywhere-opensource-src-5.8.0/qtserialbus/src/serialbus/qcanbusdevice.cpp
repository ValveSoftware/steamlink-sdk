/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtSerialBus module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qcanbusdevice.h"
#include "qcanbusdevice_p.h"

#include "qcanbusframe.h"

#include <QtCore/qdebug.h>
#include <QtCore/qdatastream.h>
#include <QtCore/qeventloop.h>
#include <QtCore/qscopedvaluerollback.h>
#include <QtCore/qtimer.h>

QT_BEGIN_NAMESPACE

/*!
    \class QCanBusDevice
    \inmodule QtSerialBus
    \since 5.8

    \brief The QCanBusDevice class is the interface class for CAN bus.

    QCanBusDevice communicates with a CAN plugin providing users with a convenient API.
    The CAN plugin must be specified during the object creation.
*/

/*!
    \enum QCanBusDevice::CanBusError
    This enum describes all the possible error conditions.

    \value NoError              No errors have occurred.
    \value ReadError            An error occurred during a read operation.
    \value WriteError           An error occurred during a write operation.
    \value ConnectionError      An error occurred when attempting to open the plugin.
    \value ConfigurationError   An error occurred when attempting to set a configuration
                                parameter.
    \value UnknownError         An unknown error occurred.
*/

/*!
    \enum QCanBusDevice::CanBusDeviceState
    This enum describes all possible device states.

    \value UnconnectedState The device is disconnected.
    \value ConnectingState  The device is being connected.
    \value ConnectedState   The device is connected to the CAN bus.
    \value ClosingState     The device is being closed.
*/

/*!
    \enum QCanBusDevice::ConfigurationKey
    This enum describes the possible configuration options for
    the CAN bus connection.

    \value RawFilterKey     This configuration determines the type of CAN bus frames
                            that the current device accepts. The expected value
                            is \c QList<QCanBusDevice::Filter>. Passing an empty list clears
                            all previously set filters including default filters. For more details
                            see \l QCanBusDevice::Filter.
    \value ErrorFilterKey   This key defines the type of error that should be
                            forwarded via the current connection. The associated
                            value should be of type \l QCanBusFrame::FrameErrors.
    \value LoopbackKey      This key defines whether the CAN bus device should operate in loopback
                            mode. Loopback means, whenever a CAN frame is transmitted on the CAN
                            bus, a local echo of this frame is sent to all applications connected to
                            this CAN device. The expected value for this key is \c bool.
    \value ReceiveOwnKey    This key defines whether this CAN device receives its own send frames.
                            This can be used to check if the transmission was successful.
                            The expected value for this key is \c bool.
    \value BitRateKey       This key defines the bitrate in bits per second.
    \value CanFdKey         This key defines whether sending and receiving of CAN FD frames
                            should be enabled. The expected value for this key is \c bool.
    \value UserKey          This key defines the range where custom keys start. Its most
                            common purpose is to permit platform-specific configuration
                            options.

    \sa configurationParameter()
*/

/*!
    \class QCanBusDevice::Filter
    \inmodule QtSerialBus
    \since 5.8

    \brief The QCanBusDevice::Filter struct defines a filter for CAN bus frames.

    A list of QCanBusDevice::Filter instances is passed to
    \l QCanBusDevice::setConfigurationParameter() to enable filtering. If a received CAN frame
    matches at least one of the filters in the list, the QCanBusDevice will accept it.

    The example below demonstrates how to use the struct:

    \snippet snippetmain.cpp Filter Examples
*/

/*!
    \enum QCanBusDevice::Filter::FormatFilter
    This enum describes the format pattern, which is used to filter incoming
    CAN bus frames.

    \value MatchBaseFormat              The CAN bus frame must use the base frame format
                                        (11 bit identifier).
    \value MatchExtendedFormat          The CAN bus frame must use the extended frame format
                                        (29 bit identifier).
    \value MatchBaseAndExtendedFormat   The CAN bus frame can have a base or an extended
                                        frame format.
*/

/*!
    \variable QCanBusDevice::Filter::frameId

    \brief The frame id used to filter the incoming frames.

    The frameId is used in conjunction with \a frameIdMask.
    The matching is successful if the following evaluates to \c true:

    \code
        (receivedFrameId & frameIdMask) == (frameId & frameIdMask)
    \endcode

    By default this field is set to \c 0x0.

    \sa frameIdMask
*/

/*!
    \variable QCanBusDevice::Filter::frameIdMask

    \brief The bit mask that is applied to the frame id of the filter and the received frame.

    The two frame ids are matching if the following evaluates to \c true:

    \code
        (receivedFrameId & frameIdMask) == (frameId & frameIdMask)
    \endcode

    By default this field is set to \c 0x0.

    \sa frameId
*/

/*!
    \variable QCanBusDevice::Filter::type

    \brief The type of the frame to be filtered.

    Any CAN bus frame type can be matched by setting this variable
    to \l QCanBusFrame::InvalidFrame. The filter object is invalid if
    type is equal to \l QCanBusFrame::UnknownFrame.

    By default this field is set to \l QCanBusFrame::InvalidFrame.

    \sa QCanBusFrame::FrameType
*/

/*!
    \variable QCanBusDevice::Filter::format

    \brief The frame format of the matching CAN bus frame.

    By default this field is set to \l QCanBusDevice::Filter::MatchBaseAndExtendedFormat.
*/

/*!
    \fn QCanBusDevice::errorOccurred(CanBusError error)

    This signal is emitted when an error of the type \a error occurs.
*/

/*!
    Constructs a serial bus device with the specified \a parent.
*/
QCanBusDevice::QCanBusDevice(QObject *parent) :
    QObject(*new QCanBusDevicePrivate, parent)
{
}


/*!
    Sets the human readable description of the last device error to
    \a errorText. \a errorId categorizes the type of error.

    CAN bus implementations must use this function to update the device's
    error state.

    \sa error(), errorOccurred()
*/
void QCanBusDevice::setError(const QString &errorText, CanBusError errorId)
{
    Q_D(QCanBusDevice);

    d->errorText = errorText;
    d->lastError = errorId;

    emit errorOccurred(errorId);
}

/*!
    Appends \a newFrames to the internal list of frames which can be
    accessed using \l readFrame() and emits the \l framesReceived()
    signal.

    Subclasses must call this function when they receive frames.

*/
void QCanBusDevice::enqueueReceivedFrames(const QVector<QCanBusFrame> &newFrames)
{
    Q_D(QCanBusDevice);

    if (newFrames.isEmpty())
        return;

    d->incomingFramesGuard.lock();
    d->incomingFrames.append(newFrames);
    d->incomingFramesGuard.unlock();
    emit framesReceived();
}

/*!
    Appends \a newFrame to the internal list of outgoing frames which
    can be accessed by \l writeFrame().

    Subclasses must call this function when they write a new frame.
*/
void QCanBusDevice::enqueueOutgoingFrame(const QCanBusFrame &newFrame)
{
    Q_D(QCanBusDevice);

    d->outgoingFrames.append(newFrame);
}

/*!
    Returns the next \l QCanBusFrame from the internal list of outgoing frames;
    otherwise returns an invalid QCanBusFrame. The returned frame is removed
    from the internal list.
*/
QCanBusFrame QCanBusDevice::dequeueOutgoingFrame()
{
    Q_D(QCanBusDevice);

    if (d->outgoingFrames.isEmpty())
        return QCanBusFrame(QCanBusFrame::InvalidFrame);
    return d->outgoingFrames.takeFirst();
}

/*!
    Returns \c true if the internal list of outgoing frames is not
    empty; otherwise returns \c false.
*/
bool QCanBusDevice::hasOutgoingFrames() const
{
    Q_D(const QCanBusDevice);

    return !d->outgoingFrames.isEmpty();
}

/*!
    Sets the configuration parameter \a key for the CAN bus connection
    to \a value. The potential keys are represented by \l ConfigurationKey.

    A parameter can be unset by setting an invalid \l QVariant.
    Unsetting a parameter implies that the configuration is reset to
    its default setting.

    \note In most cases, configuration changes only take effect
    after a reconnect.

    \sa configurationParameter()
*/
void QCanBusDevice::setConfigurationParameter(int key, const QVariant &value)
{
    Q_D(QCanBusDevice);

    for (int i = 0; i < d->configOptions.size(); i++) {
        if (d->configOptions.at(i).first == key) {
            if (value.isValid()) {
                ConfigEntry entry = d->configOptions.at(i);
                entry.second = value;
                d->configOptions.replace(i, entry);
            } else {
                d->configOptions.remove(i);
            }
            return;
        }
    }

    if (!value.isValid())
        return;

    ConfigEntry newEntry(key, value);
    d->configOptions.append(newEntry);
}

/*!
    Returns the current value assigned to the \l ConfigurationKey \a key; otherwise
    an invalid \l QVariant.

    \sa setConfigurationParameter(), configurationKeys()
*/
QVariant QCanBusDevice::configurationParameter(int key) const
{
    Q_D(const QCanBusDevice);

    foreach (const ConfigEntry &e, d->configOptions) {
        if (e.first == key)
            return e.second;
    }

    return QVariant();
}

/*!
    Returns the list of keys used by the CAN bus connection.

    The the meaning of the keys is equivalent to \l ConfigurationKey.
    If a key is not explicitly mentioned the platform's
    default setting for the relevant key is used.
*/
QVector<int> QCanBusDevice::configurationKeys() const
{
    Q_D(const QCanBusDevice);

    QVector<int> result;
    foreach (const ConfigEntry &e, d->configOptions)
        result.append(e.first);

    return result;
}

/*!
    Returns the last error that has occurred. The error value is always set to last error that
    occurred and it is never reset.

    \sa errorString()
*/
QCanBusDevice::CanBusError QCanBusDevice::error() const
{
    return d_func()->lastError;
}

/*!
    Returns a human-readable description of the last device error that occurred.

    \sa error()
*/
QString QCanBusDevice::errorString() const
{
    Q_D(const QCanBusDevice);

    if (d->lastError == QCanBusDevice::NoError)
        return QString();

    return d->errorText;
}

/*!
    Returns the number of available frames. If no frames are available,
    this function returns 0.

    \sa readFrame()
*/
qint64 QCanBusDevice::framesAvailable() const
{
    return d_func()->incomingFrames.size();
}

/*!
    Returns the number of frames waiting to be written.

    \sa writeFrame()
*/
qint64 QCanBusDevice::framesToWrite() const
{
    return d_func()->outgoingFrames.size();
}

/*!
    For buffered devices, this function waits until all buffered frames
    have been written to the device and the \l framesWritten() signal has been emitted,
    or until \a msecs milliseconds have passed. If \a msecs is -1,
    this function will not time out. For unbuffered devices, it returns immediately with \c false
    as \l writeFrame() does not require a write buffer.

    Returns \c true if the \l framesWritten() signal is emitted;
    otherwise returns \c false (i.e. if the operation timed out, or if an error occurred).

    \note This function will start a local event loop. This may lead to scenarios whereby
    other application slots may be called while the execution of this function scope is blocking.
    To avoid problems, the signals for this class should not be connected to slots.
    Similarly this function must never be called in response to the \l framesWritten()
    or \l errorOccurred() signals.

    \sa waitForFramesReceived()
 */
bool QCanBusDevice::waitForFramesWritten(int msecs)
{
    // do not enter this function recursively
    if (d_func()->waitForWrittenEntered) {
        qWarning("QCanBusDevice::waitForFramesWritten() must not be called "
                 "recursively. Check that no slot containing waitForFramesReceived() "
                 "is called in response to framesWritten(qint64) or errorOccurred(CanBusError)"
                 "signals\n");
        return false;
    }

    QScopedValueRollback<bool> guard(d_func()->waitForWrittenEntered);
    d_func()->waitForWrittenEntered = true;

    if (d_func()->state != ConnectedState)
        return false;

    if (!framesToWrite())
        return false; // nothing pending, nothing to wait upon

    QEventLoop loop;
    connect(this, &QCanBusDevice::framesWritten, &loop, [&]() { loop.exit(0); });
    connect(this, &QCanBusDevice::errorOccurred, &loop, [&]() { loop.exit(1); });
    if (msecs >= 0)
        QTimer::singleShot(msecs, &loop, [&]() { loop.exit(2); });

    int result = 0;
    while (framesToWrite() > 0) {
        // wait till all written or time out
        result = loop.exec(QEventLoop::ExcludeUserInputEvents);
        if (result > 0)
            return false;
    }
    return true;
}

/*!
    Blocks until new frames are available for reading and the \l framesReceived()
    signal has been emitted, or until \a msecs milliseconds have passed. If
    \a msecs is \c -1, this function will not time out.

    Returns \c true if new frames are available for reading and the \l framesReceived()
    signal is emitted; otherwise returns \c false (if the operation timed out
    or if an error occurred).

    \note This function will start a local event loop. This may lead to scenarios whereby
    other application slots may be called while the execution of this function scope is blocking.
    To avoid problems, the signals for this class should not be connected to slots.
    Similarly this function must never be called in response to the \l framesReceived()
    or \l errorOccurred() signals.

    \sa waitForFramesWritten()
 */
bool QCanBusDevice::waitForFramesReceived(int msecs)
{
    // do not enter this function recursively
    if (d_func()->waitForReceivedEntered) {
        qWarning("QCanBusDevice::waitForFramesReceived() must not be called "
                 "recursively. Check that no slot containing waitForFramesReceived() "
                 "is called in response to framesReceived() or errorOccurred(CanBusError) "
                 "signals\n");
        return false;
    }

    QScopedValueRollback<bool> guard(d_func()->waitForReceivedEntered);
    d_func()->waitForReceivedEntered = true;

    if (d_func()->state != ConnectedState)
        return false;

    QEventLoop loop;

    connect(this, &QCanBusDevice::framesReceived, &loop, [&]() { loop.exit(0); });
    connect(this, &QCanBusDevice::errorOccurred, &loop, [&]() { loop.exit(1); });
    if (msecs >= 0)
        QTimer::singleShot(msecs, &loop, [&]() { loop.exit(2); });

    int result = loop.exec(QEventLoop::ExcludeUserInputEvents);

    return result == 0;
}

/*!
    \fn bool QCanBusDevice::open()

    This function is called by connectDevice(). Subclasses must provide
    an implementation which returns \c true if the CAN bus connection
    could be established; otherwise \c false. The QCanBusDevice implementation
    ensures upon entry of this function that the device's \l state() is set
    to \l QCanBusDevice::ConnectingState already.

    The implementation must ensure that upon success the instance's \l state()
    is set to \l QCanBusDevice::ConnectedState; otherwise
    \l QCanBusDevice::UnconnectedState. \l setState() must be used to set the new
    device state.

    The custom implementation is responsible for opening the socket, instanciation
    of a potentially required \l QSocketNotifier and the application of custom and default
    \l QCanBusDevice::configurationParameter().

    \sa connectDevice()
*/

/*!
    \fn void QCanBusDevice::close()

    This function is responsible for closing the CAN bus connection.
    The implementation must ensure that the instance's
    \l state() is set to \l QCanBusDevice::UnconnectedState.

    This function's most important task is to close the socket to the CAN device
    and to call \l QCanBusDevice::setState().

    \sa disconnectDevice()
*/

/*!
    \fn void QCanBusDevice::framesReceived()

    This signal is emitted when one or more frames have been received.
    The frames should be read using \l readFrame() and \l framesAvailable().
*/

/*!
    Returns the next \l QCanBusFrame from the queue; otherwise returns
    an empty QCanBusFrame. The returned frame is removed from the queue.

    The queue operates according to the FIFO principle.

    \sa framesAvailable()
*/
QCanBusFrame QCanBusDevice::readFrame()
{
    Q_D(QCanBusDevice);

    if (d->state != ConnectedState)
        return QCanBusFrame(QCanBusFrame::InvalidFrame);

    QMutexLocker locker(&d->incomingFramesGuard);

    if (d->incomingFrames.isEmpty())
        return QCanBusFrame(QCanBusFrame::InvalidFrame);

    return d->incomingFrames.takeFirst();
}

/*!
    \fn void QCanBusDevice::framesWritten(qint64 framesCount)

    This signal is emitted every time a payload of frames has been
    written to the CAN bus. The \a framesCount argument is set to
    the number of frames that were written in this payload.
*/

/*!
    \fn bool QCanBusDevice::writeFrame(const QCanBusFrame &frame)

    Writes \a frame to the CAN bus and returns \c true on success;
    otherwise \c false.

    On some platforms, the frame may be put into a queue and the return
    value may only indicate a successful insertion into the queue.
    The actual frame will be send later on. Therefore the \l framesWritten()
    signal is the final confirmation that the frame has been handed off to
    the transport layer. If an error occurs the \l errorOccurred() is emitted.

    As per CAN bus specification, frames of type
    \l {QCanBusFrame::RemoteRequestFrame} {remote transfer request (RTR)}
    do not have a payload, but a length from 0 to 8 (including). This length indicates
    the expected response payload length from the remote party. Therefore when sending a RTR frame using
    this function it may still be required to set an arbitrary payload on \a frame. The length of
    the arbitrary payload is what is set as size expectation for the RTR frame.

    \sa QCanBusFrame::setPayload()
*/

/*!
    \fn QString interpretErrorFrame(const QCanBusFrame &frame)

    Interprets \a frame as error frame and returns a human readable
    description of the error.

    If \a frame is not an error frame, the returned string is empty.
*/

/*!
    Connects the device to the CAN bus. Returns \c true on success;
    otherwise \c false.

    This function calls \l open() as part of its implementation.
*/
bool QCanBusDevice::connectDevice()
{
    Q_D(QCanBusDevice);

    if (d->state != QCanBusDevice::UnconnectedState) {
        setError(tr("Can not connect an already connected device"),
                 QCanBusDevice::ConnectionError);
        return false;
    }

    setState(ConnectingState);

    if (!open()) {
        setState(UnconnectedState);
        return false;
    }

    //Connected is set by backend -> might be delayed by event loop
    return true;
}


/*!
    Disconnects the device from the CAN bus.

    This function calls \l close() as part of its implementation.
*/
void QCanBusDevice::disconnectDevice()
{
    Q_D(QCanBusDevice);

    if (d->state == QCanBusDevice::UnconnectedState
            || d->state == QCanBusDevice::ClosingState) {
        qWarning("Can not disconnect an unconnected device");
        return;
    }

    setState(QCanBusDevice::ClosingState);

    //Unconnected is set by backend -> might be delayed by event loop
    close();
}

/*!
    \fn void QCanBusDevice::stateChanged(QCanBusDevice::CanBusDeviceState state)

    This signal is emitted every time the state of the device changes.
    The new state is represented by \a state.

    \sa setState(), state()
*/

/*!
    Returns the current state of the device.

    \sa setState(), stateChanged()
*/
QCanBusDevice::CanBusDeviceState QCanBusDevice::state() const
{
    return d_func()->state;
}

/*!
    Sets the state of the device to \a newState. CAN bus implementations
    must use this function to update the device state.
*/
void QCanBusDevice::setState(QCanBusDevice::CanBusDeviceState newState)
{
    Q_D(QCanBusDevice);

    if (newState == d->state)
        return;

    d->state = newState;
    emit stateChanged(newState);
}

QT_END_NAMESPACE

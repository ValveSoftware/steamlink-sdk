/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtLocation module of the Qt Toolkit.
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

#include "qgeotiledmapreply_p.h"
#include "qgeotiledmapreply_p_p.h"

#include <qdebug.h>

QT_BEGIN_NAMESPACE
/*!
    \class QGeoTiledMapReply
    \inmodule QtLocation
    \ingroup QtLocation-impl
    \since 5.6
    \internal

    \brief The QGeoTiledMapReply class manages a tile fetch operation started
    by an instance of QGeoTiledManagerEngine.

    Instances of QGeoTiledMapReply manage the state and results of these
    operations.

    The isFinished(), error() and errorString() methods provide information
    on whether the operation has completed and if it completed successfully.

    The finished() and error(QGeoTiledMapReply::Error,QString)
    signals can be used to monitor the progress of the operation.

    It is possible that a newly created QGeoTiledMapReply may be in a finished
    state, most commonly because an error has occurred. Since such an instance
    will never emit the finished() or
    error(QGeoTiledMapReply::Error,QString) signals, it is
    important to check the result of isFinished() before making the connections
    to the signals.

    If the operation completes successfully the results are accessed by
    mapImageData() and mapImageFormat().
*/

/*!
    \enum QGeoTiledMapReply::Error

    Describes an error which prevented the completion of the operation.

    \value NoError
        No error has occurred.
    \value CommunicationError
        An error occurred while communicating with the service provider.
    \value ParseError
        The response from the service provider was in an unrecognizable format
        supported by the service provider.
    \value UnknownError
        An error occurred which does not fit into any of the other categories.
*/

/*!
    Constructs a tiled map reply object based on \a request,  with parent \a parent.
*/
QGeoTiledMapReply::QGeoTiledMapReply(const QGeoTileSpec &spec, QObject *parent)
    : QObject(parent),
      d_ptr(new QGeoTiledMapReplyPrivate(spec))
{
}

/*!
    Constructs a tiled map reply object with a given \a error and \a errorString and the specified \a parent.
*/
QGeoTiledMapReply::QGeoTiledMapReply(Error error, const QString &errorString, QObject *parent)
    : QObject(parent),
      d_ptr(new QGeoTiledMapReplyPrivate(error, errorString)) {}

/*!
    Destroys this tiled map reply object.
*/
QGeoTiledMapReply::~QGeoTiledMapReply()
{
    delete d_ptr;
}

/*!
    Sets whether or not this reply has finished to \a finished.

    If \a finished is true, this will cause the finished() signal to be
    emitted.

    If the operation completed successfully,
    QGeoTiledMapReply::setMapImageData() should be called before this
    function. If an error occurred, QGeoTiledMapReply::setError() should be used
    instead.
*/
void QGeoTiledMapReply::setFinished(bool finished)
{
    d_ptr->isFinished = finished;
    if (d_ptr->isFinished)
        emit this->finished();
}

/*!
    Return true if the operation completed successfully or encountered an
    error which cause the operation to come to a halt.
*/
bool QGeoTiledMapReply::isFinished() const
{
    return d_ptr->isFinished;
}

/*!
    Sets the error state of this reply to \a error and the textual
    representation of the error to \a errorString.

    This will also cause error() and finished() signals to be emitted, in that
    order.
*/
void QGeoTiledMapReply::setError(QGeoTiledMapReply::Error error, const QString &errorString)
{
    d_ptr->error = error;
    d_ptr->errorString = errorString;
    emit this->error(error, errorString);
    setFinished(true);
}

/*!
    Returns the error state of this reply.

    If the result is QGeoTiledMapReply::NoError then no error has occurred.
*/
QGeoTiledMapReply::Error QGeoTiledMapReply::error() const
{
    return d_ptr->error;
}

/*!
    Returns the textual representation of the error state of this reply.

    If no error has occurred this will return an empty string.  It is possible
    that an error occurred which has no associated textual representation, in
    which case this will also return an empty string.

    To determine whether an error has occurred, check to see if
    QGeoTiledMapReply::error() is equal to QGeoTiledMapReply::NoError.
*/
QString QGeoTiledMapReply::errorString() const
{
    return d_ptr->errorString;
}

/*!
    Returns whether the reply is coming from a cache.
*/
bool QGeoTiledMapReply::isCached() const
{
    return d_ptr->isCached;
}

/*!
    Sets whether the reply is coming from a cache to \a cached.
*/
void QGeoTiledMapReply::setCached(bool cached)
{
    d_ptr->isCached = cached;
}

/*!
    Returns the request which corresponds to this reply.
*/
QGeoTileSpec QGeoTiledMapReply::tileSpec() const
{
    return d_ptr->spec;
}

/*!
    Returns the tile image data.
*/
QByteArray QGeoTiledMapReply::mapImageData() const
{
    return d_ptr->mapImageData;
}

/*!
    Sets the tile image data to \a data.
*/
void QGeoTiledMapReply::setMapImageData(const QByteArray &data)
{
    d_ptr->mapImageData = data;
}

/*!
    Returns the format of the tile image.
*/
QString QGeoTiledMapReply::mapImageFormat() const
{
    return d_ptr->mapImageFormat;
}

/*!
    Sets the format of the tile image to \a format.
*/
void QGeoTiledMapReply::setMapImageFormat(const QString &format)
{
    d_ptr->mapImageFormat = format;
}

/*!
    Cancels the operation immediately.

    This will do nothing if the reply is finished.
*/
void QGeoTiledMapReply::abort()
{
    if (!isFinished())
        setFinished(true);
}

/*
    \fn void QGeoTiledMapReply::finished()

    This signal is emitted when this reply has finished processing.

    If error() equals QGeoTiledMapReply::NoError then the processing
    finished successfully.

    This signal and QGeoRoutingManager::finished() will be
    emitted at the same time.

    \note Do not delete this reply object in the slot connected to this
    signal. Use deleteLater() instead.

    \fn void QGeoTiledMapReply::error(QGeoTiledMapReply::Error error, const QString &errorString)

    This signal is emitted when an error has been detected in the processing of
    this reply. The finished() signal will probably follow.

    The error will be described by the error code \a error. If \a errorString is
    not empty it will contain a textual description of the error.

    This signal and QGeoRoutingManager::error() will be emitted at the same time.

    \note Do not delete this reply object in the slot connected to this
    signal. Use deleteLater() instead.
*/

/*!
    \fn void QGeoTiledMapReply::finished()

    This signal is emitted when this reply has finished processing.

    If error() equals QGeoTiledMapReply::NoError then the processing
    finished successfully.

    \note Do not delete this reply object in the slot connected to this
    signal. Use deleteLater() instead.
*/
/*!
    \fn void QGeoTiledMapReply::error(QGeoTiledMapReply::Error error, const QString &errorString)

    This signal is emitted when an error has been detected in the processing of
    this reply. The finished() signal will probably follow.

    The error will be described by the error code \a error. If \a errorString is
    not empty it will contain a textual description of the error.

    \note Do not delete this reply object in the slot connected to this
    signal. Use deleteLater() instead.
*/

/*******************************************************************************
*******************************************************************************/

QGeoTiledMapReplyPrivate::QGeoTiledMapReplyPrivate(const QGeoTileSpec &spec)
    : error(QGeoTiledMapReply::NoError),
      isFinished(false),
      isCached(false),
      spec(spec) {}

QGeoTiledMapReplyPrivate::QGeoTiledMapReplyPrivate(QGeoTiledMapReply::Error error, const QString &errorString)
    : error(error),
      errorString(errorString),
      isFinished(true),
      isCached(false) {}

QGeoTiledMapReplyPrivate::~QGeoTiledMapReplyPrivate() {}

#include "moc_qgeotiledmapreply_p.cpp"

QT_END_NAMESPACE

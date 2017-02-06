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

#include "qplacereply.h"
#include "qplacereply_p.h"

QT_USE_NAMESPACE

/*!
    \class QPlaceReply
    \inmodule QtLocation
    \ingroup QtLocation-places
    \ingroup QtLocation-places-replies
    \since 5.6

    \brief The QPlaceReply class manages an operation started by an instance of QPlaceManager and
           serves as a base class for more specialized replies.

    The QPlaceReply and each of its specialized subclasses manage the
    state and results of their corresponding operations.  The QPlaceReply itself is used
    for operations that have no results, that is, it only necessary to know if the operation
    succeeded or failed.

    The finished() signal can be used to monitor the progress of an operation.
    Once an operation is complete, the error() and errorString() methods provide information
    on whether the operation completed successfully.  If successful, the reply
    will contain the results for that operation, that is, each subclass will have appropriate
    functions to retrieve the results of an operation.

    \sa QPlaceManager
*/

/*!
    \enum QPlaceReply::Error

    Describes an error which occurred during an operation.
    \value NoError
        No error has occurred
    \value PlaceDoesNotExistError
        A specified place could not be found
    \value CategoryDoesNotExistError
        A specified category could not be found
    \value CommunicationError
        An error occurred communicating with the service provider.
    \value ParseError
        The response from the service provider or an import file was in an unrecognizable format
    \value PermissionsError
        The operation failed because of insufficient permissions.
    \value UnsupportedError
        The operation was not supported by the service provider.
    \value BadArgumentError.
        A parameter that was provided was invalid.
    \value CancelError
        The operation was canceled.
    \value UnknownError
        An error occurred which does not fit into any of the other categories.
*/

/*!
    \enum QPlaceReply::Type

    Describes the reply's type.
    \value Reply
        This is a generic reply.
    \value DetailsReply
        This is a reply for the retrieval of place details
    \value SearchReply
        This is a reply for the place search operation.
    \value SearchSuggestionReply
        This is a reply for a search suggestion operation.
    \value ContentReply
        This is a reply for content associated with a place.
    \value IdReply
        This is a reply that returns an identifier of a place or category.
        Typically used for place or category save and remove operations.
    \value MatchReply
        This is a reply that returns places that match
        those from another provider.
*/

/*!
    Constructs a reply object with a given \a parent.
*/
QPlaceReply::QPlaceReply(QObject *parent)
    : QObject(parent),d_ptr(new QPlaceReplyPrivate)
{
}

/*!
    \internal
*/
QPlaceReply::QPlaceReply(QPlaceReplyPrivate *dd, QObject *parent)
    : QObject(parent),d_ptr(dd)
{
}

/*!
    Destroys the reply object.
*/
QPlaceReply::~QPlaceReply()
{
    if (!isFinished()) {
        abort();
    }
    delete d_ptr;
}

/*!
    Return true if the reply has completed.
*/
bool QPlaceReply::isFinished() const
{
    return d_ptr->isFinished;
}

/*!
    Returns the type of the reply.
*/
QPlaceReply::Type QPlaceReply::type() const
{
    return QPlaceReply::Reply;
}

/*!
    Sets the status of whether the reply is \a finished
    or not.  This function does not cause the finished() signal
    to be emitted.
*/
void QPlaceReply::setFinished(bool finished)
{
    d_ptr->isFinished = finished;
}

/*!
    Sets the \a error and \a errorString of the reply.
    This function does not cause the QPlaceReply::error(QPlaceReply::Error, const QString &errorString)
    signal to be emitted.
*/
void QPlaceReply::setError(QPlaceReply::Error error, const QString &errorString)
{
    d_ptr->error = error;
    d_ptr->errorString = errorString;
}

/*!
    Returns the error string of the reply.  The error string is intended to be
    used by developers only and is not fit to be displayed to an end user.

    If no error has occurred, the string is empty.
*/
QString QPlaceReply::errorString() const
{
    return d_ptr->errorString;
}

/*!
    Returns the error code.
*/
QPlaceReply::Error QPlaceReply::error() const
{
    return d_ptr->error;
}

/*!
    Aborts the operation.
*/
void QPlaceReply::abort()
{
}

/*!
    \fn void QPlaceReply::finished()

    This signal is emitted when this reply has finished processing.

    If error() equals QPlaceReply::NoError then the processing
    finished successfully.

    This signal and QPlaceManager::finished() will be
    emitted at the same time.

    \note Do not delete this reply object in the slot connected to this
    signal. Use deleteLater() instead.
*/

/*!
    \fn void QPlaceReply::error(QPlaceReply::Error error, const QString &errorString)

    This signal is emitted when an error has been detected in the processing of
    this reply. The finished() signal will probably follow.

    The error will be described by the error code \a error. If \a errorString is
    not empty it will contain a textual description of the error meant for
    developers and not end users.

    This signal and QPlaceManager::error() will be emitted at the same time.

    \note Do not delete this reply object in the slot connected to this
    signal. Use deleteLater() instead.
*/

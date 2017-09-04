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

#include "qmediastreamscontrol.h"
#include "qmediacontrol_p.h"

QT_BEGIN_NAMESPACE

static void qRegisterMediaStreamControlMetaTypes()
{
    qRegisterMetaType<QMediaStreamsControl::StreamType>();
}

Q_CONSTRUCTOR_FUNCTION(qRegisterMediaStreamControlMetaTypes)


/*!
    \class QMediaStreamsControl
    \inmodule QtMultimedia


    \ingroup multimedia_control

    \brief The QMediaStreamsControl class provides a media stream selection control.


    The QMediaStreamsControl class provides descriptions of the available media streams
    and allows individual streams to be activated and deactivated.

    The interface name of QMediaStreamsControl is \c org.qt-project.qt.mediastreamscontrol/5.0 as
    defined in QMediaStreamsControl_iid.

    \sa QMediaService::requestControl()
*/

/*!
    \macro QMediaStreamsControl_iid

    \c org.qt-project.qt.mediastreamscontrol/5.0

    Defines the interface name of the QMediaStreamsControl class.

    \relates QMediaStreamsControl
*/

/*!
    Constructs a new media streams control with the given \a parent.
*/
QMediaStreamsControl::QMediaStreamsControl(QObject *parent)
    :QMediaControl(*new QMediaControlPrivate, parent)
{
}

/*!
    Destroys a media streams control.
*/
QMediaStreamsControl::~QMediaStreamsControl()
{
}

/*!
  \enum QMediaStreamsControl::StreamType

  Media stream type.

  \value AudioStream Audio stream.
  \value VideoStream Video stream.
  \value SubPictureStream Subpicture or teletext stream.
  \value UnknownStream The stream type is unknown.
  \value DataStream
*/

/*!
    \fn QMediaStreamsControl::streamCount()

    Returns the number of media streams.
*/

/*!
    \fn QMediaStreamsControl::streamType(int stream)

    Return the type of a media \a stream.
*/

/*!
    \fn QMediaStreamsControl::metaData(int stream, const QString &key)

    Returns the meta-data value of \a key for a given \a stream.

    Useful metadata keys are QMediaMetaData::Title,
    QMediaMetaData::Description and QMediaMetaData::Language.
*/

/*!
    \fn QMediaStreamsControl::isActive(int stream)

    Returns true if the media \a stream is active.
*/

/*!
    \fn QMediaStreamsControl::setActive(int stream, bool state)

    Sets the active \a state of a media \a stream.

    Setting the active state of a media stream to true will activate it.  If any other stream
    of the same type was previously active it will be deactivated. Setting the active state fo a
    media stream to false will deactivate it.
*/

/*!
    \fn QMediaStreamsControl::streamsChanged()

    The signal is emitted when the available streams list is changed.
*/

/*!
    \fn QMediaStreamsControl::activeStreamsChanged()

    The signal is emitted when the active streams list is changed.
*/

#include "moc_qmediastreamscontrol.cpp"
QT_END_NAMESPACE


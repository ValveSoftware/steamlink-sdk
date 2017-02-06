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

#include "qmediaplaylistioplugin_p.h"

QT_BEGIN_NAMESPACE

/*!
    \class QMediaPlaylistReader
    \internal

    \brief The QMediaPlaylistReader class provides an interface for reading a playlist file.
    \inmodule QtMultimedia

    \ingroup multimedia
    \ingroup multimedia_playback

    \sa QMediaPlaylistIOPlugin
*/

/*!
    Destroys a media playlist reader.
*/
QMediaPlaylistReader::~QMediaPlaylistReader()
{
}

/*!
    \fn QMediaPlaylistReader::atEnd() const

    Identifies if a playlist reader has reached the end of its input.

    Returns true if the reader has reached the end; and false otherwise.
*/

/*!
    \fn QMediaPlaylistReader::readItem()

    Reads an item of media from a playlist file.

    Returns the read media, or a null QMediaContent if no more media is available.
*/

/*!
    \fn QMediaPlaylistReader::close()

    Closes a playlist reader's input device.
*/

/*!
    \class QMediaPlaylistWriter
    \internal

    \brief The QMediaPlaylistWriter class provides an interface for writing a playlist file.

    \sa QMediaPlaylistIOPlugin
*/

/*!
    Destroys a media playlist writer.
*/
QMediaPlaylistWriter::~QMediaPlaylistWriter()
{
}

/*!
    \fn QMediaPlaylistWriter::writeItem(const QMediaContent &media)

    Writes an item of \a media to a playlist file.

    Returns true if the media was written successfully; and false otherwise.
*/

/*!
    \fn QMediaPlaylistWriter::close()

    Finalizes the writing of a playlist and closes the output device.
*/

/*!
    \class QMediaPlaylistIOPlugin
    \internal

    \brief The QMediaPlaylistIOPlugin class provides an interface for media playlist I/O plug-ins.
*/

/*!
    Constructs a media playlist I/O plug-in with the given \a parent.
*/
QMediaPlaylistIOPlugin::QMediaPlaylistIOPlugin(QObject *parent)
    :QObject(parent)
{
}

/*!
    Destroys a media playlist I/O plug-in.
*/
QMediaPlaylistIOPlugin::~QMediaPlaylistIOPlugin()
{
}

/*!
    \fn QMediaPlaylistIOPlugin::canRead(QIODevice *device, const QByteArray &format) const

    Identifies if plug-in can read \a format data from an I/O \a device.

    Returns true if the data can be read; and false otherwise.
*/

/*!
    \fn QMediaPlaylistIOPlugin::canRead(const QUrl& location, const QByteArray &format) const

    Identifies if a plug-in can read \a format data from a URL \a location.

    Returns true if the data can be read; and false otherwise.
*/

/*!
    \fn QMediaPlaylistIOPlugin::canWrite(QIODevice *device, const QByteArray &format) const

    Identifies if a plug-in can write \a format data to an I/O \a device.

    Returns true if the data can be written; and false otherwise.
*/

/*!
    \fn QMediaPlaylistIOPlugin::createReader(QIODevice *device, const QByteArray &format)

    Returns a new QMediaPlaylistReader which reads \a format data from an I/O \a device.

    If the device is invalid or the format is unsupported this will return a null pointer.
*/

/*!
    \fn QMediaPlaylistIOPlugin::createReader(const QUrl& location, const QByteArray &format)

    Returns a new QMediaPlaylistReader which reads \a format data from a URL \a location.

    If the location or the format is unsupported this will return a null pointer.
*/

/*!
    \fn QMediaPlaylistIOPlugin::createWriter(QIODevice *device, const QByteArray &format)

    Returns a new QMediaPlaylistWriter which writes \a format data to an I/O \a device.

    If the device is invalid or the format is unsupported this will return a null pointer.
*/

#include "moc_qmediaplaylistioplugin_p.cpp"
QT_END_NAMESPACE


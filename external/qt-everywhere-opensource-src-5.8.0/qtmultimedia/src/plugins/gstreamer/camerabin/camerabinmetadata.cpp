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

#include "camerabinmetadata.h"

#include <QtMultimedia/qmediametadata.h>

#include <gst/gst.h>
#include <gst/gstversion.h>

#include <QDebug>

QT_BEGIN_NAMESPACE

#if GST_CHECK_VERSION(0,10,30)

static QVariant fromGStreamerOrientation(const QVariant &value)
{
    // Note gstreamer tokens either describe the counter clockwise rotation of the
    // image or the clockwise transform to apply to correct the image.  The orientation
    // value returned is the clockwise rotation of the image.
    const QString token = value.toString();
    if (token == QStringLiteral("rotate-90"))
        return 270;
    else if (token == QStringLiteral("rotate-180"))
        return 180;
    else if (token == QStringLiteral("rotate-270"))
        return 90;
    else
        return 0;
}

#endif

static QVariant toGStreamerOrientation(const QVariant &value)
{
    switch (value.toInt()) {
    case 90:
        return QStringLiteral("rotate-270");
    case 180:
        return QStringLiteral("rotate-180");
    case 270:
        return QStringLiteral("rotate-90");
    default:
        return QStringLiteral("rotate-0");
    }
}

namespace {
    struct QGStreamerMetaDataKey
    {
        QString qtName;
        const char *gstName;
        QVariant::Type type;

        QGStreamerMetaDataKey(const QString &qtn, const char *gstn, QVariant::Type t)
            : qtName(qtn)
            , gstName(gstn)
            , type(t)
        { }
    };
}

typedef QList<QGStreamerMetaDataKey> QGStreamerMetaDataKeys;
Q_GLOBAL_STATIC(QGStreamerMetaDataKeys, metadataKeys)

static const QGStreamerMetaDataKeys *qt_gstreamerMetaDataKeys()
{
    if (metadataKeys->isEmpty()) {
        metadataKeys->append(QGStreamerMetaDataKey(QMediaMetaData::Title, GST_TAG_TITLE, QVariant::String));
        //metadataKeys->append(QGStreamerMetaDataKey(QMediaMetaData::SubTitle, 0, QVariant::String));
        //metadataKeys->append(QGStreamerMetaDataKey(QMediaMetaData::Author, 0, QVariant::String));
        metadataKeys->append(QGStreamerMetaDataKey(QMediaMetaData::Comment, GST_TAG_COMMENT, QVariant::String));
#if GST_CHECK_VERSION(0,10,31)
        metadataKeys->append(QGStreamerMetaDataKey(QMediaMetaData::Date, GST_TAG_DATE_TIME, QVariant::DateTime));
#endif
        metadataKeys->append(QGStreamerMetaDataKey(QMediaMetaData::Description, GST_TAG_DESCRIPTION, QVariant::String));
        //metadataKeys->append(QGStreamerMetaDataKey(QMediaMetaData::Category, 0, QVariant::String));
        metadataKeys->append(QGStreamerMetaDataKey(QMediaMetaData::Genre, GST_TAG_GENRE, QVariant::String));
        //metadataKeys->append(QGStreamerMetaDataKey(QMediaMetaData::Year, 0, QVariant::Int));
        //metadataKeys->append(QGStreamerMetaDataKey(QMediaMetaData::UserRating, , QVariant::Int));

        metadataKeys->append(QGStreamerMetaDataKey(QMediaMetaData::Language, GST_TAG_LANGUAGE_CODE, QVariant::String));

        metadataKeys->append(QGStreamerMetaDataKey(QMediaMetaData::Publisher, GST_TAG_ORGANIZATION, QVariant::String));
        metadataKeys->append(QGStreamerMetaDataKey(QMediaMetaData::Copyright, GST_TAG_COPYRIGHT, QVariant::String));
        //metadataKeys->append(QGStreamerMetaDataKey(QMediaMetaData::ParentalRating, 0, QVariant::String));
        //metadataKeys->append(QGStreamerMetaDataKey(QMediaMetaData::RatingOrganisation, 0, QVariant::String));

        // Media
        //metadataKeys->append(QGStreamerMetaDataKey(QMediaMetaData::Size, 0, QVariant::Int));
        //metadataKeys->append(QGStreamerMetaDataKey(QMediaMetaData::MediaType, 0, QVariant::String));
        metadataKeys->append(QGStreamerMetaDataKey(QMediaMetaData::Duration, GST_TAG_DURATION, QVariant::Int));

        // Audio
        metadataKeys->append(QGStreamerMetaDataKey(QMediaMetaData::AudioBitRate, GST_TAG_BITRATE, QVariant::Int));
        metadataKeys->append(QGStreamerMetaDataKey(QMediaMetaData::AudioCodec, GST_TAG_AUDIO_CODEC, QVariant::String));
        //metadataKeys->append(QGStreamerMetaDataKey(QMediaMetaData::ChannelCount, 0, QVariant::Int));
        //metadataKeys->append(QGStreamerMetaDataKey(QMediaMetaData::SampleRate, 0, QVariant::Int));

        // Music
        metadataKeys->append(QGStreamerMetaDataKey(QMediaMetaData::AlbumTitle, GST_TAG_ALBUM, QVariant::String));
        metadataKeys->append(QGStreamerMetaDataKey(QMediaMetaData::AlbumArtist,  GST_TAG_ARTIST, QVariant::String));
        metadataKeys->append(QGStreamerMetaDataKey(QMediaMetaData::ContributingArtist, GST_TAG_PERFORMER, QVariant::String));
#if GST_CHECK_VERSION(0,10,19)
        metadataKeys->append(QGStreamerMetaDataKey(QMediaMetaData::Composer, GST_TAG_COMPOSER, QVariant::String));
#endif
        //metadataKeys->append(QGStreamerMetaDataKey(QMediaMetaData::Conductor, 0, QVariant::String));
        //metadataKeys->append(QGStreamerMetaDataKey(QMediaMetaData::Lyrics, 0, QVariant::String));
        //metadataKeys->append(QGStreamerMetaDataKey(QMediaMetaData::Mood, 0, QVariant::String));
        metadataKeys->append(QGStreamerMetaDataKey(QMediaMetaData::TrackNumber, GST_TAG_TRACK_NUMBER, QVariant::Int));

        //metadataKeys->append(QGStreamerMetaDataKey(QMediaMetaData::CoverArtUrlSmall, 0, QVariant::String));
        //metadataKeys->append(QGStreamerMetaDataKey(QMediaMetaData::CoverArtUrlLarge, 0, QVariant::String));

        // Image/Video
        //metadataKeys->append(QGStreamerMetaDataKey(QMediaMetaData::Resolution, 0, QVariant::Size));
        //metadataKeys->append(QGStreamerMetaDataKey(QMediaMetaData::PixelAspectRatio, 0, QVariant::Size));

        // Video
        //metadataKeys->append(QGStreamerMetaDataKey(QMediaMetaData::VideoFrameRate, 0, QVariant::String));
        //metadataKeys->append(QGStreamerMetaDataKey(QMediaMetaData::VideoBitRate, 0, QVariant::Double));
        metadataKeys->append(QGStreamerMetaDataKey(QMediaMetaData::VideoCodec, GST_TAG_VIDEO_CODEC, QVariant::String));

        //metadataKeys->append(QGStreamerMetaDataKey(QMediaMetaData::PosterUrl, 0, QVariant::String));

        // Movie
        //metadataKeys->append(QGStreamerMetaDataKey(QMediaMetaData::ChapterNumber, 0, QVariant::Int));
        //metadataKeys->append(QGStreamerMetaDataKey(QMediaMetaData::Director, 0, QVariant::String));
        metadataKeys->append(QGStreamerMetaDataKey(QMediaMetaData::LeadPerformer, GST_TAG_PERFORMER, QVariant::String));
        //metadataKeys->append(QGStreamerMetaDataKey(QMediaMetaData::Writer, 0, QVariant::String));
#if GST_CHECK_VERSION(0,10,30)
        // Photos
        metadataKeys->append(QGStreamerMetaDataKey(QMediaMetaData::CameraManufacturer, GST_TAG_DEVICE_MANUFACTURER, QVariant::String));
        metadataKeys->append(QGStreamerMetaDataKey(QMediaMetaData::CameraModel, GST_TAG_DEVICE_MODEL, QVariant::String));
        //metadataKeys->append(QGStreamerMetaDataKey(QMediaMetaData::Event, 0, QVariant::String));
        //metadataKeys->append(QGStreamerMetaDataKey(QMediaMetaData::Subject, 0, QVariant::String));

        metadataKeys->append(QGStreamerMetaDataKey(QMediaMetaData::Orientation, GST_TAG_IMAGE_ORIENTATION, QVariant::String));

        // GPS
        metadataKeys->append(QGStreamerMetaDataKey(QMediaMetaData::GPSLatitude, GST_TAG_GEO_LOCATION_LATITUDE, QVariant::Double));
        metadataKeys->append(QGStreamerMetaDataKey(QMediaMetaData::GPSLongitude, GST_TAG_GEO_LOCATION_LONGITUDE, QVariant::Double));
        metadataKeys->append(QGStreamerMetaDataKey(QMediaMetaData::GPSAltitude, GST_TAG_GEO_LOCATION_ELEVATION, QVariant::Double));
        metadataKeys->append(QGStreamerMetaDataKey(QMediaMetaData::GPSTrack, GST_TAG_GEO_LOCATION_MOVEMENT_DIRECTION, QVariant::Double));
        metadataKeys->append(QGStreamerMetaDataKey(QMediaMetaData::GPSSpeed, GST_TAG_GEO_LOCATION_MOVEMENT_SPEED, QVariant::Double));
        metadataKeys->append(QGStreamerMetaDataKey(QMediaMetaData::GPSImgDirection, GST_TAG_GEO_LOCATION_CAPTURE_DIRECTION, QVariant::Double));
#endif
    }

    return metadataKeys;
}

CameraBinMetaData::CameraBinMetaData(QObject *parent)
    :QMetaDataWriterControl(parent)
{
}

QVariant CameraBinMetaData::metaData(const QString &key) const
{
#if GST_CHECK_VERSION(0,10,30)
    if (key == QMediaMetaData::Orientation) {
        return fromGStreamerOrientation(m_values.value(QByteArray(GST_TAG_IMAGE_ORIENTATION)));
    } else if (key == QMediaMetaData::GPSSpeed) {
        const double metersPerSec = m_values.value(QByteArray(GST_TAG_GEO_LOCATION_MOVEMENT_SPEED)).toDouble();
        return (metersPerSec * 3600) / 1000;
    }
#endif

    const auto keys = *qt_gstreamerMetaDataKeys();
    for (const QGStreamerMetaDataKey &metadataKey : keys) {
        if (metadataKey.qtName == key)
            return m_values.value(QByteArray::fromRawData(metadataKey.gstName, qstrlen(metadataKey.gstName)));
    }
    return QVariant();
}

void CameraBinMetaData::setMetaData(const QString &key, const QVariant &value)
{
    QVariant correctedValue = value;
    if (value.isValid()) {
        if (key == QMediaMetaData::Orientation) {
            correctedValue = toGStreamerOrientation(value);
        } else if (key == QMediaMetaData::GPSSpeed) {
            // kilometers per hour to meters per second.
            correctedValue = (value.toDouble() * 1000) / 3600;
        }
    }

    const auto keys = *qt_gstreamerMetaDataKeys();
    for (const QGStreamerMetaDataKey &metadataKey : keys) {
        if (metadataKey.qtName == key) {
            const char *name = metadataKey.gstName;

            if (correctedValue.isValid()) {
                correctedValue.convert(metadataKey.type);
                m_values.insert(QByteArray::fromRawData(name, qstrlen(name)), correctedValue);
            } else {
                m_values.remove(QByteArray::fromRawData(name, qstrlen(name)));
            }

            emit QMetaDataWriterControl::metaDataChanged();
            emit metaDataChanged(m_values);

            return;
        }
    }
}

QStringList CameraBinMetaData::availableMetaData() const
{
    static QMap<QByteArray, QString> keysMap;
    if (keysMap.isEmpty()) {
        const auto keys = *qt_gstreamerMetaDataKeys();
        for (const QGStreamerMetaDataKey &metadataKey : keys)
            keysMap[QByteArray(metadataKey.gstName)] = metadataKey.qtName;
    }

    QStringList res;
    for (auto it = m_values.keyBegin(), end = m_values.keyEnd(); it != end; ++it) {
        QString tag = keysMap.value(*it);
        if (!tag.isEmpty())
            res.append(tag);
    }

    return res;
}

QT_END_NAMESPACE

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

#include "qgstreamercapturemetadatacontrol.h"

#include <QtMultimedia/qmediametadata.h>

#include <gst/gst.h>
#include <gst/gstversion.h>


typedef QMap<QString, QByteArray> QGstreamerMetaDataKeyLookup;
Q_GLOBAL_STATIC(QGstreamerMetaDataKeyLookup, metadataKeys)

static const QGstreamerMetaDataKeyLookup *qt_gstreamerMetaDataKeys()
{
    if (metadataKeys->isEmpty()) {
        metadataKeys->insert(QMediaMetaData::Title, GST_TAG_TITLE);
        metadataKeys->insert(QMediaMetaData::SubTitle, 0);
        //metadataKeys->insert(QMediaMetaData::Author, 0);
        metadataKeys->insert(QMediaMetaData::Comment, GST_TAG_COMMENT);
        metadataKeys->insert(QMediaMetaData::Description, GST_TAG_DESCRIPTION);
        //metadataKeys->insert(QMediaMetaData::Category, 0);
        metadataKeys->insert(QMediaMetaData::Genre, GST_TAG_GENRE);
        //metadataKeys->insert(QMediaMetaData::Year, 0);
        //metadataKeys->insert(QMediaMetaData::UserRating, 0);

        metadataKeys->insert(QMediaMetaData::Language, GST_TAG_LANGUAGE_CODE);

        metadataKeys->insert(QMediaMetaData::Publisher, GST_TAG_ORGANIZATION);
        metadataKeys->insert(QMediaMetaData::Copyright, GST_TAG_COPYRIGHT);
        //metadataKeys->insert(QMediaMetaData::ParentalRating, 0);
        //metadataKeys->insert(QMediaMetaData::RatingOrganisation, 0);

        // Media
        //metadataKeys->insert(QMediaMetaData::Size, 0);
        //metadataKeys->insert(QMediaMetaData::MediaType, 0);
        metadataKeys->insert(QMediaMetaData::Duration, GST_TAG_DURATION);

        // Audio
        metadataKeys->insert(QMediaMetaData::AudioBitRate, GST_TAG_BITRATE);
        metadataKeys->insert(QMediaMetaData::AudioCodec, GST_TAG_AUDIO_CODEC);
        //metadataKeys->insert(QMediaMetaData::ChannelCount, 0);
        //metadataKeys->insert(QMediaMetaData::SampleRate, 0);

        // Music
        metadataKeys->insert(QMediaMetaData::AlbumTitle, GST_TAG_ALBUM);
        metadataKeys->insert(QMediaMetaData::AlbumArtist,  GST_TAG_ARTIST);
        metadataKeys->insert(QMediaMetaData::ContributingArtist, GST_TAG_PERFORMER);
        metadataKeys->insert(QMediaMetaData::Composer, GST_TAG_COMPOSER);
        //metadataKeys->insert(QMediaMetaData::Conductor, 0);
        //metadataKeys->insert(QMediaMetaData::Lyrics, 0);
        //metadataKeys->insert(QMediaMetaData::Mood, 0);
        metadataKeys->insert(QMediaMetaData::TrackNumber, GST_TAG_TRACK_NUMBER);

        //metadataKeys->insert(QMediaMetaData::CoverArtUrlSmall, 0);
        //metadataKeys->insert(QMediaMetaData::CoverArtUrlLarge, 0);

        // Image/Video
        //metadataKeys->insert(QMediaMetaData::Resolution, 0);
        //metadataKeys->insert(QMediaMetaData::PixelAspectRatio, 0);

        // Video
        //metadataKeys->insert(QMediaMetaData::VideoFrameRate, 0);
        //metadataKeys->insert(QMediaMetaData::VideoBitRate, 0);
        metadataKeys->insert(QMediaMetaData::VideoCodec, GST_TAG_VIDEO_CODEC);

        //metadataKeys->insert(QMediaMetaData::PosterUrl, 0);

        // Movie
        //metadataKeys->insert(QMediaMetaData::ChapterNumber, 0);
        //metadataKeys->insert(QMediaMetaData::Director, 0);
        metadataKeys->insert(QMediaMetaData::LeadPerformer, GST_TAG_PERFORMER);
        //metadataKeys->insert(QMediaMetaData::Writer, 0);

        // Photos
        //metadataKeys->insert(QMediaMetaData::CameraManufacturer, 0);
        //metadataKeys->insert(QMediaMetaData::CameraModel, 0);
        //metadataKeys->insert(QMediaMetaData::Event, 0);
        //metadataKeys->insert(QMediaMetaData::Subject, 0 }
    }

    return metadataKeys;
}

QGstreamerCaptureMetaDataControl::QGstreamerCaptureMetaDataControl(QObject *parent)
    :QMetaDataWriterControl(parent)
{
}

QVariant QGstreamerCaptureMetaDataControl::metaData(const QString &key) const
{
    QGstreamerMetaDataKeyLookup::const_iterator it = qt_gstreamerMetaDataKeys()->find(key);
    if (it != qt_gstreamerMetaDataKeys()->constEnd())
        return m_values.value(it.value());

    return QVariant();
}

void QGstreamerCaptureMetaDataControl::setMetaData(const QString &key, const QVariant &value)
{
    QGstreamerMetaDataKeyLookup::const_iterator it = qt_gstreamerMetaDataKeys()->find(key);
    if (it != qt_gstreamerMetaDataKeys()->constEnd()) {
        m_values.insert(it.value(), value);

        emit QMetaDataWriterControl::metaDataChanged();
        emit QMetaDataWriterControl::metaDataChanged(key, value);
        emit metaDataChanged(m_values);
    }
}

QStringList QGstreamerCaptureMetaDataControl::availableMetaData() const
{
    QStringList res;
    for (auto it = m_values.keyBegin(), end = m_values.keyEnd(); it != end; ++it) {
        QString tag = qt_gstreamerMetaDataKeys()->key(*it);
        if (!tag.isEmpty())
            res.append(tag);
    }

    return res;
}

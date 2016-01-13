/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qgstreamermetadataprovider.h"
#include "qgstreamerplayersession.h"
#include <QDebug>
#include <QtMultimedia/qmediametadata.h>

#include <gst/gstversion.h>

QT_BEGIN_NAMESPACE

typedef QMap<QByteArray, QString> QGstreamerMetaDataKeyLookup;
Q_GLOBAL_STATIC(QGstreamerMetaDataKeyLookup, metadataKeys)

static const QGstreamerMetaDataKeyLookup *qt_gstreamerMetaDataKeys()
{
    if (metadataKeys->isEmpty()) {
        metadataKeys->insert(GST_TAG_TITLE, QMediaMetaData::Title);
        //metadataKeys->insert(0, QMediaMetaData::SubTitle);
        //metadataKeys->insert(0, QMediaMetaData::Author);
        metadataKeys->insert(GST_TAG_COMMENT, QMediaMetaData::Comment);
        metadataKeys->insert(GST_TAG_DESCRIPTION, QMediaMetaData::Description);
        //metadataKeys->insert(0, QMediaMetaData::Category);
        metadataKeys->insert(GST_TAG_GENRE, QMediaMetaData::Genre);
        metadataKeys->insert("year", QMediaMetaData::Year);
        //metadataKeys->insert(0, QMediaMetaData::UserRating);

        metadataKeys->insert(GST_TAG_LANGUAGE_CODE, QMediaMetaData::Language);

        metadataKeys->insert(GST_TAG_ORGANIZATION, QMediaMetaData::Publisher);
        metadataKeys->insert(GST_TAG_COPYRIGHT, QMediaMetaData::Copyright);
        //metadataKeys->insert(0, QMediaMetaData::ParentalRating);
        //metadataKeys->insert(0, QMediaMetaData::RatingOrganisation);

        // Media
        //metadataKeys->insert(0, QMediaMetaData::Size);
        //metadataKeys->insert(0,QMediaMetaData::MediaType );
        metadataKeys->insert(GST_TAG_DURATION, QMediaMetaData::Duration);

        // Audio
        metadataKeys->insert(GST_TAG_BITRATE, QMediaMetaData::AudioBitRate);
        metadataKeys->insert(GST_TAG_AUDIO_CODEC, QMediaMetaData::AudioCodec);
        //metadataKeys->insert(0, QMediaMetaData::ChannelCount);
        //metadataKeys->insert(0, QMediaMetaData::SampleRate);

        // Music
        metadataKeys->insert(GST_TAG_ALBUM, QMediaMetaData::AlbumTitle);
#if (GST_VERSION_MAJOR >= 0) && (GST_VERSION_MINOR >= 10) && (GST_VERSION_MICRO >= 25)
        metadataKeys->insert(GST_TAG_ALBUM_ARTIST, QMediaMetaData::AlbumArtist);
#endif
        metadataKeys->insert(GST_TAG_ARTIST, QMediaMetaData::ContributingArtist);
#if (GST_VERSION_MAJOR >= 0) && (GST_VERSION_MINOR >= 10) && (GST_VERSION_MICRO >= 19)
        metadataKeys->insert(GST_TAG_COMPOSER, QMediaMetaData::Composer);
#endif
        //metadataKeys->insert(0, QMediaMetaData::Conductor);
        //metadataKeys->insert(0, QMediaMetaData::Lyrics);
        //metadataKeys->insert(0, QMediaMetaData::Mood);
        metadataKeys->insert(GST_TAG_TRACK_NUMBER, QMediaMetaData::TrackNumber);

        //metadataKeys->insert(0, QMediaMetaData::CoverArtUrlSmall);
        //metadataKeys->insert(0, QMediaMetaData::CoverArtUrlLarge);

        // Image/Video
        metadataKeys->insert("resolution", QMediaMetaData::Resolution);
        metadataKeys->insert("pixel-aspect-ratio", QMediaMetaData::PixelAspectRatio);

        // Video
        //metadataKeys->insert(0, QMediaMetaData::VideoFrameRate);
        //metadataKeys->insert(0, QMediaMetaData::VideoBitRate);
        metadataKeys->insert(GST_TAG_VIDEO_CODEC, QMediaMetaData::VideoCodec);

        //metadataKeys->insert(0, QMediaMetaData::PosterUrl);

        // Movie
        //metadataKeys->insert(0, QMediaMetaData::ChapterNumber);
        //metadataKeys->insert(0, QMediaMetaData::Director);
        metadataKeys->insert(GST_TAG_PERFORMER, QMediaMetaData::LeadPerformer);
        //metadataKeys->insert(0, QMediaMetaData::Writer);

        // Photos
        //metadataKeys->insert(0, QMediaMetaData::CameraManufacturer);
        //metadataKeys->insert(0, QMediaMetaData::CameraModel);
        //metadataKeys->insert(0, QMediaMetaData::Event);
        //metadataKeys->insert(0, QMediaMetaData::Subject);
    }

    return metadataKeys;
}

QGstreamerMetaDataProvider::QGstreamerMetaDataProvider(QGstreamerPlayerSession *session, QObject *parent)
    :QMetaDataReaderControl(parent), m_session(session)
{
    connect(m_session, SIGNAL(tagsChanged()), SLOT(updateTags()));
}

QGstreamerMetaDataProvider::~QGstreamerMetaDataProvider()
{
}

bool QGstreamerMetaDataProvider::isMetaDataAvailable() const
{
    return !m_session->tags().isEmpty();
}

bool QGstreamerMetaDataProvider::isWritable() const
{
    return false;
}

QVariant QGstreamerMetaDataProvider::metaData(const QString &key) const
{
    return m_tags.value(key);
}

QStringList QGstreamerMetaDataProvider::availableMetaData() const
{
    return m_tags.keys();
}

void QGstreamerMetaDataProvider::updateTags()
{
    QVariantMap oldTags = m_tags;
    m_tags.clear();
    bool changed = false;

    QMapIterator<QByteArray ,QVariant> i(m_session->tags());
    while (i.hasNext()) {
         i.next();
         //use gstreamer native keys for elements not in our key map
         QString key = qt_gstreamerMetaDataKeys()->value(i.key(), i.key());
         m_tags.insert(key, i.value());
         if (i.value() != oldTags.value(key)) {
             changed = true;
             emit metaDataChanged(key, i.value());
         }
    }

    if (oldTags.isEmpty() != m_tags.isEmpty()) {
        emit metaDataAvailableChanged(isMetaDataAvailable());
        changed = true;
    }

    if (changed)
        emit metaDataChanged();
}

QT_END_NAMESPACE

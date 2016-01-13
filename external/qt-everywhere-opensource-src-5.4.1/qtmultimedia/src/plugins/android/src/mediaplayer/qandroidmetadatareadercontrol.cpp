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

#include "qandroidmetadatareadercontrol.h"

#include "androidmediametadataretriever.h"
#include <QtMultimedia/qmediametadata.h>
#include <qsize.h>
#include <QDate>

QT_BEGIN_NAMESPACE

// Genre name ordered by ID
// see: http://id3.org/id3v2.3.0#Appendix_A_-_Genre_List_from_ID3v1
static const char* qt_ID3GenreNames[] =
{
    "Blues", "Classic Rock", "Country", "Dance", "Disco", "Funk", "Grunge", "Hip-Hop", "Jazz",
    "Metal", "New Age", "Oldies", "Other", "Pop", "R&B", "Rap", "Reggae", "Rock", "Techno",
    "Industrial", "Alternative", "Ska", "Death Metal", "Pranks", "Soundtrack", "Euro-Techno",
    "Ambient", "Trip-Hop", "Vocal", "Jazz+Funk", "Fusion", "Trance", "Classical", "Instrumental",
    "Acid", "House", "Game", "Sound Clip", "Gospel", "Noise", "AlternRock", "Bass", "Soul", "Punk",
    "Space", "Meditative", "Instrumental Pop", "Instrumental Rock", "Ethnic", "Gothic", "Darkwave",
    "Techno-Industrial", "Electronic", "Pop-Folk", "Eurodance", "Dream", "Southern Rock", "Comedy",
    "Cult", "Gangsta", "Top 40", "Christian Rap", "Pop/Funk", "Jungle", "Native American",
    "Cabaret", "New Wave", "Psychadelic", "Rave", "Showtunes", "Trailer", "Lo-Fi", "Tribal",
    "Acid Punk", "Acid Jazz", "Polka", "Retro", "Musical", "Rock & Roll", "Hard Rock", "Folk",
    "Folk-Rock", "National Folk", "Swing", "Fast Fusion", "Bebob", "Latin", "Revival", "Celtic",
    "Bluegrass", "Avantgarde", "Gothic Rock", "Progressive Rock", "Psychedelic Rock",
    "Symphonic Rock", "Slow Rock", "Big Band", "Chorus", "Easy Listening", "Acoustic", "Humour",
    "Speech", "Chanson", "Opera", "Chamber Music", "Sonata", "Symphony", "Booty Bass", "Primus",
    "Porn Groove", "Satire", "Slow Jam", "Club", "Tango", "Samba", "Folklore", "Ballad",
    "Power Ballad", "Rhythmic Soul", "Freestyle", "Duet", "Punk Rock", "Drum Solo", "A capella",
    "Euro-House", "Dance Hall"
};

QAndroidMetaDataReaderControl::QAndroidMetaDataReaderControl(QObject *parent)
    : QMetaDataReaderControl(parent)
    , m_available(false)
    , m_retriever(new AndroidMediaMetadataRetriever)
{
}

QAndroidMetaDataReaderControl::~QAndroidMetaDataReaderControl()
{
    if (m_retriever) {
        m_retriever->release();
        delete m_retriever;
    }
}

bool QAndroidMetaDataReaderControl::isMetaDataAvailable() const
{
    return m_available;
}

QVariant QAndroidMetaDataReaderControl::metaData(const QString &key) const
{
    return m_metadata.value(key);
}

QStringList QAndroidMetaDataReaderControl::availableMetaData() const
{
    return m_metadata.keys();
}

void QAndroidMetaDataReaderControl::onMediaChanged(const QString &url)
{
    if (!m_retriever)
        return;

    m_mediaLocation = url;
    updateData();
}

void QAndroidMetaDataReaderControl::onUpdateMetaData()
{
    if (!m_retriever || m_mediaLocation.isEmpty())
        return;

    updateData();
}

void QAndroidMetaDataReaderControl::updateData()
{
    m_metadata.clear();

    if (!m_mediaLocation.isEmpty()) {
        if (m_retriever->setDataSource(m_mediaLocation)) {
            QString mimeType = m_retriever->extractMetadata(AndroidMediaMetadataRetriever::MimeType);
            if (!mimeType.isNull())
                m_metadata.insert(QMediaMetaData::MediaType, mimeType);

            bool isVideo = !m_retriever->extractMetadata(AndroidMediaMetadataRetriever::HasVideo).isNull()
                           || mimeType.startsWith(QStringLiteral("video"));

            QString string = m_retriever->extractMetadata(AndroidMediaMetadataRetriever::Album);
            if (!string.isNull())
                m_metadata.insert(QMediaMetaData::AlbumTitle, string);

            string = m_retriever->extractMetadata(AndroidMediaMetadataRetriever::AlbumArtist);
            if (!string.isNull())
                m_metadata.insert(QMediaMetaData::AlbumArtist, string);

            string = m_retriever->extractMetadata(AndroidMediaMetadataRetriever::Artist);
            if (!string.isNull()) {
                m_metadata.insert(isVideo ? QMediaMetaData::LeadPerformer
                                          : QMediaMetaData::ContributingArtist,
                                  string.split('/', QString::SkipEmptyParts));
            }

            string = m_retriever->extractMetadata(AndroidMediaMetadataRetriever::Author);
            if (!string.isNull())
                m_metadata.insert(QMediaMetaData::Author, string.split('/', QString::SkipEmptyParts));

            string = m_retriever->extractMetadata(AndroidMediaMetadataRetriever::Bitrate);
            if (!string.isNull()) {
                m_metadata.insert(isVideo ? QMediaMetaData::VideoBitRate
                                          : QMediaMetaData::AudioBitRate,
                                  string.toInt());
            }

            string = m_retriever->extractMetadata(AndroidMediaMetadataRetriever::CDTrackNumber);
            if (!string.isNull())
                m_metadata.insert(QMediaMetaData::TrackNumber, string.toInt());

            string = m_retriever->extractMetadata(AndroidMediaMetadataRetriever::Composer);
            if (!string.isNull())
                m_metadata.insert(QMediaMetaData::Composer, string.split('/', QString::SkipEmptyParts));

            string = m_retriever->extractMetadata(AndroidMediaMetadataRetriever::Date);
            if (!string.isNull())
                m_metadata.insert(QMediaMetaData::Date, QDateTime::fromString(string, QStringLiteral("yyyyMMddTHHmmss.zzzZ")).date());

            string = m_retriever->extractMetadata(AndroidMediaMetadataRetriever::Duration);
            if (!string.isNull())
                m_metadata.insert(QMediaMetaData::Duration, string.toLongLong());

            string = m_retriever->extractMetadata(AndroidMediaMetadataRetriever::Genre);
            if (!string.isNull()) {
                // The genre can be returned as an ID3v2 id, get the name for it in that case
                if (string.startsWith('(') && string.endsWith(')')) {
                    bool ok = false;
                    int genreId = string.midRef(1, string.length() - 2).toInt(&ok);
                    if (ok && genreId >= 0 && genreId <= 125)
                        string = QLatin1String(qt_ID3GenreNames[genreId]);
                }
                m_metadata.insert(QMediaMetaData::Genre, string);
            }

            string = m_retriever->extractMetadata(AndroidMediaMetadataRetriever::Title);
            if (!string.isNull())
                m_metadata.insert(QMediaMetaData::Title, string);

            string = m_retriever->extractMetadata(AndroidMediaMetadataRetriever::VideoHeight);
            if (!string.isNull()) {
                int height = string.toInt();
                int width = m_retriever->extractMetadata(AndroidMediaMetadataRetriever::VideoWidth).toInt();
                m_metadata.insert(QMediaMetaData::Resolution, QSize(width, height));
            }

            string = m_retriever->extractMetadata(AndroidMediaMetadataRetriever::Writer);
            if (!string.isNull())
                m_metadata.insert(QMediaMetaData::Writer, string.split('/', QString::SkipEmptyParts));

            string = m_retriever->extractMetadata(AndroidMediaMetadataRetriever::Year);
            if (!string.isNull())
                m_metadata.insert(QMediaMetaData::Year, string.toInt());
        }
    }

    bool oldAvailable = m_available;
    m_available = !m_metadata.isEmpty();
    if (m_available != oldAvailable)
        Q_EMIT metaDataAvailableChanged(m_available);

    Q_EMIT metaDataChanged();
}

QT_END_NAMESPACE

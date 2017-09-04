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

#include "qandroidmetadatareadercontrol.h"

#include "androidmediametadataretriever.h"
#include <QtMultimedia/qmediametadata.h>
#include <qsize.h>
#include <QDate>
#include <QtConcurrent/qtconcurrentrun.h>
#include <QtCore/qvector.h>

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

typedef QVector<QAndroidMetaDataReaderControl *> AndroidMetaDataReaders;
Q_GLOBAL_STATIC(AndroidMetaDataReaders, g_metaDataReaders)
Q_GLOBAL_STATIC(QMutex, g_metaDataReadersMtx)

QAndroidMetaDataReaderControl::QAndroidMetaDataReaderControl(QObject *parent)
    : QMetaDataReaderControl(parent)
    , m_available(false)
{
}

QAndroidMetaDataReaderControl::~QAndroidMetaDataReaderControl()
{
    QMutexLocker l(g_metaDataReadersMtx);
    const int idx = g_metaDataReaders->indexOf(this);
    if (idx != -1)
        g_metaDataReaders->remove(idx);
}

bool QAndroidMetaDataReaderControl::isMetaDataAvailable() const
{
    const QMutexLocker l(&m_mtx);
    return m_available && !m_metadata.isEmpty();
}

QVariant QAndroidMetaDataReaderControl::metaData(const QString &key) const
{
    const QMutexLocker l(&m_mtx);
    return m_metadata.value(key);
}

QStringList QAndroidMetaDataReaderControl::availableMetaData() const
{
    const QMutexLocker l(&m_mtx);
    return m_metadata.keys();
}

void QAndroidMetaDataReaderControl::onMediaChanged(const QMediaContent &media)
{
    const QMutexLocker l(&m_mtx);
    m_metadata.clear();
    m_mediaContent = media;
}

void QAndroidMetaDataReaderControl::onUpdateMetaData()
{
    {
        const QMutexLocker l(g_metaDataReadersMtx);
        if (!g_metaDataReaders->contains(this))
            g_metaDataReaders->append(this);
    }

    const QMutexLocker ml(&m_mtx);
    if (m_mediaContent.isNull())
        return;

    const QUrl &url = m_mediaContent.canonicalUrl();
    QtConcurrent::run(&extractMetadata, this, url);
}

void QAndroidMetaDataReaderControl::updateData(const QVariantMap &metadata, const QUrl &url)
{
    const QMutexLocker l(&m_mtx);

    if (m_mediaContent.canonicalUrl() != url)
        return;

    const bool oldAvailable = m_available;
    m_metadata = metadata;
    m_available = !m_metadata.isEmpty();

    if (m_available != oldAvailable)
        Q_EMIT metaDataAvailableChanged(m_available);

    Q_EMIT metaDataChanged();
}

void QAndroidMetaDataReaderControl::extractMetadata(QAndroidMetaDataReaderControl *caller,
                                                    const QUrl &url)
{
    QVariantMap metadata;

    if (!url.isEmpty()) {
        AndroidMediaMetadataRetriever retriever;
        if (!retriever.setDataSource(url))
            return;

        QString mimeType = retriever.extractMetadata(AndroidMediaMetadataRetriever::MimeType);
        if (!mimeType.isNull())
            metadata.insert(QMediaMetaData::MediaType, mimeType);

        bool isVideo = !retriever.extractMetadata(AndroidMediaMetadataRetriever::HasVideo).isNull()
                || mimeType.startsWith(QStringLiteral("video"));

        QString string = retriever.extractMetadata(AndroidMediaMetadataRetriever::Album);
        if (!string.isNull())
            metadata.insert(QMediaMetaData::AlbumTitle, string);

        string = retriever.extractMetadata(AndroidMediaMetadataRetriever::AlbumArtist);
        if (!string.isNull())
            metadata.insert(QMediaMetaData::AlbumArtist, string);

        string = retriever.extractMetadata(AndroidMediaMetadataRetriever::Artist);
        if (!string.isNull()) {
            metadata.insert(isVideo ? QMediaMetaData::LeadPerformer
                                    : QMediaMetaData::ContributingArtist,
                            string.split('/', QString::SkipEmptyParts));
        }

        string = retriever.extractMetadata(AndroidMediaMetadataRetriever::Author);
        if (!string.isNull())
            metadata.insert(QMediaMetaData::Author, string.split('/', QString::SkipEmptyParts));

        string = retriever.extractMetadata(AndroidMediaMetadataRetriever::Bitrate);
        if (!string.isNull()) {
            metadata.insert(isVideo ? QMediaMetaData::VideoBitRate
                                    : QMediaMetaData::AudioBitRate,
                            string.toInt());
        }

        string = retriever.extractMetadata(AndroidMediaMetadataRetriever::CDTrackNumber);
        if (!string.isNull())
            metadata.insert(QMediaMetaData::TrackNumber, string.toInt());

        string = retriever.extractMetadata(AndroidMediaMetadataRetriever::Composer);
        if (!string.isNull())
            metadata.insert(QMediaMetaData::Composer, string.split('/', QString::SkipEmptyParts));

        string = retriever.extractMetadata(AndroidMediaMetadataRetriever::Date);
        if (!string.isNull())
            metadata.insert(QMediaMetaData::Date, QDateTime::fromString(string, QStringLiteral("yyyyMMddTHHmmss.zzzZ")).date());

        string = retriever.extractMetadata(AndroidMediaMetadataRetriever::Duration);
        if (!string.isNull())
            metadata.insert(QMediaMetaData::Duration, string.toLongLong());

        string = retriever.extractMetadata(AndroidMediaMetadataRetriever::Genre);
        if (!string.isNull()) {
            // The genre can be returned as an ID3v2 id, get the name for it in that case
            if (string.startsWith('(') && string.endsWith(')')) {
                bool ok = false;
                const int genreId = string.midRef(1, string.length() - 2).toInt(&ok);
                if (ok && genreId >= 0 && genreId <= 125)
                    string = QLatin1String(qt_ID3GenreNames[genreId]);
            }
            metadata.insert(QMediaMetaData::Genre, string);
        }

        string = retriever.extractMetadata(AndroidMediaMetadataRetriever::Title);
        if (!string.isNull())
            metadata.insert(QMediaMetaData::Title, string);

        string = retriever.extractMetadata(AndroidMediaMetadataRetriever::VideoHeight);
        if (!string.isNull()) {
            const int height = string.toInt();
            const int width = retriever.extractMetadata(AndroidMediaMetadataRetriever::VideoWidth).toInt();
            metadata.insert(QMediaMetaData::Resolution, QSize(width, height));
        }

        string = retriever.extractMetadata(AndroidMediaMetadataRetriever::Writer);
        if (!string.isNull())
            metadata.insert(QMediaMetaData::Writer, string.split('/', QString::SkipEmptyParts));

        string = retriever.extractMetadata(AndroidMediaMetadataRetriever::Year);
        if (!string.isNull())
            metadata.insert(QMediaMetaData::Year, string.toInt());
    }

    const QMutexLocker lock(g_metaDataReadersMtx);
    if (!g_metaDataReaders->contains(caller))
        return;

    caller->updateData(metadata, url);
}

QT_END_NAMESPACE

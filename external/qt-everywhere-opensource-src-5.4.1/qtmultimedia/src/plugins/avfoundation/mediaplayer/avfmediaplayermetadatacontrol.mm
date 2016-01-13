/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "avfmediaplayermetadatacontrol.h"
#include "avfmediaplayersession.h"

#include <QtMultimedia/qmediametadata.h>

#import <AVFoundation/AVFoundation.h>

QT_USE_NAMESPACE

AVFMediaPlayerMetaDataControl::AVFMediaPlayerMetaDataControl(AVFMediaPlayerSession *session, QObject *parent)
    : QMetaDataReaderControl(parent)
    , m_session(session)
    , m_asset(0)
{
    QObject::connect(m_session, SIGNAL(mediaStatusChanged(QMediaPlayer::MediaStatus)), this, SLOT(updateTags()));
}

AVFMediaPlayerMetaDataControl::~AVFMediaPlayerMetaDataControl()
{
#ifdef QT_DEBUG_AVF
    qDebug() << Q_FUNC_INFO;
#endif
}

bool AVFMediaPlayerMetaDataControl::isMetaDataAvailable() const
{
    return !m_tags.isEmpty();
}

bool AVFMediaPlayerMetaDataControl::isWritable() const
{
    return false;
}

QVariant AVFMediaPlayerMetaDataControl::metaData(const QString &key) const
{
    return m_tags.value(key);
}

QStringList AVFMediaPlayerMetaDataControl::availableMetaData() const
{
    return m_tags.keys();
}

void AVFMediaPlayerMetaDataControl::updateTags()
{
#ifdef QT_DEBUG_AVF
    qDebug() << Q_FUNC_INFO;
#endif
    AVAsset *currentAsset = (AVAsset*)m_session->currentAssetHandle();

    //Don't read the tags from the same asset more than once
    if (currentAsset == m_asset) {
        return;
    }

    m_asset = currentAsset;

    //Since we've changed assets, clear old tags
    m_tags.clear();

    NSArray *metadataFormats = [currentAsset availableMetadataFormats];
    for ( NSString *format in metadataFormats) {
#ifdef QT_DEBUG_AVF
        qDebug() << "format: " << [format UTF8String];
#endif
        NSArray *metadataItems = [currentAsset metadataForFormat:format];
        for (AVMetadataItem* item in metadataItems) {
            NSString *keyString = [item commonKey];
            NSString *value = [item stringValue];

            if (keyString.length != 0) {
                //Process "commonMetadata" tags here:
                if ([keyString isEqualToString:AVMetadataCommonKeyTitle]) {
                    m_tags.insert(QMediaMetaData::Title, QString([value UTF8String]));
                } else if ([keyString isEqualToString: AVMetadataCommonKeyCreator]) {
                    m_tags.insert(QMediaMetaData::Author, QString([value UTF8String]));
                } else if ([keyString isEqualToString: AVMetadataCommonKeySubject]) {
                    m_tags.insert(QMediaMetaData::SubTitle, QString([value UTF8String]));
                } else if ([keyString isEqualToString: AVMetadataCommonKeyDescription]) {
                    m_tags.insert(QMediaMetaData::Description, QString([value UTF8String]));
                } else if ([keyString isEqualToString: AVMetadataCommonKeyPublisher]) {
                    m_tags.insert(QMediaMetaData::Publisher, QString([value UTF8String]));
                } else if ([keyString isEqualToString: AVMetadataCommonKeyContributor]) {
                    m_tags.insert(QMediaMetaData::ContributingArtist, QString([value UTF8String]));
                } else if ([keyString isEqualToString: AVMetadataCommonKeyCreationDate]) {
                    m_tags.insert(QMediaMetaData::Date, QString([value UTF8String]));
                } else if ([keyString isEqualToString: AVMetadataCommonKeyType]) {
                    m_tags.insert(QMediaMetaData::MediaType, QString([value UTF8String]));
                } else if ([keyString isEqualToString: AVMetadataCommonKeyLanguage]) {
                    m_tags.insert(QMediaMetaData::Language, QString([value UTF8String]));
                } else if ([keyString isEqualToString: AVMetadataCommonKeyCopyrights]) {
                    m_tags.insert(QMediaMetaData::Copyright, QString([value UTF8String]));
                } else if ([keyString isEqualToString: AVMetadataCommonKeyAlbumName]) {
                    m_tags.insert(QMediaMetaData::AlbumTitle, QString([value UTF8String]));
                } else if ([keyString isEqualToString: AVMetadataCommonKeyAuthor]) {
                    m_tags.insert(QMediaMetaData::Author, QString([value UTF8String]));
                } else if ([keyString isEqualToString: AVMetadataCommonKeyArtist]) {
                    m_tags.insert(QMediaMetaData::AlbumArtist, QString([value UTF8String]));
                } else if ([keyString isEqualToString: AVMetadataCommonKeyArtwork]) {
                    m_tags.insert(QMediaMetaData::PosterUrl, QString([value UTF8String]));
                }
            }

            if ([format isEqualToString:AVMetadataFormatID3Metadata]) {
                //TODO: Process ID3 metadata
            } else if ([format isEqualToString:AVMetadataFormatiTunesMetadata]) {
                //TODO: Process iTunes metadata
            } else if ([format isEqualToString:AVMetadataFormatQuickTimeUserData]) {
                //TODO: Process QuickTime metadata
            }
        }
    }

    Q_EMIT metaDataChanged();
}

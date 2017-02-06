/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd and/or its subsidiary(-ies).
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

static QString itemKey(AVMetadataItem *item)
{
    NSString *keyString = [item commonKey];

    if (keyString.length != 0) {
        if ([keyString isEqualToString:AVMetadataCommonKeyTitle]) {
            return QMediaMetaData::Title;
        } else if ([keyString isEqualToString: AVMetadataCommonKeySubject]) {
            return QMediaMetaData::SubTitle;
        } else if ([keyString isEqualToString: AVMetadataCommonKeyDescription]) {
            return QMediaMetaData::Description;
        } else if ([keyString isEqualToString: AVMetadataCommonKeyPublisher]) {
            return QMediaMetaData::Publisher;
        } else if ([keyString isEqualToString: AVMetadataCommonKeyCreationDate]) {
            return QMediaMetaData::Date;
        } else if ([keyString isEqualToString: AVMetadataCommonKeyType]) {
            return QMediaMetaData::MediaType;
        } else if ([keyString isEqualToString: AVMetadataCommonKeyLanguage]) {
            return QMediaMetaData::Language;
        } else if ([keyString isEqualToString: AVMetadataCommonKeyCopyrights]) {
            return QMediaMetaData::Copyright;
        } else if ([keyString isEqualToString: AVMetadataCommonKeyAlbumName]) {
            return QMediaMetaData::AlbumTitle;
        } else if ([keyString isEqualToString: AVMetadataCommonKeyAuthor]) {
            return QMediaMetaData::Author;
        } else if ([keyString isEqualToString: AVMetadataCommonKeyArtist]) {
            return QMediaMetaData::ContributingArtist;
        } else if ([keyString isEqualToString: AVMetadataCommonKeyArtwork]) {
            return QMediaMetaData::PosterUrl;
        }
    }

    return QString();
}

void AVFMediaPlayerMetaDataControl::updateTags()
{
#ifdef QT_DEBUG_AVF
    qDebug() << Q_FUNC_INFO;
#endif
    AVAsset *currentAsset = (AVAsset*)m_session->currentAssetHandle();

    //Don't read the tags from the same asset more than once
    if (currentAsset == m_asset)
        return;

    m_asset = currentAsset;

    QVariantMap oldTags = m_tags;
    //Since we've changed assets, clear old tags
    m_tags.clear();
    bool changed = false;

    // TODO: also process ID3, iTunes and QuickTime metadata

    NSArray *metadataItems = [currentAsset commonMetadata];
    for (AVMetadataItem* item in metadataItems) {
        const QString key = itemKey(item);
        if (!key.isEmpty()) {
            const QString value = QString::fromNSString([item stringValue]);
            if (!value.isNull()) {
                m_tags.insert(key, value);
                if (value != oldTags.value(key)) {
                    changed = true;
                    Q_EMIT metaDataChanged(key, value);
                }
            }
        }
    }

    if (oldTags.isEmpty() != m_tags.isEmpty()) {
        Q_EMIT metaDataAvailableChanged(!m_tags.isEmpty());
        changed = true;
    }

    if (changed)
        Q_EMIT metaDataChanged();
}

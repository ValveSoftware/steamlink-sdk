/****************************************************************************
**
** Copyright (C) 2016 Research In Motion
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
#include "mmrenderermetadatareadercontrol.h"
#include <QtMultimedia/qmediametadata.h>

QT_BEGIN_NAMESPACE

MmRendererMetaDataReaderControl::MmRendererMetaDataReaderControl(QObject *parent)
    : QMetaDataReaderControl(parent)
{
}

bool MmRendererMetaDataReaderControl::isMetaDataAvailable() const
{
    return !availableMetaData().isEmpty();
}

QVariant MmRendererMetaDataReaderControl::metaData(const QString &key) const
{
    if (key == QMediaMetaData::Title)
        return m_metaData.title();
    else if (key == QMediaMetaData::AlbumArtist)
        return m_metaData.artist();
    else if (key == QMediaMetaData::Comment)
        return m_metaData.comment();
    else if (key == QMediaMetaData::Genre)
        return m_metaData.genre();
    else if (key == QMediaMetaData::Year)
        return m_metaData.year();
    else if (key == QMediaMetaData::MediaType)
        return m_metaData.mediaType();
    else if (key == QMediaMetaData::Duration)
        return m_metaData.duration();
    else if (key == QMediaMetaData::AudioBitRate)
        return m_metaData.audioBitRate();
    else if (key == QMediaMetaData::SampleRate)
        return m_metaData.sampleRate();
    else if (key == QMediaMetaData::AlbumTitle)
        return m_metaData.album();
    else if (key == QMediaMetaData::TrackNumber)
        return m_metaData.track();
    else if (key == QMediaMetaData::Resolution)
        return m_metaData.resolution();

    return QVariant();
}

QStringList MmRendererMetaDataReaderControl::availableMetaData() const
{
    QStringList metaData;

    if (!m_metaData.title().isEmpty())
        metaData << QMediaMetaData::Title;
    if (!m_metaData.artist().isEmpty())
        metaData << QMediaMetaData::Author;
    if (!m_metaData.comment().isEmpty())
        metaData << QMediaMetaData::Comment;
    if (!m_metaData.genre().isEmpty())
        metaData << QMediaMetaData::Genre;
    if (m_metaData.year() != 0)
        metaData << QMediaMetaData::Year;
    if (!m_metaData.mediaType().isEmpty())
        metaData << QMediaMetaData::MediaType;
    if (m_metaData.duration() != 0)
        metaData << QMediaMetaData::Duration;
    if (m_metaData.audioBitRate() != 0)
        metaData << QMediaMetaData::AudioBitRate;
    if (m_metaData.sampleRate() != 0)
        metaData << QMediaMetaData::SampleRate;
    if (!m_metaData.album().isEmpty())
        metaData << QMediaMetaData::AlbumTitle;
    if (m_metaData.track() != 0)
        metaData << QMediaMetaData::TrackNumber;
    if (m_metaData.resolution().isValid())
        metaData << QMediaMetaData::Resolution;

    return metaData;
}

void MmRendererMetaDataReaderControl::setMetaData(const MmRendererMetaData &data)
{
    const MmRendererMetaData oldMetaData = m_metaData;
    const bool oldMetaDataAvailable = isMetaDataAvailable();

    m_metaData = data;

    bool changed = false;
    if (m_metaData.title() != oldMetaData.title()) {
        changed = true;
        emit metaDataChanged(QMediaMetaData::Title, m_metaData.title());
    } else if (m_metaData.artist() != oldMetaData.artist()) {
        changed = true;
        emit metaDataChanged(QMediaMetaData::Author, m_metaData.artist());
    } else if (m_metaData.comment() != oldMetaData.comment()) {
        changed = true;
        emit metaDataChanged(QMediaMetaData::Comment, m_metaData.comment());
    } else if (m_metaData.genre() != oldMetaData.genre()) {
        changed = true;
        emit metaDataChanged(QMediaMetaData::Genre, m_metaData.genre());
    } else if (m_metaData.year() != oldMetaData.year()) {
        changed = true;
        emit metaDataChanged(QMediaMetaData::Year, m_metaData.year());
    } else if (m_metaData.mediaType() != oldMetaData.mediaType()) {
        changed = true;
        emit metaDataChanged(QMediaMetaData::MediaType, m_metaData.mediaType());
    } else if (m_metaData.duration() != oldMetaData.duration()) {
        changed = true;
        emit metaDataChanged(QMediaMetaData::Duration, m_metaData.duration());
    } else if (m_metaData.audioBitRate() != oldMetaData.audioBitRate()) {
        changed = true;
        emit metaDataChanged(QMediaMetaData::AudioBitRate, m_metaData.audioBitRate());
    } else if (m_metaData.sampleRate() != oldMetaData.sampleRate()) {
        changed = true;
        emit metaDataChanged(QMediaMetaData::SampleRate, m_metaData.sampleRate());
    } else if (m_metaData.album() != oldMetaData.album()) {
        changed = true;
        emit metaDataChanged(QMediaMetaData::AlbumTitle, m_metaData.album());
    } else if (m_metaData.track() != oldMetaData.track()) {
        changed = true;
        emit metaDataChanged(QMediaMetaData::TrackNumber, m_metaData.track());
    } else if (m_metaData.resolution() != oldMetaData.resolution()) {
        changed = true;
        emit metaDataChanged(QMediaMetaData::Resolution, m_metaData.resolution());
    }

    if (changed)
        emit metaDataChanged();

    const bool metaDataAvailable = isMetaDataAvailable();
    if (metaDataAvailable != oldMetaDataAvailable)
        emit metaDataAvailableChanged(metaDataAvailable);
}

QT_END_NAMESPACE

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

#ifndef ANDROIDMEDIAMETADATARETRIEVER_H
#define ANDROIDMEDIAMETADATARETRIEVER_H

#include <QtCore/private/qjni_p.h>

QT_BEGIN_NAMESPACE

class AndroidMediaMetadataRetriever
{
public:
    enum MetadataKey {
        Album = 1,
        AlbumArtist = 13,
        Artist = 2,
        Author = 3,
        Bitrate = 20,
        CDTrackNumber = 0,
        Compilation = 15,
        Composer = 4,
        Date = 5,
        DiscNumber = 14,
        Duration = 9,
        Genre = 6,
        HasAudio = 16,
        HasVideo = 17,
        Location = 23,
        MimeType = 12,
        NumTracks = 10,
        Title = 7,
        VideoHeight = 19,
        VideoWidth = 18,
        VideoRotation = 24,
        Writer = 11,
        Year = 8
    };

    AndroidMediaMetadataRetriever();
    ~AndroidMediaMetadataRetriever();

    QString extractMetadata(MetadataKey key);
    void release();
    bool setDataSource(const QString &url);

private:
    QJNIObjectPrivate m_metadataRetriever;
};

QT_END_NAMESPACE

#endif // ANDROIDMEDIAMETADATARETRIEVER_H

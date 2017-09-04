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
#ifndef MMRENDERERMETADATA_H
#define MMRENDERERMETADATA_H

#include <QtCore/qglobal.h>
#include <QtCore/QSize>
#include <QtCore/QString>

QT_BEGIN_NAMESPACE

class MmRendererMetaData
{
public:
    MmRendererMetaData();
    bool parse(const QString &contextName);
    void clear();

    // Duration in milliseconds
    qlonglong duration() const;

    int height() const;
    int width() const;
    bool hasVideo() const;
    bool hasAudio() const;
    bool isSeekable() const;

    QString title() const;
    QString artist() const;
    QString comment() const;
    QString genre() const;
    int year() const;
    QString mediaType() const;
    int audioBitRate() const;
    int sampleRate() const;
    QString album() const;
    int track() const;
    QSize resolution() const;

private:
    qlonglong m_duration;
    int m_height;
    int m_width;
    int m_mediaType;
    float m_pixelWidth;
    float m_pixelHeight;
    bool m_seekable;
    QString m_title;
    QString m_artist;
    QString m_comment;
    QString m_genre;
    int m_year;
    int m_audioBitRate;
    int m_sampleRate;
    QString m_album;
    int m_track;
};

QT_END_NAMESPACE

#endif

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

#include "qm3uhandler.h"
#include <qmediaresource.h>
#include <QtCore/qiodevice.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qtextstream.h>
#include <QFile>
#include <QUrl>


class QM3uPlaylistReader : public QMediaPlaylistReader
{
public:
    QM3uPlaylistReader(QIODevice *device)
        :m_ownDevice(false), m_device(device), m_textStream(new QTextStream(m_device))
    {
        readItem();
    }

    QM3uPlaylistReader(const QUrl& location)
        :m_location(location), m_ownDevice(true)
    {
        QFile *f = new QFile(location.toLocalFile());
        if (f->open(QIODevice::ReadOnly | QIODevice::Text)) {
            m_device = f;
            m_textStream = new QTextStream(m_device);
            readItem();
        } else {
            delete f;
            m_device = 0;
            m_textStream = 0;
        }
    }

    virtual ~QM3uPlaylistReader()
    {
        if (m_ownDevice) {
            delete m_device;
        }
        delete m_textStream;
    }

    virtual bool atEnd() const
    {
        //we can't just use m_textStream->atEnd(),
        //for files with empty lines/comments at end
        return nextResource.isNull();
    }

    virtual QMediaContent readItem()
    {
        QMediaContent item;
        if (!nextResource.isNull())
            item = QMediaContent(nextResource);

        nextResource = QMediaContent();

        while (m_textStream && !m_textStream->atEnd()) {
            QString line = m_textStream->readLine().trimmed();
            if (line.isEmpty() || line[0] == '#' || line.size() > 4096)
                continue;

            QUrl fileUrl = QUrl::fromLocalFile(line);
            QUrl url(line);

            //m3u may contain url encoded entries or absolute/relative file names
            //prefer existing file if any
            QList<QUrl> candidates;
            if (!m_location.isEmpty()) {
                candidates << m_location.resolved(fileUrl);
                candidates << m_location.resolved(url);
            }
            candidates << fileUrl;
            candidates << url;

            for (const QUrl &candidate : qAsConst(candidates)) {
                if (QFile::exists(candidate.toLocalFile())) {
                    nextResource = candidate;
                    break;
                }
            }

            if (nextResource.isNull()) {
                //assume the relative urls are file names, not encoded urls if m3u is local file
                if (!m_location.isEmpty() && url.isRelative()) {
                    if (m_location.scheme() == QLatin1String("file"))
                        nextResource = m_location.resolved(fileUrl);
                    else
                        nextResource = m_location.resolved(url);
                } else {
                    nextResource = QMediaContent(QUrl::fromUserInput(line));
                }
            }

            break;
        }

        return item;
    }

    virtual void close()
    {
    }

private:
    QUrl m_location;
    bool m_ownDevice;
    QIODevice *m_device;
    QTextStream *m_textStream;
    QMediaContent nextResource;
};

class QM3uPlaylistWriter : public QMediaPlaylistWriter
{
public:
    QM3uPlaylistWriter(QIODevice *device)
        :m_device(device), m_textStream(new QTextStream(m_device))
    {
    }

    virtual ~QM3uPlaylistWriter()
    {
        delete m_textStream;
    }

    virtual bool writeItem(const QMediaContent& item)
    {
        *m_textStream << item.canonicalUrl().toString() << endl;
        return true;
    }

    virtual void close()
    {
    }

private:
    QIODevice *m_device;
    QTextStream *m_textStream;
};


QM3uPlaylistPlugin::QM3uPlaylistPlugin(QObject *parent)
    :QMediaPlaylistIOPlugin(parent)
{
}

QM3uPlaylistPlugin::~QM3uPlaylistPlugin()
{
}

bool QM3uPlaylistPlugin::canRead(QIODevice *device, const QByteArray &format) const
{
    return device->isReadable() && (format == "m3u" || format == "m3u8" || format.isEmpty());
}

bool QM3uPlaylistPlugin::canRead(const QUrl& location, const QByteArray &format) const
{
    if (!QFileInfo(location.toLocalFile()).isReadable())
        return false;

    if (format == "m3u" || format == "m3u8")
        return true;

    if (!format.isEmpty())
        return false;
    QString localFile = location.toLocalFile().toLower();
    return localFile.endsWith(QLatin1String("m3u")) || localFile.endsWith(QLatin1String("m3u8"));
}

bool QM3uPlaylistPlugin::canWrite(QIODevice *device, const QByteArray &format) const
{
    return device->isOpen() && device->isWritable() && (format == "m3u" || format == "m3u8");
}

QMediaPlaylistReader *QM3uPlaylistPlugin::createReader(QIODevice *device, const QByteArray &format)
{
    Q_UNUSED(format);
    return new QM3uPlaylistReader(device);
}

QMediaPlaylistReader *QM3uPlaylistPlugin::createReader(const QUrl& location, const QByteArray &format)
{
    Q_UNUSED(format);
    return new QM3uPlaylistReader(location);
}

QMediaPlaylistWriter *QM3uPlaylistPlugin::createWriter(QIODevice *device, const QByteArray &format)
{
    Q_UNUSED(format);
    return new QM3uPlaylistWriter(device);
}


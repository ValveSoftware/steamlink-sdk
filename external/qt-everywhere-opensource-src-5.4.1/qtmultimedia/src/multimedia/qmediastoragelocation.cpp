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

#include "qmediastoragelocation_p.h"

#include <QStandardPaths>

QT_BEGIN_NAMESPACE

QMediaStorageLocation::QMediaStorageLocation()
{
}

void QMediaStorageLocation::addStorageLocation(MediaType type, const QString &location)
{
    m_customLocations[type].append(location);
}

QDir QMediaStorageLocation::defaultLocation(MediaType type) const
{
    QStringList dirCandidates;

    dirCandidates << m_customLocations.value(type);

    switch (type) {
    case Movies:
        dirCandidates << QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
        break;
    case Music:
        dirCandidates << QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
        break;
    case Pictures:
        dirCandidates << QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    default:
        break;
    }

    dirCandidates << QDir::homePath();
    dirCandidates << QDir::currentPath();
    dirCandidates << QDir::tempPath();

    Q_FOREACH (const QString &path, dirCandidates) {
        if (QFileInfo(path).isWritable())
            return QDir(path);
    }

    return QDir();
}

QString QMediaStorageLocation::generateFileName(const QString &requestedName,
                                                MediaType type,
                                                const QString &prefix,
                                                const QString &extension) const
{
    if (requestedName.isEmpty())
        return generateFileName(prefix, defaultLocation(type), extension);

    QString path = requestedName;

    if (QFileInfo(path).isRelative())
        path = defaultLocation(type).absoluteFilePath(path);

    if (QFileInfo(path).isDir())
        return generateFileName(prefix, QDir(path), extension);

    if (!path.endsWith(extension))
        path.append(QString(QLatin1String(".%1")).arg(extension));

    return path;
}

QString QMediaStorageLocation::generateFileName(const QString &prefix,
                                                const QDir &dir,
                                                const QString &extension) const
{
    QMutexLocker lock(&m_mutex);

    const QString lastMediaKey = dir.absolutePath() + QLatin1Char(' ') + prefix + QLatin1Char(' ') + extension;
    qint64 lastMediaIndex = m_lastUsedIndex.value(lastMediaKey, 0);

    if (lastMediaIndex == 0) {
        // first run, find the maximum media number during the fist capture
        Q_FOREACH (const QString &fileName, dir.entryList(QStringList() << QString(QLatin1String("%1*.%2")).arg(prefix).arg(extension))) {
            const qint64 mediaIndex = fileName.midRef(prefix.length(), fileName.size() - prefix.length() - extension.length() - 1).toInt();
            lastMediaIndex = qMax(lastMediaIndex, mediaIndex);
        }
    }

    // don't just rely on cached lastMediaIndex value,
    // someone else may create a file after camera started
    while (true) {
        const QString name = QString(QLatin1String("%1%2.%3")).arg(prefix)
                                               .arg(lastMediaIndex + 1, 8, 10, QLatin1Char('0'))
                                               .arg(extension);

        const QString path = dir.absoluteFilePath(name);
        if (!QFileInfo(path).exists()) {
            m_lastUsedIndex[lastMediaKey] = lastMediaIndex + 1;
            return path;
        }

        lastMediaIndex++;
    }

    return QString();
}

QT_END_NAMESPACE

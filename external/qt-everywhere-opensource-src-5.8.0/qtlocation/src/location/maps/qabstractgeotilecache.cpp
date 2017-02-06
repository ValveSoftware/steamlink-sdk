/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtLocation module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/
#include "qabstractgeotilecache_p.h"

#include "qgeotilespec_p.h"

#include "qgeomappingmanager_p.h"

#include <QDir>
#include <QStandardPaths>
#include <QMetaType>
#include <QPixmap>
#include <QDebug>

Q_DECLARE_METATYPE(QList<QGeoTileSpec>)
Q_DECLARE_METATYPE(QSet<QGeoTileSpec>)

QT_BEGIN_NAMESPACE

QGeoTileTexture::QGeoTileTexture()
    : textureBound(false) {}

QGeoTileTexture::~QGeoTileTexture()
{
}

QAbstractGeoTileCache::QAbstractGeoTileCache(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<QGeoTileSpec>();
    qRegisterMetaType<QList<QGeoTileSpec> >();
    qRegisterMetaType<QSet<QGeoTileSpec> >();
}

QAbstractGeoTileCache::~QAbstractGeoTileCache()
{
}

void QAbstractGeoTileCache::printStats()
{
}

void QAbstractGeoTileCache::handleError(const QGeoTileSpec &, const QString &error)
{
    qWarning() << "tile request error " << error;
}

void QAbstractGeoTileCache::setMaxDiskUsage(int diskUsage)
{
    Q_UNUSED(diskUsage);
}

int QAbstractGeoTileCache::maxDiskUsage() const
{
    return 0;
}

int QAbstractGeoTileCache::diskUsage() const
{
    return 0;
}

void QAbstractGeoTileCache::setMaxMemoryUsage(int memoryUsage)
{
    Q_UNUSED(memoryUsage);
}

int QAbstractGeoTileCache::maxMemoryUsage() const
{
    return 0;
}

int QAbstractGeoTileCache::memoryUsage() const
{
    return 0;
}

QString QAbstractGeoTileCache::baseCacheDirectory()
{
    QString dir;

    // Try the shared cache first and use a specific directory. (e.g. ~/.cache/QtLocation)
    // If this is not supported by the platform, use the application-specific cache
    // location. (e.g. ~/.cache/<app_name>/QtLocation)
    dir = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation);

    if (!dir.isEmpty()) {
        // The shared cache may not be writable when application isolation is enforced.
        static bool writable = false;
        static bool writableChecked = false;
        if (!writableChecked) {
            writableChecked = true;
            QDir::root().mkpath(dir);
            QFile writeTestFile(QDir(dir).filePath(QStringLiteral("qt_cache_check")));
            writable = writeTestFile.open(QIODevice::WriteOnly);
            if (writable)
                writeTestFile.remove();
        }
        if (!writable)
            dir = QString();
    }

    if (dir.isEmpty())
        dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);

    if (!dir.endsWith(QLatin1Char('/')))
        dir += QLatin1Char('/');

    return dir;
}

QString QAbstractGeoTileCache::baseLocationCacheDirectory()
{
    // This scheme allows to have the "tiles" prefix hardcoded here
    // NOTE: changing the Qt version here requires changing it also in QGeoFileTileCache::init,
    // in the code that remove old version tiles !
    return baseCacheDirectory() + QLatin1String("QtLocation/5.8/tiles/");
}

QT_END_NAMESPACE

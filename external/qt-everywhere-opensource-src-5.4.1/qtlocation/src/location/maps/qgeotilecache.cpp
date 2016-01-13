/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtLocation module of the Qt Toolkit.
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
#include "qgeotilecache_p.h"

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

class QGeoCachedTileMemory
{
public:
    ~QGeoCachedTileMemory()
    {
        if (cache)
            cache->evictFromMemoryCache(this);
    }

    QGeoTileSpec spec;
    QGeoTileCache *cache;
    QByteArray bytes;
    QString format;
};

QGeoTileTexture::QGeoTileTexture()
    : textureBound(false) {}

void QCache3QTileEvictionPolicy::aboutToBeRemoved(const QGeoTileSpec &key, QSharedPointer<QGeoCachedTileDisk> obj)
{
    Q_UNUSED(key);
    // set the cache pointer to zero so we can't call evictFromDiskCache
    obj->cache = 0;
}

void QCache3QTileEvictionPolicy::aboutToBeEvicted(const QGeoTileSpec &key, QSharedPointer<QGeoCachedTileDisk> obj)
{
    Q_UNUSED(key);
    Q_UNUSED(obj);
    // leave the pointer set if it's a real eviction
}

QGeoCachedTileDisk::~QGeoCachedTileDisk()
{
    if (cache)
        cache->evictFromDiskCache(this);
}

QGeoTileTexture::~QGeoTileTexture()
{
}

QGeoTileCache::QGeoTileCache(const QString &directory, QObject *parent)
    : QObject(parent), directory_(directory),
      minTextureUsage_(0), extraTextureUsage_(0)
{
    qRegisterMetaType<QGeoTileSpec>();
    qRegisterMetaType<QList<QGeoTileSpec> >();
    qRegisterMetaType<QSet<QGeoTileSpec> >();

    // We keep default values here so that they are in one place
    // rather than in each individual plugin (the plugins can
    // of course override them)

    if (directory_.isEmpty()) {
        directory_ = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation)
                + QLatin1String("/QtLocation");
        QDir::root().mkpath(directory_);
    }

    // default values
    setMaxDiskUsage(20 * 1024 * 1024);
    setMaxMemoryUsage(3 * 1024 * 1024);
    setExtraTextureUsage(6 * 1024 * 1024);

    loadTiles();
}

void QGeoTileCache::loadTiles()
{
    QStringList formats;
    formats << QLatin1String("*.*");

    QDir dir(directory_);
    QStringList files = dir.entryList(formats, QDir::Files);

    // Method:
    // 1. read each queue file then, if each file exists, deserialize the data into the appropriate
    // cache queue.
    for (int i = 1; i<=4; i++) {
        QString filename = dir.filePath(QString::fromLatin1("queue") + QString::number(i));
        QFile file(filename);
        if (!file.open(QIODevice::ReadOnly))
            continue;
        QList<QSharedPointer<QGeoCachedTileDisk> > queue;
        QList<QGeoTileSpec> specs;
        QList<int> costs;
        while (!file.atEnd()) {
            QByteArray line = file.readLine().trimmed();
            QString filename = QString::fromLatin1(line.constData(), line.length());
            if (dir.exists(filename)){
                files.removeOne(filename);
                QGeoTileSpec spec = filenameToTileSpec(filename);
                if (spec.zoom() == -1)
                    continue;
                QSharedPointer<QGeoCachedTileDisk> tileDisk(new QGeoCachedTileDisk);
                tileDisk->filename = dir.filePath(filename);
                tileDisk->cache = this;
                tileDisk->spec = spec;
                QFileInfo fi(tileDisk->filename);
                specs.append(spec);
                queue.append(tileDisk);
                costs.append(fi.size());
            }
        }

        diskCache_.deserializeQueue(i, specs, queue, costs);
        file.close();
    }

    // 2. remaining tiles that aren't registered in a queue get pushed into cache here
    // this is a backup, in case the queue manifest files get deleted or out of sync due to
    // the application not closing down properly
    for (int i = 0; i < files.size(); ++i) {
        QGeoTileSpec spec = filenameToTileSpec(files.at(i));
        if (spec.zoom() == -1)
            continue;
        QString filename = dir.filePath(files.at(i));
        addToDiskCache(spec, filename);
    }
}

QGeoTileCache::~QGeoTileCache()
{
    // write disk cache queues to disk
    QDir dir(directory_);
    for (int i = 1; i<=4; i++) {
        QString filename = dir.filePath(QString::fromLatin1("queue") + QString::number(i));
        QFile file(filename);
        if (!file.open(QIODevice::WriteOnly)){
            qWarning() << "Unable to write tile cache file " << filename;
            continue;
        }
        QList<QSharedPointer<QGeoCachedTileDisk> > queue;
        diskCache_.serializeQueue(i, queue);
        foreach (const QSharedPointer<QGeoCachedTileDisk> &tile, queue) {
            if (tile.isNull())
                continue;

            // we just want the filename here, not the full path
            int index = tile->filename.lastIndexOf(QLatin1Char('/'));
            QByteArray filename = tile->filename.mid(index + 1).toLatin1() + '\n';
            file.write(filename);
        }
        file.close();
    }
}

void QGeoTileCache::printStats()
{
    textureCache_.printStats();
    memoryCache_.printStats();
    diskCache_.printStats();
}

void QGeoTileCache::setMaxDiskUsage(int diskUsage)
{
    diskCache_.setMaxCost(diskUsage);
}

int QGeoTileCache::maxDiskUsage() const
{
    return diskCache_.maxCost();
}

int QGeoTileCache::diskUsage() const
{
    return diskCache_.totalCost();
}

void QGeoTileCache::setMaxMemoryUsage(int memoryUsage)
{
    memoryCache_.setMaxCost(memoryUsage);
}

int QGeoTileCache::maxMemoryUsage() const
{
    return memoryCache_.maxCost();
}

int QGeoTileCache::memoryUsage() const
{
    return memoryCache_.totalCost();
}

void QGeoTileCache::setExtraTextureUsage(int textureUsage)
{
    extraTextureUsage_ = textureUsage;
    textureCache_.setMaxCost(minTextureUsage_ + extraTextureUsage_);
}

void QGeoTileCache::setMinTextureUsage(int textureUsage)
{
    minTextureUsage_ = textureUsage;
    textureCache_.setMaxCost(minTextureUsage_ + extraTextureUsage_);
}

int QGeoTileCache::maxTextureUsage() const
{
    return textureCache_.maxCost();
}

int QGeoTileCache::minTextureUsage() const
{
    return minTextureUsage_;
}


int QGeoTileCache::textureUsage() const
{
    return textureCache_.totalCost();
}

QSharedPointer<QGeoTileTexture> QGeoTileCache::get(const QGeoTileSpec &spec)
{
    QSharedPointer<QGeoTileTexture> tt = textureCache_.object(spec);
    if (tt)
        return tt;

    QSharedPointer<QGeoCachedTileMemory> tm = memoryCache_.object(spec);
    if (tm) {
        QPixmap pixmap;
        if (!pixmap.loadFromData(tm->bytes)) {
            handleError(spec, QLatin1String("Problem with tile image"));
            return QSharedPointer<QGeoTileTexture>(0);
        }
        QSharedPointer<QGeoTileTexture> tt = addToTextureCache(spec, pixmap);
        if (tt)
            return tt;
    }

    QSharedPointer<QGeoCachedTileDisk> td = diskCache_.object(spec);
    if (td) {
        QStringList parts = td->filename.split('.');
        QFile file(td->filename);
        file.open(QIODevice::ReadOnly);
        QByteArray bytes = file.readAll();
        file.close();

        QPixmap pixmap;
        if (!pixmap.loadFromData(bytes, (parts.size() == 2 ? parts.at(1).toLocal8Bit().constData() : 0))) {
            handleError(spec, QLatin1String("Problem with tile image"));
            return QSharedPointer<QGeoTileTexture>(0);
        }

        addToMemoryCache(spec, bytes, (parts.size() == 2 ? parts.at(1) : QLatin1String("")));
        QSharedPointer<QGeoTileTexture> tt = addToTextureCache(td->spec, pixmap);
        if (tt)
            return tt;
    }

    return QSharedPointer<QGeoTileTexture>();
}

QString QGeoTileCache::directory() const
{
    return directory_;
}

void QGeoTileCache::insert(const QGeoTileSpec &spec,
                           const QByteArray &bytes,
                           const QString &format,
                           QGeoTiledMappingManagerEngine::CacheAreas areas)
{
    if (areas & QGeoTiledMappingManagerEngine::DiskCache) {
        QString filename = tileSpecToFilename(spec, format, directory_);
        QFile file(filename);
        file.open(QIODevice::WriteOnly);
        file.write(bytes);
        file.close();

        addToDiskCache(spec, filename);
    }

    if (areas & QGeoTiledMappingManagerEngine::MemoryCache) {
        addToMemoryCache(spec, bytes, format);
    }

    /* inserts do not hit the texture cache -- this actually reduces overall
     * cache hit rates because many tiles come too late to be useful
     * and act as a poison */
}

void QGeoTileCache::evictFromDiskCache(QGeoCachedTileDisk *td)
{
    QFile::remove(td->filename);
}

void QGeoTileCache::evictFromMemoryCache(QGeoCachedTileMemory * /* tm  */)
{
}

QSharedPointer<QGeoCachedTileDisk> QGeoTileCache::addToDiskCache(const QGeoTileSpec &spec, const QString &filename)
{
    QSharedPointer<QGeoCachedTileDisk> td(new QGeoCachedTileDisk);
    td->spec = spec;
    td->filename = filename;
    td->cache = this;

    QFileInfo fi(filename);
    int diskCost = fi.size();
    diskCache_.insert(spec, td, diskCost);
    return td;
}

QSharedPointer<QGeoCachedTileMemory> QGeoTileCache::addToMemoryCache(const QGeoTileSpec &spec, const QByteArray &bytes, const QString &format)
{
    QSharedPointer<QGeoCachedTileMemory> tm(new QGeoCachedTileMemory);
    tm->spec = spec;
    tm->cache = this;
    tm->bytes = bytes;
    tm->format = format;

    int cost = bytes.size();
    memoryCache_.insert(spec, tm, cost);

    return tm;
}

QSharedPointer<QGeoTileTexture> QGeoTileCache::addToTextureCache(const QGeoTileSpec &spec, const QPixmap &pixmap)
{
    QSharedPointer<QGeoTileTexture> tt(new QGeoTileTexture);
    tt->spec = spec;
    tt->image = pixmap.toImage();

    int textureCost = tt->image.width() * tt->image.height() * tt->image.depth() / 8;
    textureCache_.insert(spec, tt, textureCost);

    return tt;
}

void QGeoTileCache::handleError(const QGeoTileSpec &, const QString &error)
{
    qWarning() << "tile request error " << error;
}

QString QGeoTileCache::tileSpecToFilename(const QGeoTileSpec &spec, const QString &format, const QString &directory)
{
    QString filename = spec.plugin();
    filename += QLatin1String("-");
    filename += QString::number(spec.mapId());
    filename += QLatin1String("-");
    filename += QString::number(spec.zoom());
    filename += QLatin1String("-");
    filename += QString::number(spec.x());
    filename += QLatin1String("-");
    filename += QString::number(spec.y());

    //Append version if real version number to ensure backwards compatibility and eviction of old tiles
    if (spec.version() != -1) {
        filename += QLatin1String("-");
        filename += QString::number(spec.version());
    }

    filename += QLatin1String(".");
    filename += format;

    QDir dir = QDir(directory);

    return dir.filePath(filename);
}

QGeoTileSpec QGeoTileCache::filenameToTileSpec(const QString &filename)
{
    QGeoTileSpec emptySpec;

    QStringList parts = filename.split('.');

    if (parts.length() != 2)
        return emptySpec;

    QString name = parts.at(0);
    QStringList fields = name.split('-');

    int length = fields.length();
    if (length != 5 && length != 6)
        return emptySpec;

    QList<int> numbers;

    bool ok = false;
    for (int i = 1; i < length; ++i) {
        ok = false;
        int value = fields.at(i).toInt(&ok);
        if (!ok)
            return emptySpec;
        numbers.append(value);
    }

    //File name without version, append default
    if (numbers.length() < 5)
        numbers.append(-1);

    return QGeoTileSpec(fields.at(0),
                    numbers.at(0),
                    numbers.at(1),
                    numbers.at(2),
                    numbers.at(3),
                    numbers.at(4));
}

QT_END_NAMESPACE

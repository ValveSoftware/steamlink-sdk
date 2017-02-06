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
#include "qgeofiletilecache_p.h"

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
    QGeoFileTileCache *cache;
    QByteArray bytes;
    QString format;
};

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

QGeoFileTileCache::QGeoFileTileCache(const QString &directory, QObject *parent)
    : QAbstractGeoTileCache(parent), directory_(directory), minTextureUsage_(0), extraTextureUsage_(0)
    ,costStrategyDisk_(ByteSize), costStrategyMemory_(ByteSize), costStrategyTexture_(ByteSize)
{

}

void QGeoFileTileCache::init()
{
    const QString basePath = baseCacheDirectory() + QLatin1String("QtLocation/");

    // delete old tiles from QtLocation 5.7 or prior
    // Newer version use plugin-specific subdirectories, versioned with qt version so those are not affected.
    // TODO Remove cache cleanup in Qt 6
    QDir baseDir(basePath);
    if (baseDir.exists()) {
        const QStringList oldCacheFiles = baseDir.entryList(QDir::Files);
        foreach (const QString& file, oldCacheFiles)
            baseDir.remove(file);
        const QStringList oldCacheDirs = { QStringLiteral("osm"), QStringLiteral("mapbox"), QStringLiteral("here") };
        foreach (const QString& d, oldCacheDirs) {
            QDir oldCacheDir(basePath + QLatin1Char('/') + d);
            if (oldCacheDir.exists())
                oldCacheDir.removeRecursively();
        }
    }

    if (directory_.isEmpty()) {
        directory_ = baseLocationCacheDirectory();
        qWarning() << "Plugin uses uninitialized QGeoFileTileCache directory which was deleted during startup";
    }

    QDir::root().mkpath(directory_);

    // default values
    if (!diskCache_.maxCost()) { // If setMaxDiskUsage has not been called yet
        if (costStrategyDisk_ == ByteSize)
            setMaxDiskUsage(50 * 1024 * 1024);
        else
            setMaxDiskUsage(1000);
    }

    if (!memoryCache_.maxCost()) { // If setMaxMemoryUsage has not been called yet
        if (costStrategyMemory_ == ByteSize)
            setMaxMemoryUsage(3 * 1024 * 1024);
        else
            setMaxMemoryUsage(100);
    }

    if (!textureCache_.maxCost()) { // If setExtraTextureUsage has not been called yet
        if (costStrategyTexture_ == ByteSize)
            setExtraTextureUsage(6 * 1024 * 1024);
        else
            setExtraTextureUsage(30); // byte size of texture is >> compressed image, hence unitary cost should be lower
    }

    loadTiles();
}

void QGeoFileTileCache::loadTiles()
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
                if (costStrategyDisk_ == ByteSize)
                    costs.append(fi.size());
                else
                    costs.append(1);

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

QGeoFileTileCache::~QGeoFileTileCache()
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

void QGeoFileTileCache::printStats()
{
    textureCache_.printStats();
    memoryCache_.printStats();
    diskCache_.printStats();
}

void QGeoFileTileCache::setMaxDiskUsage(int diskUsage)
{
    diskCache_.setMaxCost(diskUsage);
}

int QGeoFileTileCache::maxDiskUsage() const
{
    return diskCache_.maxCost();
}

int QGeoFileTileCache::diskUsage() const
{
    return diskCache_.totalCost();
}

void QGeoFileTileCache::setMaxMemoryUsage(int memoryUsage)
{
    memoryCache_.setMaxCost(memoryUsage);
}

int QGeoFileTileCache::maxMemoryUsage() const
{
    return memoryCache_.maxCost();
}

int QGeoFileTileCache::memoryUsage() const
{
    return memoryCache_.totalCost();
}

void QGeoFileTileCache::setExtraTextureUsage(int textureUsage)
{
    extraTextureUsage_ = textureUsage;
    textureCache_.setMaxCost(minTextureUsage_ + extraTextureUsage_);
}

void QGeoFileTileCache::setMinTextureUsage(int textureUsage)
{
    minTextureUsage_ = textureUsage;
    textureCache_.setMaxCost(minTextureUsage_ + extraTextureUsage_);
}

int QGeoFileTileCache::maxTextureUsage() const
{
    return textureCache_.maxCost();
}

int QGeoFileTileCache::minTextureUsage() const
{
    return minTextureUsage_;
}


int QGeoFileTileCache::textureUsage() const
{
    return textureCache_.totalCost();
}

void QGeoFileTileCache::clearAll()
{
    textureCache_.clear();
    memoryCache_.clear();
    diskCache_.clear();
    QDir dir(directory_);
    dir.setNameFilters(QStringList() << QLatin1String("*-*-*-*.*"));
    dir.setFilter(QDir::Files);
    foreach (QString dirFile, dir.entryList()) {
        dir.remove(dirFile);
    }
}

void QGeoFileTileCache::clearMapId(const int mapId)
{
    for (const QGeoTileSpec &k : diskCache_.keys())
        if (k.mapId() == mapId)
            diskCache_.remove(k, true);
    for (const QGeoTileSpec &k : memoryCache_.keys())
        if (k.mapId() == mapId)
            memoryCache_.remove(k);
    for (const QGeoTileSpec &k : textureCache_.keys())
        if (k.mapId() == mapId)
            textureCache_.remove(k);

    // TODO: It seems the cache leaves residues, like some tiles do not get picked up.
    // After the above calls, files that shouldnt be left behind are still on disk.
    // Do an additional pass and make sure what has to be deleted gets deleted.
    QDir dir(directory_);
    QStringList formats;
    formats << QLatin1String("*.*");
    QStringList files = dir.entryList(formats, QDir::Files);
    qWarning() << "Old tile data detected. Cache eviction left out "<< files.size() << "tiles";
    for (const QString &tileFileName : files) {
        QGeoTileSpec spec = filenameToTileSpec(tileFileName);
        if (spec.mapId() != mapId)
            continue;
        QFile::remove(dir.filePath(tileFileName));
    }
}

void QGeoFileTileCache::setCostStrategyDisk(QAbstractGeoTileCache::CostStrategy costStrategy)
{
    costStrategyDisk_ = costStrategy;
}

QAbstractGeoTileCache::CostStrategy QGeoFileTileCache::costStrategyDisk() const
{
    return costStrategyDisk_;
}

void QGeoFileTileCache::setCostStrategyMemory(QAbstractGeoTileCache::CostStrategy costStrategy)
{
    costStrategyMemory_ = costStrategy;
}

QAbstractGeoTileCache::CostStrategy QGeoFileTileCache::costStrategyMemory() const
{
    return costStrategyMemory_;
}

void QGeoFileTileCache::setCostStrategyTexture(QAbstractGeoTileCache::CostStrategy costStrategy)
{
    costStrategyTexture_ = costStrategy;
}

QAbstractGeoTileCache::CostStrategy QGeoFileTileCache::costStrategyTexture() const
{
    return costStrategyTexture_;
}

QSharedPointer<QGeoTileTexture> QGeoFileTileCache::get(const QGeoTileSpec &spec)
{
    QSharedPointer<QGeoTileTexture> tt = getFromMemory(spec);
    if (tt)
        return tt;
    return getFromDisk(spec);
}

void QGeoFileTileCache::insert(const QGeoTileSpec &spec,
                           const QByteArray &bytes,
                           const QString &format,
                           QGeoTiledMappingManagerEngine::CacheAreas areas)
{
    if (bytes.isEmpty())
        return;

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

void QGeoFileTileCache::evictFromDiskCache(QGeoCachedTileDisk *td)
{
    QFile::remove(td->filename);
}

void QGeoFileTileCache::evictFromMemoryCache(QGeoCachedTileMemory * /* tm  */)
{
}

QSharedPointer<QGeoCachedTileDisk> QGeoFileTileCache::addToDiskCache(const QGeoTileSpec &spec, const QString &filename)
{
    QSharedPointer<QGeoCachedTileDisk> td(new QGeoCachedTileDisk);
    td->spec = spec;
    td->filename = filename;
    td->cache = this;

    int cost = 1;
    if (costStrategyDisk_ == ByteSize) {
        QFileInfo fi(filename);
        cost = fi.size();
    }
    diskCache_.insert(spec, td, cost);
    return td;
}

QSharedPointer<QGeoCachedTileMemory> QGeoFileTileCache::addToMemoryCache(const QGeoTileSpec &spec, const QByteArray &bytes, const QString &format)
{
    QSharedPointer<QGeoCachedTileMemory> tm(new QGeoCachedTileMemory);
    tm->spec = spec;
    tm->cache = this;
    tm->bytes = bytes;
    tm->format = format;

    int cost = 1;
    if (costStrategyMemory_ == ByteSize)
        cost = bytes.size();
    memoryCache_.insert(spec, tm, cost);

    return tm;
}

QSharedPointer<QGeoTileTexture> QGeoFileTileCache::addToTextureCache(const QGeoTileSpec &spec, const QImage &image)
{
    QSharedPointer<QGeoTileTexture> tt(new QGeoTileTexture);
    tt->spec = spec;
    tt->image = image;

    int cost = 1;
    if (costStrategyTexture_ == ByteSize)
        cost = image.width() * image.height() * image.depth() / 8;
    textureCache_.insert(spec, tt, cost);

    return tt;
}

QSharedPointer<QGeoTileTexture> QGeoFileTileCache::getFromMemory(const QGeoTileSpec &spec)
{
    QSharedPointer<QGeoTileTexture> tt = textureCache_.object(spec);
    if (tt)
        return tt;

    QSharedPointer<QGeoCachedTileMemory> tm = memoryCache_.object(spec);
    if (tm) {
        QImage image;
        if (!image.loadFromData(tm->bytes)) {
            handleError(spec, QLatin1String("Problem with tile image"));
            return QSharedPointer<QGeoTileTexture>(0);
        }
        QSharedPointer<QGeoTileTexture> tt = addToTextureCache(spec, image);
        if (tt)
            return tt;
    }
    return QSharedPointer<QGeoTileTexture>();
}

QSharedPointer<QGeoTileTexture> QGeoFileTileCache::getFromDisk(const QGeoTileSpec &spec)
{
    QSharedPointer<QGeoCachedTileDisk> td = diskCache_.object(spec);
    if (td) {
        const QString format = QFileInfo(td->filename).suffix();
        QFile file(td->filename);
        file.open(QIODevice::ReadOnly);
        QByteArray bytes = file.readAll();
        file.close();

        QImage image;
        if (!image.loadFromData(bytes)) {
            handleError(spec, QLatin1String("Problem with tile image"));
            return QSharedPointer<QGeoTileTexture>(0);
        }

        addToMemoryCache(spec, bytes, format);
        QSharedPointer<QGeoTileTexture> tt = addToTextureCache(td->spec, image);
        if (tt)
            return tt;
    }

    return QSharedPointer<QGeoTileTexture>();
}

QString QGeoFileTileCache::tileSpecToFilename(const QGeoTileSpec &spec, const QString &format, const QString &directory) const
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

QGeoTileSpec QGeoFileTileCache::filenameToTileSpec(const QString &filename) const
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

QString QGeoFileTileCache::directory() const
{
    return directory_;
}

QT_END_NAMESPACE

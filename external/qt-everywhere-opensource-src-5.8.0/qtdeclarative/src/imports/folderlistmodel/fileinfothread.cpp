/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
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

#include "fileinfothread_p.h"
#include <qdiriterator.h>

#include <QDebug>


FileInfoThread::FileInfoThread(QObject *parent)
    : QThread(parent),
      abort(false),
#ifndef QT_NO_FILESYSTEMWATCHER
      watcher(0),
#endif
      sortFlags(QDir::Name),
      needUpdate(true),
      folderUpdate(false),
      sortUpdate(false),
      showFiles(true),
      showDirs(true),
      showDirsFirst(false),
      showDotAndDotDot(false),
      showHidden(false),
      showOnlyReadable(false),
      caseSensitive(true)
{
#ifndef QT_NO_FILESYSTEMWATCHER
    watcher = new QFileSystemWatcher(this);
    connect(watcher, SIGNAL(directoryChanged(QString)), this, SLOT(dirChanged(QString)));
    connect(watcher, SIGNAL(fileChanged(QString)), this, SLOT(updateFile(QString)));
#endif // !QT_NO_FILESYSTEMWATCHER
}

FileInfoThread::~FileInfoThread()
{
    QMutexLocker locker(&mutex);
    abort = true;
    condition.wakeOne();
    locker.unlock();
    wait();
}

void FileInfoThread::clear()
{
    QMutexLocker locker(&mutex);
#ifndef QT_NO_FILESYSTEMWATCHER
    watcher->removePaths(watcher->files());
    watcher->removePaths(watcher->directories());
#endif
}

void FileInfoThread::removePath(const QString &path)
{
    QMutexLocker locker(&mutex);
#ifndef QT_NO_FILESYSTEMWATCHER
    if (!path.startsWith(QLatin1Char(':')))
        watcher->removePath(path);
#else
    Q_UNUSED(path);
#endif
    currentPath.clear();
}

void FileInfoThread::setPath(const QString &path)
{
    Q_ASSERT(!path.isEmpty());

    QMutexLocker locker(&mutex);
#ifndef QT_NO_FILESYSTEMWATCHER
    if (!path.startsWith(QLatin1Char(':')))
        watcher->addPath(path);
#endif
    currentPath = path;
    needUpdate = true;
    condition.wakeAll();
}

void FileInfoThread::setRootPath(const QString &path)
{
    Q_ASSERT(!path.isEmpty());

    QMutexLocker locker(&mutex);
    rootPath = path;
}

#ifndef QT_NO_FILESYSTEMWATCHER
void FileInfoThread::dirChanged(const QString &directoryPath)
{
    Q_UNUSED(directoryPath);
    QMutexLocker locker(&mutex);
    folderUpdate = true;
    condition.wakeAll();
}
#endif

void FileInfoThread::setSortFlags(QDir::SortFlags flags)
{
    QMutexLocker locker(&mutex);
    sortFlags = flags;
    sortUpdate = true;
    condition.wakeAll();
}

void FileInfoThread::setNameFilters(const QStringList & filters)
{
    QMutexLocker locker(&mutex);
    nameFilters = filters;
    folderUpdate = true;
    condition.wakeAll();
}

void FileInfoThread::setShowFiles(bool show)
{
    QMutexLocker locker(&mutex);
    showFiles = show;
    folderUpdate = true;
    condition.wakeAll();
}

void FileInfoThread::setShowDirs(bool showFolders)
{
    QMutexLocker locker(&mutex);
    showDirs = showFolders;
    folderUpdate = true;
    condition.wakeAll();
}

void FileInfoThread::setShowDirsFirst(bool show)
{
    QMutexLocker locker(&mutex);
    showDirsFirst = show;
    folderUpdate = true;
    condition.wakeAll();
}

void FileInfoThread::setShowDotAndDotDot(bool on)
{
    QMutexLocker locker(&mutex);
    showDotAndDotDot = on;
    folderUpdate = true;
    needUpdate = true;
    condition.wakeAll();
}

void FileInfoThread::setShowHidden(bool on)
{
    QMutexLocker locker(&mutex);
    showHidden = on;
    folderUpdate = true;
    needUpdate = true;
    condition.wakeAll();
}

void FileInfoThread::setShowOnlyReadable(bool on)
{
    QMutexLocker locker(&mutex);
    showOnlyReadable = on;
    folderUpdate = true;
    condition.wakeAll();
}

void FileInfoThread::setCaseSensitive(bool on)
{
    QMutexLocker locker(&mutex);
    caseSensitive = on;
    folderUpdate = true;
    condition.wakeAll();
}

#ifndef QT_NO_FILESYSTEMWATCHER
void FileInfoThread::updateFile(const QString &path)
{
    Q_UNUSED(path);
    QMutexLocker locker(&mutex);
    folderUpdate = true;
    condition.wakeAll();
}
#endif

void FileInfoThread::run()
{
    forever {
        bool updateFiles = false;
        QMutexLocker locker(&mutex);
        if (abort) {
            return;
        }
        if (currentPath.isEmpty() || !needUpdate)
            condition.wait(&mutex);

        if (abort) {
            return;
        }

        if (!currentPath.isEmpty()) {
            updateFiles = true;
        }
        if (updateFiles)
            getFileInfos(currentPath);
        locker.unlock();
    }
}


void FileInfoThread::getFileInfos(const QString &path)
{
    QDir::Filters filter;
    if (caseSensitive)
        filter = QDir::CaseSensitive;
    if (showFiles)
        filter = filter | QDir::Files;
    if (showDirs)
        filter = filter | QDir::AllDirs | QDir::Drives;
    if (!showDotAndDotDot)
        filter = filter | QDir::NoDot | QDir::NoDotDot;
    else if (path == rootPath)
        filter = filter | QDir::NoDotDot;
    if (showHidden)
        filter = filter | QDir::Hidden;
    if (showOnlyReadable)
        filter = filter | QDir::Readable;
    if (showDirsFirst)
        sortFlags = sortFlags | QDir::DirsFirst;

    QDir currentDir(path, QString(), sortFlags);
    QList<FileProperty> filePropertyList;

    const QFileInfoList fileInfoList = currentDir.entryInfoList(nameFilters, filter, sortFlags);

    if (!fileInfoList.isEmpty()) {
        filePropertyList.reserve(fileInfoList.count());
        for (const QFileInfo &info : fileInfoList) {
            //qDebug() << "Adding file : " << info.fileName() << "to list ";
            filePropertyList << FileProperty(info);
        }
        if (folderUpdate) {
            int fromIndex = 0;
            int toIndex = currentFileList.size()-1;
            findChangeRange(filePropertyList, fromIndex, toIndex);
            folderUpdate = false;
            currentFileList = filePropertyList;
            //qDebug() << "emit directoryUpdated : " << fromIndex << " " << toIndex;
            emit directoryUpdated(path, filePropertyList, fromIndex, toIndex);
        } else {
            currentFileList = filePropertyList;
            if (sortUpdate) {
                emit sortFinished(filePropertyList);
                sortUpdate = false;
            } else
                emit directoryChanged(path, filePropertyList);
        }
    } else {
        // The directory is empty
        if (folderUpdate) {
            int fromIndex = 0;
            int toIndex = currentFileList.size()-1;
            folderUpdate = false;
            currentFileList.clear();
            emit directoryUpdated(path, filePropertyList, fromIndex, toIndex);
        } else {
            currentFileList.clear();
            emit directoryChanged(path, filePropertyList);
        }
    }
    needUpdate = false;
}

void FileInfoThread::findChangeRange(const QList<FileProperty> &list, int &fromIndex, int &toIndex)
{
    if (currentFileList.size() == 0) {
        fromIndex = 0;
        toIndex = list.size();
        return;
    }

    int i;
    int listSize = list.size() < currentFileList.size() ? list.size() : currentFileList.size();
    bool changeFound = false;

    for (i=0; i < listSize; i++) {
        if (list.at(i) != currentFileList.at(i)) {
            changeFound = true;
            break;
        }
    }

    if (changeFound)
        fromIndex = i;
    else
        fromIndex = i-1;

    // For now I let the rest of the list be updated..
    toIndex = list.size() > currentFileList.size() ? list.size() - 1 : currentFileList.size() - 1;
}

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

#ifndef FILEINFOTHREAD_P_H
#define FILEINFOTHREAD_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QFileSystemWatcher>
#include <QFileInfo>
#include <QDir>

#include "fileproperty_p.h"

class FileInfoThread : public QThread
{
    Q_OBJECT

Q_SIGNALS:
    void directoryChanged(const QString &directory, const QList<FileProperty> &list) const;
    void directoryUpdated(const QString &directory, const QList<FileProperty> &list, int fromIndex, int toIndex) const;
    void sortFinished(const QList<FileProperty> &list) const;

public:
    FileInfoThread(QObject *parent = 0);
    ~FileInfoThread();

    void clear();
    void removePath(const QString &path);
    void setPath(const QString &path);
    void setRootPath(const QString &path);
    void setSortFlags(QDir::SortFlags flags);
    void setNameFilters(const QStringList & nameFilters);
    void setShowFiles(bool show);
    void setShowDirs(bool showFolders);
    void setShowDirsFirst(bool show);
    void setShowDotAndDotDot(bool on);
    void setShowHidden(bool on);
    void setShowOnlyReadable(bool on);
    void setCaseSensitive(bool on);

public Q_SLOTS:
#ifndef QT_NO_FILESYSTEMWATCHER
    void dirChanged(const QString &directoryPath);
    void updateFile(const QString &path);
#endif

protected:
    void run();
    void getFileInfos(const QString &path);
    void findChangeRange(const QList<FileProperty> &list, int &fromIndex, int &toIndex);

private:
    QMutex mutex;
    QWaitCondition condition;
    volatile bool abort;

#ifndef QT_NO_FILESYSTEMWATCHER
    QFileSystemWatcher *watcher;
#endif
    QList<FileProperty> currentFileList;
    QDir::SortFlags sortFlags;
    QString currentPath;
    QString rootPath;
    QStringList nameFilters;
    bool needUpdate;
    bool folderUpdate;
    bool sortUpdate;
    bool showFiles;
    bool showDirs;
    bool showDirsFirst;
    bool showDotAndDotDot;
    bool showHidden;
    bool showOnlyReadable;
    bool caseSensitive;
};

#endif // FILEINFOTHREAD_P_H

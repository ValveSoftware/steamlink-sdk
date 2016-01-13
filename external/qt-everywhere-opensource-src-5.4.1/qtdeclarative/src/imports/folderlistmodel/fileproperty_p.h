/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the examples of the Qt Toolkit.
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

#ifndef FILEPROPERTY_P_H
#define FILEPROPERTY_P_H

#include <QFileInfo>
#include <QDateTime>

class FileProperty
{
public:
    FileProperty(const QFileInfo &info)
    {
        mFileName = info.fileName();
        mFilePath = info.filePath();
        mBaseName = info.baseName();
        mSize = info.size();
        mSuffix = info.completeSuffix();
        mIsDir = info.isDir();
        mIsFile = info.isFile();
        mLastModified = info.lastModified();
        mLastRead = info.lastRead();
    }
    ~FileProperty()
    {}

    inline QString fileName() const { return mFileName; }
    inline QString filePath() const { return mFilePath; }
    inline QString baseName() const { return mBaseName; }
    inline qint64 size() const { return mSize; }
    inline QString suffix() const { return mSuffix; }
    inline bool isDir() const { return mIsDir; }
    inline bool isFile() const { return mIsFile; }
    inline QDateTime lastModified() const { return mLastModified; }
    inline QDateTime lastRead() const { return mLastRead; }

    inline bool operator !=(const FileProperty &fileInfo) const {
        return !operator==(fileInfo);
    }
    bool operator ==(const FileProperty &property) const {
        return ((mFileName == property.mFileName) && (isDir() == property.isDir()));
    }

private:
    QString mFileName;
    QString mFilePath;
    QString mBaseName;
    QString mSuffix;
    qint64 mSize;
    bool mIsDir;
    bool mIsFile;
    QDateTime mLastModified;
    QDateTime mLastRead;
};
#endif // FILEPROPERTY_P_H

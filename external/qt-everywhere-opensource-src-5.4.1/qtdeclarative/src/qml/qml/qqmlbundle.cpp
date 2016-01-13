/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#include "qqmlbundle_p.h"
#include <iostream>
#include <cstdlib>

static const unsigned char qmlBundleHeaderData[] = { 255, 'q', 'm', 'l', 'd', 'i', 'r', 255 };
static const unsigned int qmlBundleHeaderLength = 8;

//
// Entries
//
QString QQmlBundle::FileEntry::fileName() const
{
    return QString((QChar *)&data[0], fileNameLength / sizeof(QChar));
}

bool QQmlBundle::FileEntry::isFileName(const QString &fileName) const
{
    return fileName.length() * sizeof(QChar) == (unsigned)fileNameLength &&
           0 == ::memcmp(fileName.constData(), &data[0], fileNameLength);
}

const char *QQmlBundle::FileEntry::contents() const {
    return &data[fileNameLength];
}

quint32 QQmlBundle::FileEntry::fileSize() const
{
    return size - (sizeof(FileEntry) + fileNameLength);
}


//
// QQmlBundle
//
QQmlBundle::QQmlBundle(const QString &fileName)
: file(fileName),
  buffer(0),
  bufferSize(0),
  opened(false),
  headerWritten(false)
{
}

QQmlBundle::~QQmlBundle()
{
    close();
}

bool QQmlBundle::open(QIODevice::OpenMode mode)
{
    if (!opened) {
        if (!file.open(mode))
            return false;

        bufferSize = file.size();
        buffer = file.map(0, bufferSize);

        if (bufferSize == 0 ||
            (bufferSize >= 8 && 0 == ::memcmp(buffer, qmlBundleHeaderData, qmlBundleHeaderLength))) {
            opened = true;
            headerWritten = false;
            return true;
        } else {
            close();
            return false;
        }
    }
    return true;
}

void QQmlBundle::close()
{
    if (opened) {
        opened = false;
        headerWritten = false;
        file.unmap(buffer);
        file.close();
    }
}

QList<const QQmlBundle::FileEntry *> QQmlBundle::files() const
{
    QList<const FileEntry *> files;
    const char *ptr = (const char *) buffer + qmlBundleHeaderLength;
    const char *end = (const char *) buffer + bufferSize;

    while (ptr < end) {
        const Entry *cmd = (const Entry *) ptr;

        switch (static_cast<Entry::Kind>(cmd->kind)) {
        case Entry::File: {
            const FileEntry *f = reinterpret_cast<const FileEntry *>(cmd);
            files.append(f);
        }   break;

        case Entry::Link:
        case Entry::Skip: {
            // Skip
        }   break;

        default:
            // throw an error
            return QList<const FileEntry *>();
        } // switch

        ptr += cmd->size;
        Q_ASSERT(ptr <= end); // throw an error
    }
    return files;
}

void QQmlBundle::remove(const FileEntry *entry)
{
    Q_ASSERT(entry->kind == Entry::File); // ### throw an error
    Q_ASSERT(file.isWritable());
    const_cast<FileEntry *>(entry)->kind = Entry::Skip;
}

int QQmlBundle::bundleHeaderLength()
{
    return qmlBundleHeaderLength;
}

bool QQmlBundle::isBundleHeader(const char *data, int size)
{
    if ((unsigned int)size < qmlBundleHeaderLength)
        return false;

    return 0 == ::memcmp(data, qmlBundleHeaderData, qmlBundleHeaderLength);
}

//
// find a some empty space we can use to insert new entries.
//
const QQmlBundle::Entry *QQmlBundle::findInsertPoint(quint32 size, qint32 *offset)
{
    const char *ptr = (const char *) buffer + qmlBundleHeaderLength;
    const char *end = (const char *) buffer + bufferSize;

    while (ptr < end) {
        const Entry *cmd = (const Entry *) ptr;

        if (cmd->kind == Entry::Skip && size + sizeof(RawEntry) < cmd->size) {
            *offset = ptr - ((const char *) buffer + qmlBundleHeaderLength);
            return cmd;
        }

        ptr += cmd->size;
        Q_ASSERT(ptr <= end); // throw an error
    }

    return 0;
}

const QQmlBundle::FileEntry *QQmlBundle::find(const QString &fileName) const
{
    const char *ptr = (const char *) buffer + qmlBundleHeaderLength;
    const char *end = (const char *) buffer + bufferSize;

    while (ptr < end) {
        const Entry *cmd = (const Entry *) ptr;

        if (cmd->kind == Entry::File) {
            const FileEntry *fileEntry = static_cast<const FileEntry *>(cmd);

            if (fileEntry->isFileName(fileName))
                return fileEntry;
        }

        ptr += cmd->size;
        Q_ASSERT(ptr <= end); // throw an error
    }

    return 0;
}

const QQmlBundle::FileEntry *QQmlBundle::link(const FileEntry *entry, const QString &linkName) const
{
    const char *ptr = (const char *) buffer + entry->link;

    while (ptr != (const char *)buffer) {
        const Entry *cmd = (const Entry *) ptr;
        Q_ASSERT(cmd->kind == Entry::Link);

        const FileEntry *fileEntry = static_cast<const FileEntry *>(cmd);
        if (fileEntry->fileName() == linkName)
            return fileEntry;

        ptr = (const char *) buffer + fileEntry->link;
    }

    return 0;
}

const QQmlBundle::FileEntry *QQmlBundle::find(const QChar *fileName, int length) const
{
    return find(QString::fromRawData(fileName, length));
}

bool QQmlBundle::add(const QString &name, const QString &fileName)
{
    if (!file.isWritable())
        return false;
    else if (find(fileName))
        return false;

    QFile inputFile(fileName);
    if (!inputFile.open(QFile::ReadOnly))
        return false;

    // ### use best-fit algorithm
    if (!file.atEnd())
        file.seek(file.size());

    FileEntry cmd;
    const quint32 inputFileSize = inputFile.size();

    cmd.kind = Entry::File;
    cmd.link = 0;
    cmd.size = sizeof(FileEntry) + name.length() * sizeof(QChar) + inputFileSize;
    cmd.fileNameLength = name.length() * sizeof(QChar);

    if (bufferSize == 0 && headerWritten == false) {
        file.write((const char *)qmlBundleHeaderData, qmlBundleHeaderLength);
        headerWritten = true;
    }

    file.write((const char *) &cmd, sizeof(FileEntry));
    file.write((const char *) name.constData(), name.length() * sizeof(QChar));

    uchar *source = inputFile.map(0, inputFileSize);
    file.write((const char *) source, inputFileSize);
    inputFile.unmap(source);
    return true;
}

bool QQmlBundle::add(const QString &fileName)
{
    return add(fileName, fileName);
}

bool QQmlBundle::addMetaLink(const QString &fileName,
                             const QString &linkName,
                             const QByteArray &data)
{
    if (!file.isWritable())
        return false;

    const FileEntry *fileEntry = find(fileName);
    if (!fileEntry)
        return false;

    // ### use best-fit algorithm
    if (!file.atEnd())
        file.seek(file.size());

    FileEntry cmd;

    const quint32 inputFileSize = data.size();

    cmd.kind = Entry::Link;
    cmd.link = fileEntry->link;
    cmd.size = sizeof(FileEntry) + linkName.length() * sizeof(QChar) + inputFileSize;
    cmd.fileNameLength = linkName.length() * sizeof(QChar);

    if (bufferSize == 0 && headerWritten == false) {
        file.write((const char *)qmlBundleHeaderData, qmlBundleHeaderLength);
        headerWritten = true;
    }

    const_cast<FileEntry *>(fileEntry)->link = file.size();

    file.write((const char *) &cmd, sizeof(FileEntry));
    file.write((const char *) linkName.constData(), linkName.length() * sizeof(QChar));
    file.write((const char *) data.constData(), inputFileSize);
    return true;
}

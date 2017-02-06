/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "directory.h"
#include <QDebug>

/*
    Directory constructor

    Initialize the saves directory and creates the file list
*/
Directory::Directory(QObject *parent) : QObject(parent)
{
    m_dir.cd(QDir::currentPath());

    // Go to the saved directory. if not found, create it
    m_saveDir = "saves";
    if (m_dir.cd(m_saveDir) == 0) {
        m_dir.mkdir(m_saveDir);
        m_dir.cd(m_saveDir);
    }
     m_filterList << "*.txt";

    refresh();
}

/*
    Directory::filesCount
    Returns the number of files
*/
int Directory::filesCount() const
{
    return m_fileList.size();
}

/*
    Function to append data into list property
*/
void appendFiles(QQmlListProperty<File> *property, File *file)
{
    Q_UNUSED(property)
    Q_UNUSED(file)
    // Do nothing. can't add to a directory using this method
}

/*
    Function called to retrieve file in the list using an index
*/
File* fileAt(QQmlListProperty<File> *property, int index)
{
    return static_cast< QList<File *> *>(property->data)->at(index);
}

/*
    Returns the number of files in the list
*/
int filesSize(QQmlListProperty<File> *property)
{
    return static_cast< QList<File *> *>(property->data)->size();
}

/*
    Function called to empty the list property contents
*/
void clearFilesPtr(QQmlListProperty<File> *property)
{
    return static_cast< QList<File *> *>(property->data)->clear();
}

/*
    Returns the list of files as a QQmlListProperty.
*/
QQmlListProperty<File> Directory::files()
{
    refresh();
    return QQmlListProperty<File>(this, &m_fileList, &appendFiles, &filesSize, &fileAt, &clearFilesPtr);
}

/*
    Return the name of the currently selected file.
*/
QString Directory::filename() const
{
    return currentFile.name();
}

/*
    Return the file's content as a string.
*/
QString Directory::fileContent() const
{
    return m_fileContent;
}

/*
    Set the file name of the current file.
*/
void Directory::setFilename(const QString &str)
{
    if (str != currentFile.name()) {
        currentFile.setName(str);
        emit filenameChanged();
    }
}

/*
    Set the content of the file as a string.
*/
void Directory::setFileContent(const QString &str)
{
    if (str != m_fileContent) {
        m_fileContent = str;
        emit fileContentChanged();
    }
}

/*
    Called from QML to save the file using the filename and content.
    Makes sure that the file has a .txt extension.
*/
void Directory::saveFile()
{
    if (currentFile.name().size() == 0) {
        qWarning() << "Cannot save file, empty filename.";
        return;
    }

    QString extendedName = currentFile.name();
    if (!currentFile.name().endsWith(".txt")) {
        extendedName.append(".txt");
    }

    QFile file(m_dir.filePath(extendedName));
    if (file.open(QFile::WriteOnly | QFile::Truncate)) {
        QTextStream outStream(&file);
        outStream << m_fileContent;
    }

    file.close();
    refresh();
    emit directoryChanged();
}

/*
    Load the contents of a file.
    Only loads files with a .txt extension
*/
void Directory::loadFile()
{
    m_fileContent.clear();
    QString extendedName = currentFile.name();
    if (!currentFile.name().endsWith(".txt")) {
        extendedName.append(".txt");
    }

    QFile file( m_dir.filePath(extendedName));
    if (file.open(QFile::ReadOnly )) {
        QTextStream inStream(&file);
        QString line;

        do {
            line = inStream.read(75);
            m_fileContent.append(line);
        } while (!line .isNull());
    }
    file.close();
}

/*
    Reloads the content of the files list. This is to ensure
    that the newly created files are added onto the list.
*/
void Directory::refresh()
{
    m_dirFiles = m_dir.entryList(m_filterList, QDir::Files, QDir::Name);
    m_fileList.clear();

    File * file;
    for (int i = 0; i < m_dirFiles.size(); i ++) {
        file = new File();

        if (m_dirFiles.at(i).endsWith(".txt")) {
            QString name = m_dirFiles.at(i);
            file->setName(name.remove(".txt", Qt::CaseSensitive));
        } else {
            file->setName(m_dirFiles.at(i));
        }
        m_fileList.append(file);
    }
}

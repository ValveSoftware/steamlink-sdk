/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
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
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
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

#ifndef DIRECTORY_H
#define DIRECTORY_H

#include "file.h"

#include <QDir>
#include <QStringList>
#include <QTextStream>
#include <QQmlListProperty>
#include <QObject>

class Directory : public QObject {
    Q_OBJECT

    // Number of files in the directory
    Q_PROPERTY(int filesCount READ filesCount)

    // List property containing file names as QString
    Q_PROPERTY(QQmlListProperty<File> files READ files CONSTANT)

    // File name of the text file to read/write
    Q_PROPERTY(QString filename READ filename WRITE setFilename NOTIFY filenameChanged)

    // Text content of the file
    Q_PROPERTY(QString fileContent READ fileContent WRITE setFileContent NOTIFY fileContentChanged)

public:
    Directory(QObject *parent = 0);

    // Properties' read functions
    int filesCount() const;
    QString filename() const;
    QString fileContent() const;
    QQmlListProperty<File> files();

    // Properties' write functions
    void setFilename(const QString &str);
    void setFileContent(const QString &str);

    // Accessible from QML
    Q_INVOKABLE void saveFile();
    Q_INVOKABLE void loadFile();

signals:
    void directoryChanged();
    void filenameChanged();
    void fileContentChanged();

private:
    QDir m_dir;
    QStringList m_dirFiles;
    File currentFile;
    QString m_saveDir;
    QStringList m_filterList;

    // Contains the file data in QString format
    QString m_fileContent;

    // Registered to QML in a plugin. Accessible from QML as a property of Directory
    QList<File *> m_fileList;

    // Refresh content of the directory
    void refresh();
};

#endif

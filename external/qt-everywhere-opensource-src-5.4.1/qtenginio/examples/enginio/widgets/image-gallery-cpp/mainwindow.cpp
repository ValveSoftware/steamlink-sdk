/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the examples of the Qt Toolkit.
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

#include <QtWidgets>
#include <QDebug>
#include <QFrame>
#include <QLabel>
#include <QListView>
#include <QMenu>
#include <QNetworkAccessManager>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#include <enginioclient.h>
#include <enginioreply.h>

// To get the backend right, we use a helper class in the example.
// Usually one would just insert the backend information below.
#include "backendhelper.h"

#include "imageobject.h"
#include "mainwindow.h"
#include "imagemodel.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("Enginio Image Gallery"));

    m_client = new EnginioClient(this);
    m_client->setBackendId(backendId("image-gallery"));

    m_model = new ImageModel(this);
    m_model->setClient(m_client);

    m_view = new QListView;
    m_view->setModel(m_model);
    m_view->setViewMode(QListView::IconMode);
    m_view->setGridSize(QSize(104, 104));

    m_fileDialog = new QFileDialog(this);
    m_fileDialog->setFileMode(QFileDialog::ExistingFile);
    m_fileDialog->setNameFilter("Image files (*.png *.jpg *.jpeg)");
    connect(m_fileDialog, SIGNAL(fileSelected(QString)),
            this, SLOT(fileSelected(QString)));

    m_uploadButton = new QPushButton("Upload image");
    connect(m_uploadButton, SIGNAL(clicked()),
            m_fileDialog, SLOT(show()));

    QFrame *frame = new QFrame;
    QVBoxLayout *windowLayout = new QVBoxLayout(frame);
    windowLayout->addWidget(m_view);
    windowLayout->addWidget(m_uploadButton);
    setCentralWidget(frame);

    queryImages();
}

QSize MainWindow::sizeHint() const
{
    return QSize(500, 700);
}

void MainWindow::queryImages()
{
    QJsonObject query;
    query.insert("objectType", QStringLiteral("objects.image"));
    QJsonObject fileObject;
    fileObject.insert("file", QJsonObject());

    // Use include parameter to request full "file" object for every result
    // object including image and thumbnail URLs.
    query.insert("include", fileObject);

    // Filter out objects which have not yet been uploaded completely
    QJsonObject filter;
    QJsonObject exists;
    exists.insert("$exists", true);
    filter.insert("file.id", exists);
    query.insert("query", filter);

    m_model->setQuery(query);
}

void MainWindow::error(EnginioReply *error)
{
    qWarning() << Q_FUNC_INFO << error;
}

void MainWindow::fileSelected(const QString &filePath)
{
    if (filePath.isEmpty())
        return;

    QJsonObject object;
    object.insert("objectType", QString("objects.image"));
    QString fileName = filePath.split(QDir::separator()).last();
    object.insert("name", fileName);
    object.insert("localPath", filePath);
    EnginioReply *reply = m_model->append(object);
    connect(reply, SIGNAL(finished(EnginioReply*)), this, SLOT(beginUpload(EnginioReply*)));
}

void MainWindow::beginUpload(EnginioReply *reply)
{
    reply->deleteLater();
    QString path = reply->data().value("localPath").toString();
    QString objectId = reply->data().value("id").toString();

    QJsonObject object;
    object.insert("id", objectId);
    object.insert("objectType", QStringLiteral("objects.image"));
    object.insert("propertyName", QStringLiteral("file"));

    QJsonObject fileObject;
    fileObject.insert(QStringLiteral("fileName"), path);

    QJsonObject uploadJson;
    uploadJson.insert(QStringLiteral("targetFileProperty"), object);
    uploadJson.insert(QStringLiteral("file"), fileObject);

    EnginioReply *upload = m_client->uploadFile(uploadJson, QUrl::fromUserInput(path));
    connect(upload, SIGNAL(finished(EnginioReply*)), upload, SLOT(deleteLater()));
}

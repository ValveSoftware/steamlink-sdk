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

#include <QtWidgets/qframe.h>
#include <QtWidgets/qlabel.h>
#include <QtWidgets/qlistview.h>
#include <QtWidgets/qpushbutton.h>
#include <QtWidgets/qtoolbar.h>
#include <QtWidgets/qinputdialog.h>
#include <QtWidgets/qlayout.h>

#include <QtCore/qjsondocument.h>
#include <QtCore/qjsonobject.h>

#include <Enginio/enginioclient.h>
#include <Enginio/enginioreply.h>

// To get the backend right, we use a helper class in the example.
// Usually one would just insert the backend information below.
#include "backendhelper.h"

#include "mainwindow.h"
#include "todosmodel.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("Enginio TODO example"));

    QByteArray EnginioBackendId = backendId("todo");

    //![client]
    m_client = new EnginioClient(this);
    m_client->setBackendId(EnginioBackendId);
    //![client]

    QObject::connect(m_client, &EnginioClient::error, this, &MainWindow::error);

    //![model]
    m_model = new TodosModel(this);
    m_model->setClient(m_client);

    QJsonObject query;
    query["objectType"] = QString::fromUtf8("objects.todos");
    m_model->setQuery(query);
    //![model]

    QToolBar *toolBar = new QToolBar(this);
    m_addNewButton = new QPushButton(toolBar);
    m_addNewButton->setText("&Add");
    QObject::connect(m_addNewButton, &QPushButton::clicked, this, &MainWindow::appendItem);

    m_removeButton = new QPushButton(toolBar);
    m_removeButton->setText("&Remove");
    m_removeButton->setEnabled(false);
    QObject::connect(m_removeButton, &QPushButton::clicked, this, &MainWindow::removeItem);

    m_toggleButton = new QPushButton(toolBar);
    m_toggleButton->setText("&Toggle Completed");
    m_toggleButton->setEnabled(false);
    QObject::connect(m_toggleButton, &QPushButton::clicked, this, &MainWindow::toggleCompleted);

    toolBar->addWidget(m_addNewButton);
    toolBar->addWidget(m_removeButton);
    toolBar->addWidget(m_toggleButton);

    m_view = new QTreeView(this);
    m_view->setAlternatingRowColors(true);
    QFrame *frame = new QFrame(this);
    QVBoxLayout *windowLayout = new QVBoxLayout(frame);
    windowLayout->addWidget(m_view);
    windowLayout->addWidget(toolBar);
    setCentralWidget(frame);

    //![assignModel]
    m_view->setModel(m_model);
    //![assignModel]

    m_view->setSelectionModel(new QItemSelectionModel(m_model, m_model));
    QObject::connect(m_view->selectionModel(), &QItemSelectionModel::selectionChanged, this, &MainWindow::selectionChanged);
}

QSize MainWindow::sizeHint() const
{
    return QSize(400, 600);
}

void MainWindow::error(EnginioReply *error)
{
    qWarning() << Q_FUNC_INFO << error;
}

//![removeItem]
void MainWindow::removeItem()
{
    QModelIndex index = m_view->currentIndex();
    EnginioReply *reply = m_model->remove(index.row());
    QObject::connect(reply, &EnginioReply::finished, reply, &EnginioReply::deleteLater);
}
//![removeItem]

//![appendItem]
void MainWindow::appendItem()
{
    bool ok;
    QString text = QInputDialog::getText(this, tr("Create a new To Do")
                                         , tr("Title:"), QLineEdit::Normal
                                         , "", &ok);
    if (ok && !text.isEmpty()){
        QJsonObject object;
        object["title"] = text;
        object["completed"] = false; // By default a new To Do is not completed
        EnginioReply *reply = m_model->append(object);
        QObject::connect(reply, &EnginioReply::finished, reply, &EnginioReply::deleteLater);
    }
}
//![appendItem]

void MainWindow::toggleCompleted()
{
    QModelIndex index = m_view->currentIndex();
    bool completed = m_model->data(index, m_model->CompletedRole).toBool();
    m_model->setData(index, QVariant(!completed), TodosModel::CompletedRole);
}

void MainWindow::selectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    Q_UNUSED(deselected);
    bool value = selected.count();
    m_removeButton->setEnabled(value);
    m_toggleButton->setEnabled(value);
}

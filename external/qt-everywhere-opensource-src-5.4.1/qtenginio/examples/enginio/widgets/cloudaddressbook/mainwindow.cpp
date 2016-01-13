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

#include <QtCore/qbytearray.h>
#include <QtCore/qpair.h>

#include <QtCore/qjsondocument.h>
#include <QtCore/qjsonobject.h>
#include <QtCore/qsortfilterproxymodel.h>

#include <Enginio/enginioclient.h>
#include <Enginio/enginioreply.h>

#include <QtGui/qicon.h>
#include <QtWidgets/qpushbutton.h>

#include "addressbookmodel.h"

// To get the backend right, we use a helper class in the example.
// Usually one would just insert the backend information below.
#include "backendhelper.h"

#include "mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupUi(this);
    QByteArray EnginioBackendId = backendId("cloudaddressbook");

    //![client]
    client = new EnginioClient(this);
    client->setBackendId(EnginioBackendId);
    //![client]

    // this line is a debugging conviniance, passing all errors to qDebug()
    QObject::connect(client, &EnginioClient::error, this, &MainWindow::error);

    //![model]
    model = new AddressBookModel(this);
    model->setClient(client);

    QJsonObject query;
    query["objectType"] = QString::fromUtf8("objects.addressbook");
    model->setQuery(query);
    //![model]

    tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    //![assignProxyModel]
    sortFilterProxyModel = new QSortFilterProxyModel(this);
    sortFilterProxyModel->setSourceModel(model);
    tableView->setSortingEnabled(true);
    tableView->setModel(sortFilterProxyModel);
    //![assignProxyModel]

    // create the full text search based on searchEdit text value
    QObject::connect(searchEdit, &QLineEdit::returnPressed, this, &MainWindow::onSearchEdit);

    // clear the status bar when the search is finished
    QObject::connect(model, &AddressBookModel::searchFinished, this, &MainWindow::onSearchFinished);

    // append an empty row
    QObject::connect(add, &QPushButton::clicked, this, &MainWindow::onAddRow);

    // enable remove button when a row is selected
    QObject::connect(tableView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &MainWindow::onSelectionChanged);

    // remove selected rows
    QObject::connect(remove, &QPushButton::clicked, this, &MainWindow::onRemoveRow);
}

void MainWindow::onSearchEdit()
{
    model->newSearch(searchEdit->text());
    statusbar->showMessage(QStringLiteral("Searching for: ") + searchEdit->text());
    searchEdit->clear();
}

void MainWindow::onSearchFinished()
{
    statusbar->showMessage(QStringLiteral("Search finished"), 1500);
}

void MainWindow::onAddRow()
{
    QJsonObject item;
    item.insert("objectType", QString::fromUtf8("objects.addressbook"));
    EnginioReply *reply = model->append(item);
    QObject::connect(reply, &EnginioReply::finished, reply, &EnginioReply::deleteLater);
}

void MainWindow::onSelectionChanged()
{
    remove->setEnabled(tableView->selectionModel()->selectedRows().count());
}

void MainWindow::onRemoveRow()
{
    foreach (const QModelIndex &index, tableView->selectionModel()->selectedRows()) {
        QModelIndex sourceIndex = sortFilterProxyModel->mapToSource(index);
        EnginioReply *reply = model->remove(sourceIndex.row());
        QObject::connect(reply, &EnginioReply::finished, reply, &EnginioReply::deleteLater);
    }
}

void MainWindow::error(EnginioReply *error)
{
    qWarning() << Q_FUNC_INFO << error;
}



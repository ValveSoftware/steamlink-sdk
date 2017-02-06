/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtSensors module of the Qt Toolkit.
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

#include <QtCore/QDebug>
#include <QtWidgets/QTreeWidget>

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <qsensorgesture.h>
#include <qsensorgesturemanager.h>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    //! [0]

    QSensorGestureManager manager;

    Q_FOREACH (const QString &gesture, manager.gestureIds()) {

        QTreeWidgetItem *gestureId = new QTreeWidgetItem(ui->treeWidget);
        QStringList recognizerSignals =  manager.recognizerSignals(gesture);
        gestureId->setText(0,gesture);

        for (int i = 0; i < recognizerSignals.count(); i++) {
            QTreeWidgetItem *oneSignal = new QTreeWidgetItem(gestureId);
            oneSignal->setText(0,recognizerSignals.at(i));
        }
        ui->treeWidget->insertTopLevelItem(0,gestureId);
    }
    //! [0]


    ui->textEdit->setReadOnly(true);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::detectedShake(const QString &name)
{
    QString str = "<font size=+2><B>"+name+"</b></font><br>";
    ui->textEdit->insertHtml(str);
    ui->textEdit->ensureCursorVisible();
}

void MainWindow::on_pushButton_clicked()
{
    ui->textEdit->clear();
}

void MainWindow::onShake()
{
    QString str = "<font size=+2><B>onShake()</b></font><br>";
    ui->textEdit->insertHtml(str);
    ui->textEdit->ensureCursorVisible();
}

void MainWindow::on_startPushButton_clicked()
{
    if (ui->treeWidget->currentItem() == 0)
            return;
    QString currentRecognizer;

    if (ui->treeWidget->currentItem()->childCount() == 0) {
        currentRecognizer = ui->treeWidget->currentItem()->parent()->text(0);
    } else {
        currentRecognizer = ui->treeWidget->currentItem()->text(0);
    }

    if (recognizerMap.contains(currentRecognizer))
        return;
    //! [1]
    QSensorGestureManager manager;
    QSensorGesture *thisGesture = new QSensorGesture(QStringList() << currentRecognizer, this);

    if (currentRecognizer.contains("QtSensors.shake")) {
        connect(thisGesture,SIGNAL(shake()),
                this,SLOT(onShake()));
    }

    connect(thisGesture,SIGNAL(detected(QString)),
            this,SLOT(detectedShake(QString)));
    thisGesture->startDetection();

    //! [1]

    recognizerMap.insert(currentRecognizer,thisGesture);

    QString str = QString("<font size=+2><B>Started %1</b></font><br>").arg(currentRecognizer);
    ui->textEdit->insertHtml(str);
    ui->textEdit->ensureCursorVisible();
}

void MainWindow::on_stopPushButton_clicked()
{
    if (ui->treeWidget->currentItem() == 0)
            return;
    QString currentRecognizer;

    if (ui->treeWidget->currentItem()->childCount() == 0) {
        currentRecognizer = ui->treeWidget->currentItem()->parent()->text(0);
    } else {
        currentRecognizer = ui->treeWidget->currentItem()->text(0);
    }

    if (!recognizerMap.contains(currentRecognizer))
        return;
    //! [2]

        recognizerMap[currentRecognizer]->stopDetection();

        if (currentRecognizer == "QtSensors.shake") {
            disconnect(recognizerMap[currentRecognizer],SIGNAL(shake()),
                       this,SLOT(onShake()));
        }
        disconnect(recognizerMap[currentRecognizer],SIGNAL(detected(QString)),
                   this,SLOT(detectedShake(QString)));
        //! [2]

        recognizerMap.take(currentRecognizer);

    QString str = QString("<font size=+2><B>Stopped %1</b></font><br>").arg(currentRecognizer);
    ui->textEdit->insertHtml(str);
    ui->textEdit->ensureCursorVisible();
}

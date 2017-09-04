/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtScxml module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
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

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QStringListModel>
#include <QScxmlStateMachine>

QT_USE_NAMESPACE

MainWindow::MainWindow(QScxmlStateMachine *machine, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MainWindow),
    m_machine(machine)
{
    ui->setupUi(this);

    connect(ui->digit0, &QAbstractButton::clicked, [this] {
        m_machine->submitEvent("DIGIT.0");
    });
    connect(ui->digit1, &QAbstractButton::clicked, [this] {
        m_machine->submitEvent("DIGIT.1");
    });
    connect(ui->digit2, &QAbstractButton::clicked, [this] {
        m_machine->submitEvent("DIGIT.2");
    });
    connect(ui->digit3, &QAbstractButton::clicked, [this] {
        m_machine->submitEvent("DIGIT.3");
    });
    connect(ui->digit4, &QAbstractButton::clicked, [this] {
        m_machine->submitEvent("DIGIT.4");
    });
    connect(ui->digit5, &QAbstractButton::clicked, [this] {
        m_machine->submitEvent("DIGIT.5");
    });
    connect(ui->digit6, &QAbstractButton::clicked, [this] {
        m_machine->submitEvent("DIGIT.6");
    });
    connect(ui->digit7, &QAbstractButton::clicked, [this] {
        m_machine->submitEvent("DIGIT.7");
    });
    connect(ui->digit8, &QAbstractButton::clicked, [this] {
        m_machine->submitEvent("DIGIT.8");
    });
    connect(ui->digit9, &QAbstractButton::clicked, [this] {
        m_machine->submitEvent("DIGIT.9");
    });
    connect(ui->point, &QAbstractButton::clicked, [this] {
        m_machine->submitEvent("POINT");
    });
    connect(ui->operPlus, &QAbstractButton::clicked, [this] {
        m_machine->submitEvent("OPER.PLUS");
    });
    connect(ui->operMinus, &QAbstractButton::clicked, [this] {
        m_machine->submitEvent("OPER.MINUS");
    });
    connect(ui->operStar, &QAbstractButton::clicked, [this] {
        m_machine->submitEvent("OPER.STAR");
    });
    connect(ui->operDiv, &QAbstractButton::clicked, [this] {
        m_machine->submitEvent("OPER.DIV");
    });
    connect(ui->equals, &QAbstractButton::clicked, [this] {
        m_machine->submitEvent("EQUALS");
    });
    connect(ui->c, &QAbstractButton::clicked, [this] {
        m_machine->submitEvent("C");
    });

    m_machine->connectToEvent(QLatin1String("updateDisplay"), this, [this](const QScxmlEvent &event) {
        const QString display = event.data().toMap().value("display").toString();
        ui->display->setText(display);
    });
}

MainWindow::~MainWindow()
{
    delete ui;
}


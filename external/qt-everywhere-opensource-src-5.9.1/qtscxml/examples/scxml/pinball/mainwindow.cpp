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
    m_ui(new Ui::MainWindow),
    m_machine(machine)
{
    m_ui->setupUi(this);

    // lights
    initAndConnect(QLatin1String("cLightOn"), m_ui->cLabel);
    initAndConnect(QLatin1String("rLightOn"), m_ui->rLabel);
    initAndConnect(QLatin1String("aLightOn"), m_ui->aLabel);
    initAndConnect(QLatin1String("zLightOn"), m_ui->zLabel);
    initAndConnect(QLatin1String("yLightOn"), m_ui->yLabel);
    initAndConnect(QLatin1String("hurryLightOn"), m_ui->hurryLabel);
    initAndConnect(QLatin1String("jackpotLightOn"), m_ui->jackpotLabel);
    initAndConnect(QLatin1String("gameOverLightOn"), m_ui->gameOverLabel);

    // help labels
    initAndConnect(QLatin1String("offState"), m_ui->offStateLabel);
    initAndConnect(QLatin1String("hurryStateOff"), m_ui->normalStateLabel);
    initAndConnect(QLatin1String("hurryStateOn"), m_ui->hurryStateLabel);
    initAndConnect(QLatin1String("jackpotStateOn"), m_ui->jackpotStateLabel);

    // context enablement
    initAndConnect(QLatin1String("offState"), m_ui->startButton);
    initAndConnect(QLatin1String("onState"), m_ui->cButton);
    initAndConnect(QLatin1String("onState"), m_ui->rButton);
    initAndConnect(QLatin1String("onState"), m_ui->aButton);
    initAndConnect(QLatin1String("onState"), m_ui->zButton);
    initAndConnect(QLatin1String("onState"), m_ui->yButton);
    initAndConnect(QLatin1String("onState"), m_ui->ballOutButton);

    // datamodel update
    m_machine->connectToEvent("updateScore", [this] (const QScxmlEvent &event) {
        const QVariant data = event.data();
        const QString highScore = data.toMap().value("highScore").toString();
        m_ui->highScoreLabel->setText(highScore);
        const QString score = data.toMap().value("score").toString();
        m_ui->scoreLabel->setText(score);
    });

    // gui interaction
    connect(m_ui->cButton, &QAbstractButton::clicked,
            [this] { m_machine->submitEvent("cLetterTriggered");
            });
    connect(m_ui->rButton, &QAbstractButton::clicked,
            [this] { m_machine->submitEvent("rLetterTriggered");
            });
    connect(m_ui->aButton, &QAbstractButton::clicked,
            [this] { m_machine->submitEvent("aLetterTriggered");
            });
    connect(m_ui->zButton, &QAbstractButton::clicked,
            [this] { m_machine->submitEvent("zLetterTriggered");
            });
    connect(m_ui->yButton, &QAbstractButton::clicked,
            [this] { m_machine->submitEvent("yLetterTriggered");
            });
    connect(m_ui->startButton, &QAbstractButton::clicked,
            [this] { m_machine->submitEvent("startTriggered");
            });
    connect(m_ui->ballOutButton, &QAbstractButton::clicked,
            [this] { m_machine->submitEvent("ballOutTriggered");
            });
}

MainWindow::~MainWindow()
{
    delete m_ui;
}

void MainWindow::initAndConnect(const QString &state, QWidget *widget)
{
    widget->setEnabled(m_machine->isActive(state));
    m_machine->connectToState(state, widget, &QWidget::setEnabled);
}

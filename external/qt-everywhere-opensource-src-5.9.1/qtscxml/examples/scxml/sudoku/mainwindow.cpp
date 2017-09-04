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

#include <QStringListModel>
#include <QScxmlStateMachine>
#include <QComboBox>
#include <QToolButton>
#include <QLabel>
#include <QGridLayout>
#include <QFile>
#include <QDir>
#include <QTextStream>

static int Size = 9;

QT_USE_NAMESPACE

static QVariantList emptyRow()
{
    QVariantList row;
    for (int i = 0; i < Size; i++)
        row.append(QVariant(0));
    return row;
}

static QVariantMap readSudoku(const QString &fileName)
{
    QFile input(fileName);
    input.open(QIODevice::ReadOnly | QIODevice::Text);
    QTextStream str(&input);
    const QString data = str.readAll();

    QVariantList initRowsVariant;
    const QStringList rows = data.split(QLatin1Char('\n'));
    for (int i = 0; i < Size; i++) {
        if (i < rows.count()) {
            QVariantList initRowVariant;
            const QStringList row = rows.at(i).split(QLatin1Char(','));
            for (int j = 0; j < Size; j++) {
                const int val = j < row.count()
                        ? row.at(j).toInt() % (Size + 1) : 0;
                initRowVariant.append(val);
            }
            initRowsVariant.append(QVariant(initRowVariant));
        } else {
            initRowsVariant.append(QVariant(emptyRow()));
        }
    }

    QVariantMap dataVariant;
    dataVariant.insert(QStringLiteral("initState"), initRowsVariant);

    return dataVariant;
}

MainWindow::MainWindow(QScxmlStateMachine *machine, QWidget *parent) :
    QWidget(parent),
    m_machine(machine)
{
    const QVector<QToolButton *> initVector(Size, nullptr);
    m_buttons = QVector<QVector<QToolButton *> >(Size, initVector);

    QGridLayout *layout = new QGridLayout(this);

    for (int i = 0; i < Size; i++) {
        for (int j = 0; j < Size; j++) {
            QToolButton *button = new QToolButton(this);
            button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
            layout->addWidget(button, i + i / 3, j + j / 3);
            m_buttons[i][j] = button;
            connect(button, &QToolButton::clicked, [this, i, j] () {
                QVariantMap data;
                data.insert(QStringLiteral("x"), i);
                data.insert(QStringLiteral("y"), j);
                m_machine->submitEvent("tap", data);
            });
        }
    }

    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 2; j++) {
            QFrame *hFrame = new QFrame(this);
            hFrame->setFrameShape(QFrame::HLine);
            layout->addWidget(hFrame, 4 * j + 3, 4 * i, 1, 3);

            QFrame *vFrame = new QFrame(this);
            vFrame->setFrameShape(QFrame::VLine);
            layout->addWidget(vFrame, 4 * i, 4 * j + 3, 3, 1);
        }
    }

    m_startButton = new QToolButton(this);
    m_startButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_startButton->setText(tr("Start"));
    layout->addWidget(m_startButton, Size + 3, 0, 1, 3);

    connect(m_startButton, &QAbstractButton::clicked,
            [this] {
        if (m_machine->isActive("playing"))
            m_machine->submitEvent("stop");
        else
            m_machine->submitEvent("start");
    });

    m_label = new QLabel(tr("unsolved"));
    m_label->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_label, Size + 3, 4, 1, 3);

    m_undoButton = new QToolButton(this);
    m_undoButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_undoButton->setText(tr("Undo"));
    m_undoButton->setEnabled(false);
    layout->addWidget(m_undoButton, Size + 3, 8, 1, 3);

    connect(m_undoButton, &QAbstractButton::clicked,
            [this] {
        m_machine->submitEvent("undo");
    });

    m_chooser = new QComboBox(this);
    layout->addWidget(m_chooser, Size + 4, 0, 1, 11);

    QDir dataDir(QLatin1String(":/data"));
    QFileInfoList sudokuFiles = dataDir.entryInfoList(QStringList() << "*.data");
    foreach (const QFileInfo &sudokuFile, sudokuFiles)
        m_chooser->addItem(sudokuFile.completeBaseName(), sudokuFile.absoluteFilePath());

    connect(m_chooser, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            [this] (int index) {
        const QString sudokuFile = m_chooser->itemData(index).toString();
        const QVariantMap initValues = readSudoku(sudokuFile);
        m_machine->submitEvent("setup", initValues);
    });

    const QVariantMap initValues = readSudoku(m_chooser->itemData(0).toString());
    m_machine->setInitialValues(initValues);

    m_machine->connectToState("playing", [this] (bool playing) {
        if (playing) {
            m_startButton->setText(tr("Stop"));
            m_undoButton->setEnabled(true);
            m_chooser->setEnabled(false);
        } else {
            m_startButton->setText(tr("Start"));
            m_undoButton->setEnabled(false);
            m_chooser->setEnabled(true);
        }
    });

    m_machine->connectToState("solved", [this] (bool solved) {
        if (solved)
            m_label->setText(tr("SOLVED !!!"));
        else
            m_label->setText(tr("unsolved"));
    });

    m_machine->connectToEvent("updateGUI", [this] (const QScxmlEvent &event) {
        const QVariant data = event.data();

        const QVariantList currentRows = data.toMap().value("currentState").toList();
        for (int i = 0; i < currentRows.count(); i++) {
            const QVariantList row = currentRows.at(i).toList();
            for (int j = 0; j < row.count(); j++) {
                const int value = row.at(j).toInt();
                const QString text = value ? QString::number(value) : QString();
                m_buttons[i][j]->setText(text);
            }
        }

        const bool active = m_machine->isActive("playing");

        const QVariantList initRows = data.toMap().value("initState").toList();
        for (int i = 0; i < initRows.count(); i++) {
            const QVariantList row = initRows.at(i).toList();
            for (int j = 0; j < row.count(); j++) {
                const int value = row.at(j).toInt();
                const bool enabled = !value && active; // enable only zeroes from initState
                m_buttons[i][j]->setEnabled(enabled);
            }
        }
    });

    setLayout(layout);
}

MainWindow::~MainWindow()
{
}


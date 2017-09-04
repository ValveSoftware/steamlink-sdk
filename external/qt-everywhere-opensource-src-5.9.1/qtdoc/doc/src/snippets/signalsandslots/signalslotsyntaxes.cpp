/****************************************************************************
**
** Copyright (C) 2017 Sze Howe Koh <szehowe.koh@gmail.com>
** Contact: https://www.qt.io/licensing/
**
** This file is part of the documentation of the Qt Toolkit.
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

#include "signalslotsyntaxes.h"
#include <QQuickWidget>
#include <QQuickItem>
#include <QAudioInput>

void DemoWidget::demoImplicitConversion() {
//! [implicitconversion]
    auto slider = new QSlider(this);
    auto doubleSpinBox = new QDoubleSpinBox(this);

    // OK: The compiler can convert an int into a double
    connect(slider, &QSlider::valueChanged,
            doubleSpinBox, &QDoubleSpinBox::setValue);

    // ERROR: The string table doesn't contain conversion information
    connect(slider, SIGNAL(valueChanged(int)),
            doubleSpinBox, SLOT(setValue(double)));
//! [implicitconversion]
}

void DemoWidget::demoTypeResolution() {
//! [typeresolution]
    auto audioInput = new QAudioInput(QAudioFormat(), this);
    auto widget = new QWidget(this);

    // OK
    connect(audioInput, SIGNAL(stateChanged(QAudio::State)),
            widget, SLOT(show()));

    // ERROR: The strings "State" and "QAudio::State" don't match
    using namespace QAudio;
    connect(audioInput, SIGNAL(stateChanged(State)),
            widget, SLOT(show()));

    // ...
//! [typeresolution]
}


//! [lambda]
TextSender::TextSender(QWidget *parent) : QWidget(parent) {
    lineEdit = new QLineEdit(this);
    button = new QPushButton("Send", this);

    connect(button, &QPushButton::clicked, [=] {
        emit textCompleted(lineEdit->text());
    });

    // ...
}
//! [lambda]


void DemoWidget::demoCrossLanguageConnect()
{
//! [crosslanguage]
    auto cppObj = new CppGui(this);
    auto quickWidget = new QQuickWidget(QUrl("QmlGui.qml"), this);
    auto qmlObj = quickWidget->rootObject();

    // Connect QML signal to C++ slot
    connect(qmlObj, SIGNAL(qmlSignal(QString)),
            cppObj, SLOT(cppSlot(QString)));

    // Connect C++ signal to QML slot
    connect(cppObj, SIGNAL(cppSignal(QVariant)),
            qmlObj, SLOT(qmlSlot(QVariant)));
//! [crosslanguage]
}


//! [defaultparams]
DemoWidget::DemoWidget(QWidget *parent) : QWidget(parent) {

    // OK: printNumber() will be called with a default value of 42
    connect(qApp, SIGNAL(aboutToQuit()),
            this, SLOT(printNumber()));

    // ERROR: Compiler requires compatible arguments
    connect(qApp, &QCoreApplication::aboutToQuit,
            this, &DemoWidget::printNumber);
}
//! [defaultparams]


void DemoWidget::demoOverloadConnect()
{
//! [overload]
    auto slider = new QSlider(this);
    auto lcd = new QLCDNumber(this);

    // String-based syntax
    connect(slider, SIGNAL(valueChanged(int)),
            lcd, SLOT(display(int)));

    // Functor-based syntax, first alternative
    connect(slider, &QSlider::valueChanged,
            lcd, static_cast<void (QLCDNumber::*)(int)>(&QLCDNumber::display));

    // Functor-based syntax, second alternative
    void (QLCDNumber::*mySlot)(int) = &QLCDNumber::display;
    connect(slider, &QSlider::valueChanged,
            lcd, mySlot);

    // Functor-based syntax, third alternative
    connect(slider, &QSlider::valueChanged,
            lcd, QOverload<int>::of(&QLCDNumber::display));

    // Functor-based syntax, fourth alternative (requires C++14)
    connect(slider, &QSlider::valueChanged,
            lcd, qOverload<int>(&QLCDNumber::display));
//! [overload]
}

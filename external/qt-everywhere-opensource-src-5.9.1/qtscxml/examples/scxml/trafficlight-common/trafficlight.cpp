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

#include "trafficlight.h"

#include <QtWidgets>

class TrafficLightWidget : public QWidget
{
public:
    TrafficLightWidget(QWidget *parent = 0)
        : QWidget(parent), m_background(QLatin1String(":/background.png"))
    {
        QVBoxLayout *vbox = new QVBoxLayout(this);
        vbox->setContentsMargins(0, 40, 0, 80);
        m_red = new LightWidget(QLatin1String(":/red.png"));
        vbox->addWidget(m_red, 0, Qt::AlignHCenter);
        m_yellow = new LightWidget(QLatin1String(":/yellow.png"));
        vbox->addWidget(m_yellow, 0, Qt::AlignHCenter);
        m_green = new LightWidget(QLatin1String(":/green.png"));
        vbox->addWidget(m_green, 0, Qt::AlignHCenter);
        setLayout(vbox);
    }

    LightWidget *redLight() const
    { return m_red; }
    LightWidget *yellowLight() const
    { return m_yellow; }
    LightWidget *greenLight() const
    { return m_green; }

    virtual void paintEvent(QPaintEvent *) Q_DECL_OVERRIDE
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.drawImage(0, 0, m_background);
    }

    virtual QSize sizeHint() const Q_DECL_OVERRIDE
    {
        return m_background.size();
    }

private:
    QImage m_background;
    LightWidget *m_red;
    LightWidget *m_yellow;
    LightWidget *m_green;
};

TrafficLight::TrafficLight(QScxmlStateMachine *machine, QWidget *parent)
    : QWidget(parent)
    , m_machine(machine)
{
    TrafficLightWidget *widget = new TrafficLightWidget(this);
    setFixedSize(widget->sizeHint());

    machine->connectToState(QStringLiteral("red"),
                            widget->redLight(), &LightWidget::switchLight);
    machine->connectToState(QStringLiteral("redGoingGreen"),
                            widget->redLight(), &LightWidget::switchLight);
    machine->connectToState(QStringLiteral("yellow"),
                            widget->yellowLight(), &LightWidget::switchLight);
    machine->connectToState(QStringLiteral("blinking"),
                            widget->yellowLight(), &LightWidget::switchLight);
    machine->connectToState(QStringLiteral("green"),
                            widget->greenLight(), &LightWidget::switchLight);

    QAbstractButton *button = new ButtonWidget(this);
    auto setButtonGeometry = [this, button](){
        QSize buttonSize = button->sizeHint();
        button->setGeometry(width() - buttonSize.width() - 20, height() - buttonSize.height() - 20,
                            buttonSize.width(), buttonSize.height());
    };
    connect(button, &QAbstractButton::toggled, this, setButtonGeometry);
    setButtonGeometry();

    connect(button, &QAbstractButton::toggled, this, &TrafficLight::toggleWorking);
}

void TrafficLight::toggleWorking(bool pause)
{
    m_machine->submitEvent(pause ? "smash" : "repair");
}

LightWidget::LightWidget(const QString &image, QWidget *parent)
    : QWidget(parent)
    , m_image(image)
    , m_on(false)
{}

bool LightWidget::isOn() const
{ return m_on; }

void LightWidget::setOn(bool on)
{
    if (on == m_on)
        return;
    m_on = on;
    update();
}

void LightWidget::switchLight(bool onoff)
{ setOn(onoff); }

void LightWidget::paintEvent(QPaintEvent *)
{
    if (!m_on)
        return;
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.drawImage(0, 0, m_image);
}

QSize LightWidget::sizeHint() const
{
    return m_image.size();
}

ButtonWidget::ButtonWidget(QWidget *parent) :
    QAbstractButton(parent), m_playIcon(QLatin1String(":/play.png")),
    m_pauseIcon(QLatin1String(":/pause.png"))
{
    setCheckable(true);
}

void ButtonWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.drawImage(0, 0, isChecked() ? m_playIcon : m_pauseIcon);
}

QSize ButtonWidget::sizeHint() const
{
    return isChecked() ? m_playIcon.size() : m_pauseIcon.size();
}

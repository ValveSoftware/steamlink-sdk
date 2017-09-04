/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
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
#include "fullscreennotification.h"

#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QSequentialAnimationGroup>

FullScreenNotification::FullScreenNotification(QWidget *parent)
    : QLabel(parent)
    , m_previouslyVisible(false)
{
    setText(tr("You are now in full screen mode. Press ESC to quit!"));
    setStyleSheet(
        "font-size: 24px;"
        "color: white;"
        "background-color: black;"
        "border-color: white;"
        "border-width: 2px;"
        "border-style: solid;"
        "padding: 100px");
    setAttribute(Qt::WA_TransparentForMouseEvents);

    auto effect = new QGraphicsOpacityEffect;
    effect->setOpacity(1);
    setGraphicsEffect(effect);

    auto animations = new QSequentialAnimationGroup(this);
    animations->addPause(3000);
    auto opacityAnimation = new QPropertyAnimation(effect, "opacity", animations);
    opacityAnimation->setDuration(2000);
    opacityAnimation->setStartValue(1.0);
    opacityAnimation->setEndValue(0.0);
    opacityAnimation->setEasingCurve(QEasingCurve::OutQuad);
    animations->addAnimation(opacityAnimation);

    connect(this, &FullScreenNotification::shown,
            [animations](){ animations->start(); });

    connect(animations, &QAbstractAnimation::finished,
            [this](){ this->hide(); });
}

void FullScreenNotification::showEvent(QShowEvent *event)
{
    QLabel::showEvent(event);
    if (!m_previouslyVisible && isVisible())
        emit shown();
    m_previouslyVisible = isVisible();
}

/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the demonstration applications of the Qt Toolkit.
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

#include "featurepermissionbar.h"

#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QUrl>
#include <QPropertyAnimation>

static const int defaultHeight = 30;

static QString textForPermissionType(QWebEnginePage::Feature type)
{
    switch (type) {
    case QWebEnginePage::Notifications:
        return QObject::tr("use desktop notifications");
    case QWebEnginePage::Geolocation:
        return QObject::tr("use your position");
    case QWebEnginePage::MediaAudioCapture:
        return QObject::tr("use your microphone");
    case QWebEnginePage::MediaVideoCapture:
        return QObject::tr("use your camera");
    case QWebEnginePage::MediaAudioVideoCapture:
        return QObject::tr("use your camera and microphone");
    case QWebEnginePage::MouseLock:
        return QObject::tr("lock your mouse");
    default:
        Q_UNREACHABLE();
    }
    return QString();
}

FeaturePermissionBar::FeaturePermissionBar(QWidget *view)
    : QWidget(view)
    , m_messageLabel(new QLabel(this))
{
    setAutoFillBackground(true);
    QHBoxLayout *l = new QHBoxLayout;
    setLayout(l);
    l->setContentsMargins(defaultHeight, 0, 0, 0);
    l->addWidget(m_messageLabel);
    l->addStretch();
    QPushButton *allowButton = new QPushButton(tr("Allow"), this);
    QPushButton *denyButton = new QPushButton(tr("Deny"), this);
    QPushButton *discardButton = new QPushButton(QIcon(QStringLiteral(":closetab.png")), QString(), this);
    connect(allowButton, &QPushButton::clicked, this, &FeaturePermissionBar::permissionGranted);
    connect(denyButton, &QPushButton::clicked, this, &FeaturePermissionBar::permissionDenied);
    connect(discardButton, &QPushButton::clicked, this, &FeaturePermissionBar::permissionUnknown);
    connect(allowButton, &QPushButton::clicked, this, &QObject::deleteLater);
    connect(denyButton, &QPushButton::clicked, this, &QObject::deleteLater);
    connect(discardButton, &QPushButton::clicked, this, &QObject::deleteLater);
    l->addWidget(denyButton);
    l->addWidget(allowButton);
    l->addWidget(discardButton);
    setGeometry(0, -defaultHeight, view->width(), defaultHeight);
}

void FeaturePermissionBar::requestPermission(const QUrl &securityOrigin, QWebEnginePage::Feature feature)
{
    m_securityOrigin = securityOrigin;
    m_feature = feature;
    m_messageLabel->setText(tr("%1 wants to %2.").arg(securityOrigin.host()).arg(textForPermissionType(feature)));
    show();
    // Ease in
    QPropertyAnimation *animation = new QPropertyAnimation(this);
    animation->setTargetObject(this);
    animation->setPropertyName(QByteArrayLiteral("pos"));
    animation->setDuration(300);
    animation->setStartValue(QVariant::fromValue(pos()));
    animation->setEndValue(QVariant::fromValue(QPoint(0,0)));
    animation->setEasingCurve(QEasingCurve::InOutQuad);
    animation->start(QPropertyAnimation::DeleteWhenStopped);
}

void FeaturePermissionBar::permissionDenied()
{
    emit featurePermissionProvided(m_securityOrigin, m_feature, QWebEnginePage::PermissionDeniedByUser);
}

void FeaturePermissionBar::permissionGranted()
{
    emit featurePermissionProvided(m_securityOrigin, m_feature, QWebEnginePage::PermissionGrantedByUser);
}

void FeaturePermissionBar::permissionUnknown()
{
    emit featurePermissionProvided(m_securityOrigin, m_feature, QWebEnginePage::PermissionUnknown);
}

/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the demonstration applications of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
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
        return QObject::tr("desktop notifications");
    case QWebEnginePage::Geolocation:
        return QObject::tr("your position");
    case QWebEnginePage::MediaAudioCapture:
        return QObject::tr("your microphone");
    case QWebEnginePage::MediaVideoCapture:
        return QObject::tr("your camera");
    case QWebEnginePage::MediaAudioVideoCapture:
        return QObject::tr("your camera and microphone");
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
    connect(allowButton, &QPushButton::clicked, this, &FeaturePermissionBar::permissionGranted);
    connect(denyButton, &QPushButton::clicked, this, &FeaturePermissionBar::permissionDenied);
    QPushButton *discardButton = new QPushButton(QIcon(QStringLiteral(":closetab.png")), QString(), this);
    connect(discardButton, &QPushButton::clicked, this, &QObject::deleteLater);
    connect(allowButton, &QPushButton::clicked, this, &QObject::deleteLater);
    connect(denyButton, &QPushButton::clicked, this, &QObject::deleteLater);
    l->addWidget(denyButton);
    l->addWidget(allowButton);
    l->addWidget(discardButton);
    setGeometry(0, -defaultHeight, view->width(), defaultHeight);
}

void FeaturePermissionBar::requestPermission(const QUrl &securityOrigin, QWebEnginePage::Feature feature)
{
    m_securityOrigin = securityOrigin;
    m_feature = feature;
    m_messageLabel->setText(tr("%1 wants to use %2.").arg(securityOrigin.host()).arg(textForPermissionType(feature)));
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

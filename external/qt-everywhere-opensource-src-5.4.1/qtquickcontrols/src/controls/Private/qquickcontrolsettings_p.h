/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Quick Controls module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QQUICKCONTROLSETTINGS_P_H
#define QQUICKCONTROLSETTINGS_P_H

#include <QtCore/qurl.h>
#include <QtCore/qobject.h>

QT_BEGIN_NAMESPACE

class QQmlEngine;

class QQuickControlSettings : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QUrl style READ style NOTIFY styleChanged)
    Q_PROPERTY(QString styleName READ styleName WRITE setStyleName NOTIFY styleNameChanged)
    Q_PROPERTY(QString stylePath READ stylePath WRITE setStylePath NOTIFY stylePathChanged)
    Q_PROPERTY(qreal dpiScaleFactor READ dpiScaleFactor CONSTANT)
    Q_PROPERTY(qreal dragThreshold READ dragThreshold CONSTANT)
    Q_PROPERTY(bool hasTouchScreen READ hasTouchScreen CONSTANT)
    Q_PROPERTY(bool isMobile READ isMobile CONSTANT)

public:
    QQuickControlSettings(QQmlEngine *engine);

    QString style() const;

    QString styleName() const;
    void setStyleName(const QString &name);

    QString stylePath() const;
    void setStylePath(const QString &path);

    qreal dpiScaleFactor() const;
    qreal dragThreshold() const;

    bool hasTouchScreen() const;
    bool isMobile() const;

signals:
    void styleChanged();
    void styleNameChanged();
    void stylePathChanged();

private:
    QString styleFilePath() const;

    QString m_name;
    QString m_path;
};

QT_END_NAMESPACE

#endif // QQUICKCONTROLSETTINGS_P_H

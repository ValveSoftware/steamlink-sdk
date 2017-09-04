/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtSensors module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QMLSENSORGESTURE_H
#define QMLSENSORGESTURE_H

#include <QQmlParserStatus>
#include <QStringList>

QT_BEGIN_NAMESPACE

class QSensorGesture;
class QSensorGestureManager;

class QmlSensorGesture : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_PROPERTY(QStringList availableGestures READ availableGestures NOTIFY availableGesturesChanged)
    Q_PROPERTY(QStringList gestures READ gestures WRITE setGestures NOTIFY gesturesChanged)
    Q_PROPERTY(QStringList validGestures READ validGestures NOTIFY validGesturesChanged)
    Q_PROPERTY(QStringList invalidGestures READ invalidGestures NOTIFY invalidGesturesChanged)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_INTERFACES(QQmlParserStatus)

public:
    explicit QmlSensorGesture(QObject* parent = 0);
    ~QmlSensorGesture();
    void classBegin() override;
    void componentComplete() override;

Q_SIGNALS:
    void detected(const QString &gesture);
    void availableGesturesChanged();
    void gesturesChanged();
    void validGesturesChanged();
    void invalidGesturesChanged();
    void enabledChanged();

public:
    QStringList availableGestures();
    QStringList gestures() const;
    void setGestures(const QStringList& value);
    bool enabled() const;
    void setEnabled(bool value);
    QStringList validGestures() const;
    QStringList invalidGestures() const;

private:
    void deleteGesture();
    void createGesture();

private:
    QStringList gestureIds;
    bool isEnabled;
    bool initDone;
    QStringList gestureList;

    QSensorGesture* sensorGesture;
    QSensorGestureManager* sensorGestureManager;
};

QT_END_NAMESPACE

#endif

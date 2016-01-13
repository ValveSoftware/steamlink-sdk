/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtSensors module of the Qt Toolkit.
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
    void classBegin() Q_DECL_OVERRIDE;
    void componentComplete() Q_DECL_OVERRIDE;

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

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

#ifndef QMLSENSORRANGE_H
#define QMLSENSORRANGE_H

#include <QObject>

QT_BEGIN_NAMESPACE

class QmlSensorRange : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int minimum READ minimum)
    Q_PROPERTY(int maximum READ maximum)
public:
    explicit QmlSensorRange(QObject *parent = 0);
    ~QmlSensorRange();

    int minimum() const;
    void setMinumum(int mini) { min = mini; }

    int maximum() const;
    void setMaximum(int maxi) { max = maxi; }

private:

    int min;
    int max;
};

class QmlSensorOutputRange : public QObject
{
    Q_OBJECT
    Q_PROPERTY(qreal minimum READ minimum)
    Q_PROPERTY(qreal maximum READ maximum)
    Q_PROPERTY(qreal accuracy READ accuracy)
public:
    explicit QmlSensorOutputRange(QObject *parent = 0);
    ~QmlSensorOutputRange();

    qreal minimum() const;
    void setMinimum(int mini) { min = mini; }

    qreal maximum() const;
    void setMaximum(int maxi) { max = maxi; }

    qreal accuracy() const;
    void setAccuracy(int accu) { acc = accu; }

private:
    qreal min;
    qreal max;
    qreal acc;
};

QT_END_NAMESPACE
#endif

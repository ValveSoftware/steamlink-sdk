/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtSensors module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QDebug>
#include <QTimer>

#include "qtemplaterecognizer.h"

QTemplateGestureRecognizer::QTemplateGestureRecognizer(QObject *parent)
    : QSensorGestureRecognizer(parent)
{
}

QTemplateGestureRecognizer::~QTemplateGestureRecognizer()
{

}

void QTemplateGestureRecognizer::create()
{
    connect(&_timer,SIGNAL(timeout()),this,SLOT(timeout()));
    _timer.setInterval(1000);
}

bool QTemplateGestureRecognizer::start()
{
    Q_EMIT detected(id());
    _timer.start();
    return _timer.isActive();
}

bool QTemplateGestureRecognizer::stop()
{
    _timer.stop();
    return true;
}


bool QTemplateGestureRecognizer::isActive()
{
    return _timer.isActive();
}

QString QTemplateGestureRecognizer::id() const
{
    return QString("QtSensors.template");
}

void QTemplateGestureRecognizer::timeout()
{
    Q_EMIT detected(id());
}


QTemplateGestureRecognizer1::QTemplateGestureRecognizer1(QObject *parent)
    : QSensorGestureRecognizer(parent)
{
}

QTemplateGestureRecognizer1::~QTemplateGestureRecognizer1()
{

}

void QTemplateGestureRecognizer1::create()
{
    connect(&_timer,SIGNAL(timeout()),this,SLOT(timeout()));
    _timer.setInterval(500);
}

bool QTemplateGestureRecognizer1::start()
{
    Q_EMIT detected(id());
    _timer.start();
    return _timer.isActive();
}

bool QTemplateGestureRecognizer1::stop()
{
    _timer.stop();
    return true;
}


bool QTemplateGestureRecognizer1::isActive()
{
    return _timer.isActive();
}

QString QTemplateGestureRecognizer1::id() const
{
    return QString("QtSensors.template1");
}

void QTemplateGestureRecognizer1::timeout()
{
    Q_EMIT detected(id());
}

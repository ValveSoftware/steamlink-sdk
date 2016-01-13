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

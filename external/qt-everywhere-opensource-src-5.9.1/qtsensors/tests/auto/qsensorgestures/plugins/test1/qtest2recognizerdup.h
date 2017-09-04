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

#ifndef QTEST2RECOGNIZERDUP_H
#define QTEST2RECOGNIZERDUP_H

#include <qsensorgesturerecognizer.h>

class QTest2RecognizerDup : public QSensorGestureRecognizer
{
    Q_OBJECT

public:
    QTest2RecognizerDup(QObject *parent = 0);
    ~QTest2RecognizerDup();

    void create();

    QString id() const;
    bool start();
    bool stop();
    bool isActive();

    int thresholdTime() const;
    void setThresholdTime(int msec);

Q_SIGNALS:
    void test2_dup();

private:
    int timerTimeout;
    QTimer *timer;
    bool active;
};


#endif // QTEST2RECOGNIZERDUP_H

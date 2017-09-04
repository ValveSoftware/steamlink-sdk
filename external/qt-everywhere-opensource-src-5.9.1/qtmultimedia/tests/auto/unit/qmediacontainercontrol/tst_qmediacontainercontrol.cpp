/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
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

//TESTED_COMPONENT=src/multimedia

#include <QtTest/QtTest>
#include "qmediacontainercontrol.h"
#include "qmediarecorder.h"

#include "../qmultimedia_common/mockmediacontainercontrol.h"

//MaemoAPI-
class tst_QMediaContainerControl :public QObject
{
    Q_OBJECT

private slots:
    //to test the constructor
    void tst_mediacontainercontrol()
    {

        QObject obj;
        MockMediaContainerControl control(&obj);
        QStringList strlist=control.supportedContainers();
        QStringList strlist1;
        strlist1 << "wav" << "mp3" << "mov";
        QVERIFY(strlist[0]==strlist1[0]); //checking with "wav" mime type
        QVERIFY(strlist[1]==strlist1[1]); //checking with "mp3" mime type
        QVERIFY(strlist[2]==strlist1[2]); //checking with "mov" mime type

        control.setContainerFormat("wav");
        const QString str("wav");
        QVERIFY2(control.containerFormat() == str,"Failed");

        const QString str1("WAV format");
        QVERIFY2(control.containerDescription("wav") == str1,"FAILED");
    }
};

QTEST_MAIN(tst_QMediaContainerControl);
#include "tst_qmediacontainercontrol.moc"

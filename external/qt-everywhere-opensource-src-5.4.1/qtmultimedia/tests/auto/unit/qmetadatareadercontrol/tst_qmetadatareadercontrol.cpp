/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
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
#include <QtCore/QString>
#include <QtTest/QtTest>
#include <QtCore/QCoreApplication>
#include <QtMultimedia/qmediametadata.h>

#include "mockmetadatareadercontrol.h"

class tst_QMetaDataReaderControl : public QObject
{
    Q_OBJECT

private slots:
    // Test case for QMetaDataReaderControl
    void metaDataReaderControlConstructor();
    void metaDataReaderControlAvailableMetaData();
    void metaDataReaderControlIsMetaDataAvailable();
    void metaDataReaderControlMetaData();
    void metaDataReaderControlMetaDataAvailableChangedSignal();
    void metaDataReaderControlMetaDataChangedSignal();
};

QTEST_MAIN(tst_QMetaDataReaderControl);

/* Test case for constructor. */
void tst_QMetaDataReaderControl::metaDataReaderControlConstructor()
{
    MockMetaDataReaderControl *metaData = new MockMetaDataReaderControl();
    QVERIFY(metaData !=NULL);
    delete metaData;
}

/* Test case for availableMetaData() */
void tst_QMetaDataReaderControl::metaDataReaderControlAvailableMetaData()
{
    MockMetaDataReaderControl *metaData = new MockMetaDataReaderControl();
    QVERIFY(metaData !=NULL);
    metaData->availableMetaData() ;
    delete metaData;
}

/* Test case for availableMetaData */
void tst_QMetaDataReaderControl::metaDataReaderControlIsMetaDataAvailable ()
{
    MockMetaDataReaderControl *metaData = new MockMetaDataReaderControl();
    QVERIFY(metaData !=NULL);
    metaData->availableMetaData();
    delete metaData;
}

/* Test case for metaData */
void tst_QMetaDataReaderControl::metaDataReaderControlMetaData ()
{
    MockMetaDataReaderControl *metaData = new MockMetaDataReaderControl();
    QVERIFY(metaData !=NULL);
    metaData->metaData(QMediaMetaData::Title);
    delete metaData;
}

/* Test case for signal metaDataAvailableChanged */
void tst_QMetaDataReaderControl::metaDataReaderControlMetaDataAvailableChangedSignal ()
{
    MockMetaDataReaderControl *metaData = new MockMetaDataReaderControl();
    QVERIFY(metaData !=NULL);
    QSignalSpy spy(metaData,SIGNAL(metaDataAvailableChanged(bool)));
    metaData->setMetaDataAvailable(true);
    QVERIFY(spy.count() == 1);
    delete metaData;
}

 /* Test case for signal metaDataChanged */
void tst_QMetaDataReaderControl::metaDataReaderControlMetaDataChangedSignal ()
{
    MockMetaDataReaderControl *metaData = new MockMetaDataReaderControl();
    QVERIFY(metaData !=NULL);
    QSignalSpy spy(metaData,SIGNAL(metaDataChanged()));
    metaData->metaDataChanged();
    QVERIFY(spy.count () == 1);
    delete metaData;
}

#include "tst_qmetadatareadercontrol.moc"



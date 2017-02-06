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
#include <QtCore/QString>
#include <QtTest/QtTest>
#include <QtCore/QCoreApplication>
#include <QtMultimedia/qmediametadata.h>
#include "qmetadatawritercontrol.h"

#include "mockmetadatawritercontrol.h"

class tst_QMetaDataWriterControl: public QObject
{
    Q_OBJECT

public slots:
    void initTestCase();
    void cleanupTestCase();

private slots:
    void constructor();
};

void tst_QMetaDataWriterControl::initTestCase()
{

}

void tst_QMetaDataWriterControl::cleanupTestCase()
{

}

//MaemoAPI-1862:test constructor
void tst_QMetaDataWriterControl::constructor()
{
    QMetaDataWriterControl *mock = new MockMetaDataWriterControl();
    mock->availableMetaData();
    mock->isMetaDataAvailable();
    mock->isWritable();
    mock->metaData(QMediaMetaData::Title);
    mock->setMetaData(QMediaMetaData::Title, QVariant());
    ((MockMetaDataWriterControl*)mock)->setWritable();
    ((MockMetaDataWriterControl*)mock)->setMetaDataAvailable();
    delete mock;
}

QTEST_MAIN(tst_QMetaDataWriterControl);

#include "tst_qmetadatawritercontrol.moc"

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


#include <QtTest/QtTest>

#include "private/qabstractxmlforwarditerator_p.h"

/*!
 \class tst_QAbstractXmlForwardIterator
 \internal
 \since 4.4
 \brief Tests class QAbstractXmlForwardIterator.

 This test is not intended for testing the engine, but the functionality specific
 to the QAbstractXmlForwardIterator class.

 In other words, if you have an engine bug; don't add it here because it won't be
 tested properly. Instead add it to the test suite.

 */
class tst_QAbstractXmlForwardIterator : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void objectSize() const;
};

void tst_QAbstractXmlForwardIterator::objectSize() const
{
    /* We add sizeof(void *) times two. One for the
     * virtual table pointer, one for the d-pointer.
     *
     * The reason we check also allow (+ sizeof(void *) / 2) is that on 64 bit
     * the first member in QAbstractXmlForwardIterator gets padded to a pointer
     * address due to QSharedData not being sizeof(qptrdiff), but sizeof(qint32).
     */
    QVERIFY(   sizeof(QAbstractXmlForwardIterator<int>) == sizeof(QSharedData) + sizeof(void *) * 2
            || sizeof(QAbstractXmlForwardIterator<int>) == sizeof(QSharedData) + sizeof(void *) * 2 + sizeof(void *) / 2);
}

QTEST_MAIN(tst_QAbstractXmlForwardIterator)

#include "tst_qabstractxmlforwarditerator.moc"

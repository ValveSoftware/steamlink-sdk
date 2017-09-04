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


#include <QtTest/QtTest>

#include <QXmlNodeModelIndex>

/*!
 \class tst_QXmlNodeModelIndex
 \internal
 \since 4.4
 \brief Tests class QXmlNodeModelIndex.

 */
class tst_QXmlNodeModelIndex : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void copyConstructor() const;
    void constCorrectness() const;
    void assignmentOperator() const;
    void equalnessOperator() const;
    void inequalnessOperator() const;
    void objectSize() const;
    void internalPointer() const;
    void data() const;
    void additionalData() const;
    void isNull() const;
    void model() const;
    void withqHash() const;
};

void tst_QXmlNodeModelIndex::objectSize() const
{
    /* We can't do an exact comparison, because some platforms do padding. */
    QVERIFY(sizeof(QXmlNodeModelIndex) >= sizeof(QAbstractXmlNodeModel *) + sizeof(qint64) * 2);
}

void tst_QXmlNodeModelIndex::constCorrectness() const
{
    const QXmlNodeModelIndex index;
    /* All these functions should be const. */
    index.internalPointer();
    index.data();
    index.additionalData();
    index.isNull();
    index.model();
}

void tst_QXmlNodeModelIndex::assignmentOperator() const
{
    QXmlNodeModelIndex o1;
    const QXmlNodeModelIndex o2;
    o1 = o2;
    // TODO
}

void tst_QXmlNodeModelIndex::equalnessOperator() const
{
    QXmlNodeModelIndex o1;
    const QXmlNodeModelIndex o2;
    // TODO check const correctness
    o1 == o2;
}

void tst_QXmlNodeModelIndex::inequalnessOperator() const
{
    QXmlNodeModelIndex o1;
    const QXmlNodeModelIndex o2;
    // TODO check const correctness
    o1 != o2;
}

void tst_QXmlNodeModelIndex::copyConstructor() const
{
    /* Check that we can take a const reference. */
    {
        const QXmlNodeModelIndex index;
        const QXmlNodeModelIndex copy(index);
    }

    /* Take a copy of a temporary. */
    {
        /* The extra paranthesis silences a warning on win32-msvc. */
        const QXmlNodeModelIndex copy((QXmlNodeModelIndex()));
    }
}

void tst_QXmlNodeModelIndex::internalPointer() const
{
    /* Check default value. */
    {
        const QXmlNodeModelIndex index;
        QCOMPARE(index.internalPointer(), static_cast<void *>(0));
    }
}

void tst_QXmlNodeModelIndex::data() const
{
    /* Check default value. */
    {
        const QXmlNodeModelIndex index;
        QCOMPARE(index.data(), qint64(0));
    }

    // TODO check that the return value for data() is qint64.
}

void tst_QXmlNodeModelIndex::additionalData() const
{
    /* Check default value. */
    {
        const QXmlNodeModelIndex index;
        QCOMPARE(index.additionalData(), qint64(0));
    }

    // TODO check that the return value for data() is qint64.
}

void tst_QXmlNodeModelIndex::isNull() const
{
    /* Check default value. */
    {
        const QXmlNodeModelIndex index;
        QVERIFY(index.isNull());
    }

    /* Test default value on a temporary object. */
    {
        QVERIFY(QXmlNodeModelIndex().isNull());
    }
}

void tst_QXmlNodeModelIndex::model() const
{
    /* Check default value. */
    {
        const QXmlNodeModelIndex index;
        QCOMPARE(index.model(), static_cast<const QAbstractXmlNodeModel *>(0));
    }
}

void tst_QXmlNodeModelIndex::withqHash() const
{
    QXmlNodeModelIndex null;
    qHash(null);
    //Do something which means operator== must be available.
}

QTEST_MAIN(tst_QXmlNodeModelIndex)

#include "tst_qxmlnodemodelindex.moc"

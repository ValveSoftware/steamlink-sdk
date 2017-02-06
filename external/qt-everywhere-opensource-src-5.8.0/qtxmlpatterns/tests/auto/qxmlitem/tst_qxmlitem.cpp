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

#include <QXmlItem>

/*!
 \class tst_QXmlItem
 \internal
 \since 4.4
 \brief Tests class QXmlItem.
 */
class tst_QXmlItem : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void defaultConstructor() const;
    void copyConstructor() const;
    void copyConstructorFromQVariant() const;
    void copyConstructorFromQXmlNodeModelIndex() const;
    void assignmentOperator() const;
    void isNull() const;
    void isNode() const;
    void isAtomicValue() const;
    void toAtomicValue() const;
    void toNodeModelIndex() const;

    void objectSize() const;
    void constCorrectness() const;
    void withinQVariant() const;
};

void tst_QXmlItem::defaultConstructor() const
{
    {
        QXmlItem();
    }

    {
        QXmlItem();
        QXmlItem();
    }

    {
        QXmlItem();
        QXmlItem();
        QXmlItem();
    }
}

void tst_QXmlItem::copyConstructor() const
{
    /* Check that we can copy from a const reference. */
    {
        const QXmlItem item;
        const QXmlItem copy(item);
    }

    /* On a QXmlItem constructed from a null QVariant. */
    {
        const QXmlItem item((QVariant()));
        const QXmlItem copy(item);
    }

    /* On a QXmlItem constructed from a null QXmlNodeModelIndex. */
    {
        const QXmlItem item((QXmlNodeModelIndex()));
        const QXmlItem copy(item);
    }
}

void tst_QXmlItem::copyConstructorFromQVariant() const
{
    /* Construct & destruct a single value. */
    {
        const QXmlItem item(QVariant(QString::fromLatin1("dummy")));
    }

    /* Copy a null QVariant. */
    {
        const QXmlItem item((QVariant()));
        QVERIFY(item.isNull());
    }

}

void tst_QXmlItem::copyConstructorFromQXmlNodeModelIndex() const
{
    // TODO copy a valid model index.

    /* Construct from a null QXmlNodeModelIndex. */
    {
        const QXmlItem item((QXmlNodeModelIndex()));
        QVERIFY(item.isNull());
    }
}

void tst_QXmlItem::assignmentOperator() const
{
    /* Assign to self. */
    {
        /* With null value. */
        {
            QXmlItem item;
            item = item;
            item = item;
            item = item;
            item = item;
            item = item;
        }

        /* With the same atomic value. */
        {
            QXmlItem item(QVariant(QString::fromLatin1("dummy")));
            item = item;
            item = item;
            item = item;
            item = item;
            item = item;
        }

        /* With the same node. */
        {
            // TODO
        }

        /* With a QXmlItem constructed from a null QVariant. */
        {
            QXmlItem item((QVariant()));
            item = item;
            item = item;
            item = item;
            item = item;
            item = item;
        }

        /* With a QXmlItem constructed from a null QXmlNodeModelIndex. */
        {
            QXmlItem item((QXmlNodeModelIndex()));
            item = item;
            item = item;
            item = item;
            item = item;
            item = item;
        }
    }
}

void tst_QXmlItem::isNull() const
{
    /* Check default value. */
    {
        const QXmlItem item;
        QVERIFY(item.isNull());
    }

    /* On atomic value. */
    {
        const QXmlItem item(QVariant(3));
        QVERIFY(!item.isNull());
    }

    /* On a QXmlItem constructed from a null QVariant. */
    {
        const QXmlItem item((QVariant()));
        QVERIFY(item.isNull());
    }

    /* On a QXmlItem constructed from a null QXmlNodeModelIndex. */
    {
        const QXmlItem item((QXmlNodeModelIndex()));
        QVERIFY(item.isNull());
    }
}

void tst_QXmlItem::isNode() const
{
    /* Check default value. */
    {
        const QXmlItem item;
        QVERIFY(!item.isNode());
    }

    /* On atomic value. */
    {
        const QXmlItem item(QVariant(3));
        QVERIFY(!item.isNode());
    }
    // TODO on valid node index

    /* On a QXmlItem constructed from a null QVariant. */
    {
        const QXmlItem item((QVariant()));
        QVERIFY(!item.isNode());
    }

    /* On a QXmlItem constructed from a null QXmlNodeModelIndex. */
    {
        const QXmlItem item((QXmlNodeModelIndex()));
        QVERIFY(!item.isNode());
    }
}

void tst_QXmlItem::isAtomicValue() const
{
    /* Check default value. */
    {
        const QXmlItem item;
        QVERIFY(!item.isAtomicValue());
    }

    /* On valid atomic value. */
    {
        const QXmlItem item(QVariant(3));
        QVERIFY(item.isAtomicValue());
    }

    // TODO on valid node index

    /* On a QXmlItem constructed from a null QVariant. */
    {
        const QXmlItem item((QVariant()));
        QVERIFY(!item.isAtomicValue());
    }

    /* On a QXmlItem constructed from a null QXmlNodeModelIndex. */
    {
        const QXmlItem item((QXmlNodeModelIndex()));
        QVERIFY(!item.isAtomicValue());
    }
}

void tst_QXmlItem::toAtomicValue() const
{
    /* Check default value. */
    {
        const QXmlItem item;
        QVERIFY(item.toAtomicValue().isNull());
    }

    /* On atomic value. */
    {
        const QXmlItem item(QVariant(3));
        QCOMPARE(item.toAtomicValue(), QVariant(3));
    }

    /* On a QXmlItem constructed from a null QVariant. */
    {
        const QXmlItem item((QVariant()));
        QVERIFY(item.toAtomicValue().isNull());
    }

    /* On a QXmlItem constructed from a null QXmlNodeModelIndex. */
    {
        const QXmlItem item((QXmlNodeModelIndex()));
        QVERIFY(item.toAtomicValue().isNull());
    }
}

void tst_QXmlItem::toNodeModelIndex() const
{
    /* Check default value. */
    {
        const QXmlItem item;
        QVERIFY(item.toNodeModelIndex().isNull());
    }

    /* On valid atomic value. */
    {
        const QXmlItem item(QVariant(3));
        QVERIFY(item.toNodeModelIndex().isNull());
    }

    /* On a QXmlItem constructed from a null QVariant. */
    {
        const QXmlItem item((QVariant()));
        QVERIFY(item.isNull());
    }

    /* On a QXmlItem constructed from a null QXmlNodeModelIndex. */
    {
        const QXmlItem item((QXmlNodeModelIndex()));
        QVERIFY(item.isNull());
    }
}

void tst_QXmlItem::objectSize() const
{
    /* We can't currently test this in portable way,
     * so disable it. */
    return;

    QCOMPARE(sizeof(QPatternist::NodeIndexStorage), sizeof(QXmlItem));

    /* Data, additional data, and pointer to model. We test for two, such that we
     * account for the padding that MSVC do. */
    QVERIFY(sizeof(QXmlItem) == sizeof(qint64) * 2 + sizeof(QAbstractXmlNodeModel *) * 2
            || sizeof(QXmlItem) == sizeof(qint64) * 2 + sizeof(QAbstractXmlNodeModel *) * 2);
}

/*!
  Check that the functions that should be const, are.
 */
void tst_QXmlItem::constCorrectness() const
{
    const QXmlItem item;
    item.isNull();
    item.isNode();
    item.isAtomicValue();

    item.toAtomicValue();
    item.toNodeModelIndex();
}

/*!
  Check that QXmlItem can be used inside QVariant.
 */
void tst_QXmlItem::withinQVariant() const
{
    QXmlItem val;
    const QVariant variant(qVariantFromValue(val));
    QXmlItem val2(qvariant_cast<QXmlItem>(variant));
}

QTEST_MAIN(tst_QXmlItem)

#include "tst_qxmlitem.moc"

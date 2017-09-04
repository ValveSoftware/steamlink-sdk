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
#include <QtQml/private/qqmlobjectmodel_p.h>
#include <QtTest/qsignalspy.h>
#include <QtTest/qtest.h>

class tst_QQmlObjectModel : public QObject
{
    Q_OBJECT

private slots:
    void changes();
};

static bool compareItems(QQmlObjectModel *model, const QObjectList &items)
{
    for (int i = 0; i < items.count(); ++i) {
        if (model->get(i) != items.at(i))
            return false;
    }
    return true;
}

void tst_QQmlObjectModel::changes()
{
    QQmlObjectModel model;

    QSignalSpy countSpy(&model, SIGNAL(countChanged()));
    QSignalSpy childrenSpy(&model, SIGNAL(childrenChanged()));

    int count = 0;
    int countSignals = 0;
    int childrenSignals = 0;

    QObjectList items;
    QObject item0, item1, item2, item3;

    // append(item0) -> [item0]
    model.append(&item0); items.append(&item0);
    QCOMPARE(model.count(), ++count);
    QVERIFY(compareItems(&model, items));
    QCOMPARE(countSpy.count(), ++countSignals);
    QCOMPARE(childrenSpy.count(), ++childrenSignals);

    // insert(0, item1) -> [item1, item0]
    model.insert(0, &item1); items.insert(0, &item1);
    QCOMPARE(model.count(), ++count);
    QVERIFY(compareItems(&model, items));
    QCOMPARE(countSpy.count(), ++countSignals);
    QCOMPARE(childrenSpy.count(), ++childrenSignals);

    // append(item2) -> [item1, item0, item2]
    model.append(&item2); items.append(&item2);
    QCOMPARE(model.count(), ++count);
    QVERIFY(compareItems(&model, items));
    QCOMPARE(countSpy.count(), ++countSignals);
    QCOMPARE(childrenSpy.count(), ++childrenSignals);

    // insert(2, item3) -> [item1, item0, item3, item2]
    model.insert(2, &item3); items.insert(2, &item3);
    QCOMPARE(model.count(), ++count);
    QVERIFY(compareItems(&model, items));
    QCOMPARE(countSpy.count(), ++countSignals);
    QCOMPARE(childrenSpy.count(), ++childrenSignals);

    // move(0, 1) -> [item0, item1, item3, item2]
    model.move(0, 1); items.move(0, 1);
    QCOMPARE(model.count(), count);
    QVERIFY(compareItems(&model, items));
    QCOMPARE(countSpy.count(), countSignals);
    QCOMPARE(childrenSpy.count(), ++childrenSignals);

    // move(3, 2) -> [item0, item1, item2, item3]
    model.move(3, 2); items.move(3, 2);
    QCOMPARE(model.count(), count);
    QVERIFY(compareItems(&model, items));
    QCOMPARE(countSpy.count(), countSignals);
    QCOMPARE(childrenSpy.count(), ++childrenSignals);

    // remove(0) -> [item1, item2, item3]
    model.remove(0); items.removeAt(0);
    QCOMPARE(model.count(), --count);
    QVERIFY(compareItems(&model, items));
    QCOMPARE(countSpy.count(), ++countSignals);
    QCOMPARE(childrenSpy.count(), ++childrenSignals);

    // remove(2) -> [item1, item2]
    model.remove(2); items.removeAt(2);
    QCOMPARE(model.count(), --count);
    QVERIFY(compareItems(&model, items));
    QCOMPARE(countSpy.count(), ++countSignals);
    QCOMPARE(childrenSpy.count(), ++childrenSignals);

    // clear() -> []
    model.clear(); items.clear();
    QCOMPARE(model.count(), 0);
    QVERIFY(compareItems(&model, items));
    QCOMPARE(countSpy.count(), ++countSignals);
    QCOMPARE(childrenSpy.count(), ++childrenSignals);
}

QTEST_MAIN(tst_QQmlObjectModel)

#include "tst_qqmlobjectmodel.moc"

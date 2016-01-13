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
#include <qdeclarativecomponent.h>
#include <qdeclarativeengine.h>
#include <qdeclarativeitem.h>
#include <private/qdeclarativelayoutitem_p.h>
#include <qgraphicsview.h>
#include <qgraphicsscene.h>
#include <qgraphicswidget.h>
#include <qgraphicslinearlayout.h>

class tst_qdeclarativelayoutitem : public QObject
{
    Q_OBJECT
public:
    tst_qdeclarativelayoutitem();

private slots:
    void test_resizing();
};

tst_qdeclarativelayoutitem::tst_qdeclarativelayoutitem()
{
}

void tst_qdeclarativelayoutitem::test_resizing()
{
    //Create Layout (must be done in C++)
    QGraphicsView view;
    QGraphicsScene scene;
    QGraphicsWidget *widget = new QGraphicsWidget();
    QGraphicsLinearLayout *layout = new QGraphicsLinearLayout();
    widget->setLayout(layout);
    scene.addItem(widget);
    view.setScene(&scene);
    //Add the QML snippet into the layout
    QDeclarativeEngine engine;
    QDeclarativeComponent c(&engine, QUrl::fromLocalFile(SRCDIR "/data/layoutItem.qml"));
    QDeclarativeLayoutItem* obj = static_cast<QDeclarativeLayoutItem*>(c.create());
    QVERIFY(obj);
    QCOMPARE(obj->minimumSize(), QSizeF(100,100));
    QCOMPARE(obj->preferredSize(), QSizeF(200,200));
    QCOMPARE(obj->maximumSize(), QSizeF(300,300));

    layout->addItem(obj);
    layout->setContentsMargins(0,0,0,0);
    widget->setContentsMargins(0,0,0,0);
    view.show();

    QVERIFY(obj!= 0);

    widget->setGeometry(QRectF(0,0, 400,400));
    QCOMPARE(obj->width(), 300.0);
    QCOMPARE(obj->height(), 300.0);

    widget->setGeometry(QRectF(0,0, 300,300));
    QCOMPARE(obj->width(), 300.0);
    QCOMPARE(obj->height(), 300.0);

    widget->setGeometry(QRectF(0,0, 200,200));
    QCOMPARE(obj->width(), 200.0);
    QCOMPARE(obj->height(), 200.0);

    widget->setGeometry(QRectF(0,0, 100,100));
    QCOMPARE(obj->width(), 100.0);
    QCOMPARE(obj->height(), 100.0);

    widget->setGeometry(QRectF(0,0, 40,40));
    QCOMPARE(obj->width(), 100.0);
    QCOMPARE(obj->height(), 100.0);

    widget->setGeometry(QRectF(0,0, 412,112));
    QCOMPARE(obj->width(), 300.0);
    QCOMPARE(obj->height(), 112.0);
}


QTEST_MAIN(tst_qdeclarativelayoutitem)

#include "tst_qdeclarativelayoutitem.moc"

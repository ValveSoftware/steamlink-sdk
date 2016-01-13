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
#include <qtest.h>
#include <QtTest/QSignalSpy>
#include <QtDeclarative/qdeclarativeengine.h>
#include <QtDeclarative/qdeclarativecomponent.h>
#include <QtCore/qdir.h>
#include <QtCore/qfile.h>
#include <QtCore/qabstractitemmodel.h>
#include <QDebug>

// From qdeclarastivefolderlistmodel.h
const int FileNameRole = Qt::UserRole+1;
const int FilePathRole = Qt::UserRole+2;
enum SortField { Unsorted, Name, Time, Size, Type };

class tst_qdeclarativefolderlistmodel : public QObject
{
    Q_OBJECT
public:
    tst_qdeclarativefolderlistmodel() : removeStart(0), removeEnd(0) {}

public slots:
    void removed(const QModelIndex &, int start, int end) {
        removeStart = start;
        removeEnd = end;
    }

private slots:
    void basicProperties();
    void refresh();

private:
    void checkNoErrors(const QDeclarativeComponent& component);
    QDeclarativeEngine engine;

    int removeStart;
    int removeEnd;
};

void tst_qdeclarativefolderlistmodel::checkNoErrors(const QDeclarativeComponent& component)
{
    // Wait until the component is ready
    QTRY_VERIFY(component.isReady() || component.isError());

    if (component.isError()) {
        QList<QDeclarativeError> errors = component.errors();
        for (int ii = 0; ii < errors.count(); ++ii) {
            const QDeclarativeError &error = errors.at(ii);
            QByteArray errorStr = QByteArray::number(error.line()) + ":" +
                                  QByteArray::number(error.column()) + ":" +
                                  error.description().toUtf8();
            qWarning() << errorStr;
        }
    }
    QVERIFY(!component.isError());
}

void tst_qdeclarativefolderlistmodel::basicProperties()
{
    QDeclarativeComponent component(&engine, QUrl::fromLocalFile(SRCDIR "/data/basic.qml"));
    checkNoErrors(component);

    QAbstractListModel *flm = qobject_cast<QAbstractListModel*>(component.create());
    QVERIFY(flm != 0);

    flm->setProperty("folder",QUrl::fromLocalFile(SRCDIR "/data"));
    QTRY_COMPARE(flm->property("count").toInt(),2); // wait for refresh
    QCOMPARE(flm->property("folder").toUrl(), QUrl::fromLocalFile(SRCDIR "/data"));
    QCOMPARE(flm->property("parentFolder").toUrl(), QUrl::fromLocalFile(SRCDIR));
    QCOMPARE(flm->property("sortField").toInt(), int(Name));
    QCOMPARE(flm->property("nameFilters").toStringList(), QStringList() << "*.qml");
    QCOMPARE(flm->property("sortReversed").toBool(), false);
    QCOMPARE(flm->property("showDirs").toBool(), true);
    QCOMPARE(flm->property("showDotAndDotDot").toBool(), false);
    QCOMPARE(flm->property("showOnlyReadable").toBool(), false);
    QCOMPARE(flm->data(flm->index(0),FileNameRole).toString(), QLatin1String("basic.qml"));
    QCOMPARE(flm->data(flm->index(1),FileNameRole).toString(), QLatin1String("dummy.qml"));

    flm->setProperty("folder",QUrl::fromLocalFile(""));
    QCOMPARE(flm->property("folder").toUrl(), QUrl::fromLocalFile(""));
}

void tst_qdeclarativefolderlistmodel::refresh()
{
    QDeclarativeComponent component(&engine, QUrl::fromLocalFile(SRCDIR "/data/basic.qml"));
    checkNoErrors(component);

    QAbstractListModel *flm = qobject_cast<QAbstractListModel*>(component.create());
    QVERIFY(flm != 0);

    flm->setProperty("folder",QUrl::fromLocalFile(SRCDIR "/data"));
    QTRY_COMPARE(flm->property("count").toInt(),2); // wait for refresh

    int count = flm->rowCount();

    connect(flm, SIGNAL(rowsRemoved(const QModelIndex&,int,int)),
            this, SLOT(removed(const QModelIndex&,int,int)));

    flm->setProperty("sortReversed", true);

    QCOMPARE(removeStart, 0);
    QCOMPARE(removeEnd, count-1);
}

QTEST_MAIN(tst_qdeclarativefolderlistmodel)

#include "tst_qdeclarativefolderlistmodel.moc"

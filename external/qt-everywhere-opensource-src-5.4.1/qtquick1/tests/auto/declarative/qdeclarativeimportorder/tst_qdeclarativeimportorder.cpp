/****************************************************************************
**
** Copyright (C) 2012 Jolla Mobile <robin.burchell@jollamobile.com>
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
#include <qdeclarativedatatest.h>
#include <QDeclarativeEngine>
#include <QDeclarativeComponent>
#include <QDeclarativeContext>

#include <QDebug>


class tst_qdeclarativeimportorder : public QDeclarativeDataTest
{
    Q_OBJECT
public:
    tst_qdeclarativeimportorder()
    {
    }

private slots:
    void qmlObject();

private:
    QDeclarativeEngine engine;
};

Q_GLOBAL_STATIC(QStringList, importMessages);

static void orderedImportMsgHandler(QtMsgType type, const QMessageLogContext &, const QString &message)
{
    if (type == QtDebugMsg)
        importMessages()->append(message);
    else
        fprintf(stderr, "possibly unexpected message of type %d: %s", type, qPrintable(message));
}

void tst_qdeclarativeimportorder::qmlObject()
{
    QDeclarativeComponent component(&engine, testFileUrl("importOrderJs.qml"));

    QtMessageHandler old = qInstallMessageHandler(orderedImportMsgHandler);
    QObject *object = component.create();
    qInstallMessageHandler(old); // do this before the QVERIFY so output goes out ok if it errors
    QVERIFY(object != 0);
    QCOMPARE(*importMessages(), QStringList() << "a.js" << "b.js" << "c.js"
                                              << "d.js" << "e.js" << "f.js"
                                              << "g.js" << "h.js" << "i.js"
                                              << "j.js" << "k.js" << "l.js"
                                              << "m.js" << "n.js" << "o.js"
                                              << "p.js" << "q.js" << "r.js"
                                              << "s.js" << "t.js" << "u.js"
                                              << "v.js" << "w.js" << "x.js"
                                              << "y.js" << "z.js");
    // now I know my ABCs,
    // next time won't you sing with me?
}

QTEST_MAIN(tst_qdeclarativeimportorder)

#include "tst_qdeclarativeimportorder.moc"

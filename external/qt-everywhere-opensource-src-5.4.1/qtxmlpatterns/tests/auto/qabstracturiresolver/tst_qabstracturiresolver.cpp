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


#include <QFile>
#include <QtTest/QtTest>

/* We expect these headers to be available. */
#include <QtXmlPatterns/QAbstractUriResolver>
#include <QtXmlPatterns/qabstracturiresolver.h>
#include <QAbstractUriResolver>
#include <qabstracturiresolver.h>

#include "TestURIResolver.h"

/*!
 \class tst_QAbstractUriResolver
 \internal
 \since 4.4
 \brief Tests the QAbstractUriResolver class.
 */
class tst_QAbstractUriResolver : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void constructor() const;
    void resolve() const;
    void constCorrectness() const;
    void objectSize() const;
    void hasQ_OBJECTMacro() const;
};

void tst_QAbstractUriResolver::constructor() const
{
    /* Allocate instances. */
    {
        TestURIResolver instance;
    }

    {
        TestURIResolver instance1;
        TestURIResolver instance2;
    }

    {
        TestURIResolver instance1;
        TestURIResolver instance2;
        TestURIResolver instance3;
    }
}

void tst_QAbstractUriResolver::constCorrectness() const
{
    const TestURIResolver instance;

    /* This function is supposed to be const. */
    instance.resolve(QUrl(), QUrl());
}

void tst_QAbstractUriResolver::resolve() const
{
    const TestURIResolver instance;
    QCOMPARE(instance.resolve(QUrl(QLatin1String("foo/relative.file")),
                              QUrl(QLatin1String("http://example.com/NotThisOne"))),
             QUrl(QLatin1String("http://example.com/")));
}

void tst_QAbstractUriResolver::objectSize() const
{
    /* We shouldn't have a different size. */
    QCOMPARE(sizeof(QAbstractUriResolver), sizeof(QObject));
}

void tst_QAbstractUriResolver::hasQ_OBJECTMacro() const
{
    TestURIResolver uriResolver;
    /* If this code fails to compile, the Q_OBJECT macro is missing in
     * the class declaration. */
    QAbstractUriResolver *const secondPointer = qobject_cast<QAbstractUriResolver *>(&uriResolver);
    /* The static_cast is for compiling on broken compilers. */
    QCOMPARE(static_cast<QAbstractUriResolver *>(&uriResolver), secondPointer);
}

QTEST_MAIN(tst_QAbstractUriResolver)

#include "tst_qabstracturiresolver.moc"

// vim: et:ts=4:sw=4:sts=4

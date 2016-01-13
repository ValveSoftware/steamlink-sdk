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
#include <QAxObject>
#include <QByteArray>

class tst_QAxObject : public QObject
{
    Q_OBJECT

private slots:
    void propertyByRefWritable();
    void setPropertyByRef();
    void multiplePropertiesDuplicateName();
};

void tst_QAxObject::propertyByRefWritable()
{
    const QAxObject speak("SAPI.SPVoice");
    const QMetaObject *metaObject = speak.metaObject();

    for (int i = 0; i < metaObject->propertyCount(); ++i) {
        if (qstrcmp(metaObject->property(i).name(), "Voice") == 0) {
            QVERIFY(metaObject->property(i).isWritable());
            break;
        }
    }
}

void tst_QAxObject::setPropertyByRef()
{
    QAxObject speak("SAPI.SPVoice");

    QVERIFY(speak.setProperty("Voice", speak.property("Voice")));
}

void tst_QAxObject::multiplePropertiesDuplicateName()
{
    // Remote desktop client control has two instances for most properties,
    // one for the setter and one for the getter
    QAxObject ax("MsTscAx.MsTscAx.4");
    if (ax.isNull())
        QSKIP("MsTscAx control was not found so test cannot be run", SkipAll);
    int newDesktopHeight = 768;
    QVERIFY(ax.setProperty("DesktopHeight", newDesktopHeight));
    QCOMPARE(ax.property("DesktopHeight").toInt(), newDesktopHeight);
}

QTEST_MAIN(tst_QAxObject)
#include "tst_qaxobject.moc"

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

/****************************************************************************
**
** Copyright (C) 2016 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com, author Stephen Kelly <stephen.kelly@kdab.com>
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

#ifndef MYOBJECT_H
#define MYOBJECT_H

#include <QtCore/QObject>

QT_TRANSLATE_NOOP("scope", "string")
QT_TRANSLATE_NOOP_ALIAS("scope", "string_alias")
QT_TRANSLATE_NOOP_UTF8("scope", "utf8_string")
QT_TRANSLATE_NOOP_UTF8_ALIAS("scope", "utf8_string_alias")
QT_TRANSLATE_NOOP3("scope", "string_with_comment", "comment")
QT_TRANSLATE_NOOP3_ALIAS("scope", "string_with_comment_alias", "comment")
QT_TRANSLATE_NOOP3_UTF8("scope", "utf8_string_with_comment", "comment")
QT_TRANSLATE_NOOP3_UTF8_ALIAS("scope", "utf8_string_with_comment_alias", "comment")
QT_TRID_NOOP("this_a_id")
QT_TRID_NOOP_ALIAS("this_a_id_alias")
QString test = qtTrId("yet_another_id");
QString test_alias = qtTrId_alias("yet_another_id_alias");

class Bogus : QObject {
    Q_OBJECT

    static const char * const s_strings[];
};

const char * const Bogus::s_strings[] = {
    QT_TR_NOOP("this should be in Bogus"),
    QT_TR_NOOP_ALIAS("this should be in Bogus Alias"),
    QT_TR_NOOP_UTF8("this should be utf8 in Bogus")
    QT_TR_NOOP_UTF8_ALIAS("this should be utf8 in Bogus Alias")
};

class MyObject : public QObject
{
    Q_OBJECT
    explicit MyObject(QObject *parent = 0)
    {
        tr("Boo", "nsF::D");
        tr_alias("Boo_alias", "nsB::C");
        trUtf8("utf8_Boo", "nsF::D");
        trUtf8_alias("utf8_Boo_alias", "nsF::D");
        translate("QTranslator", "Simple");
        translate_alias("QTranslator", "Simple with comment alias", "with comment")
    }
};

struct NonQObject
{
    Q_DECLARE_TR_FUNCTIONS_ALIAS(NonQObject)

    NonQObject()
    {
        tr("NonQObject_Boo", "nsF::NonQObject_D");
        tr_alias("NonQObject_Boo_alias", "nsB::NonQObject_C");
        trUtf8("utf_NonQObject_Boo", "nsF::D");
        trUtf8_alias("utf8_NonQObject_Boo_alias", "nsF::D");
        translate("NonQObject_QTranslator", "Simple");
        translate_alias("NonQObject_QTranslator", "Simple with comment alias", "with comment")
    }
};

#endif

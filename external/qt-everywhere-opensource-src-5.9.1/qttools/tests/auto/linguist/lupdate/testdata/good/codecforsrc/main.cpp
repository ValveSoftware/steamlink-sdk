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

#include <QApplication>
#include <QtGui>
#include <QtCore>
#include <QTextCodec>

int main(int argc, char **argv)
{
        QApplication a(argc, argv);


        QWidget w;
        QLabel label1(QObject::tr("abc", "ascii"), &w);
        QLabel label2(QObject::tr("æøå", "utf-8"), &w);
        QLabel label2a(QObject::tr("\303\246\303\270\303\245", "utf-8 oct"), &w);
        QLabel label3(QObject::trUtf8("Für Élise", "trUtf8"), &w);
        QLabel label3a(QObject::trUtf8("F\303\274r \303\211lise", "trUtf8 oct"), &w);

        QBoxLayout *ly = new QVBoxLayout(&w);
        ly->addWidget(&label1);
        ly->addWidget(&label2);
        ly->addWidget(&label2a);
        ly->addWidget(&label3);
        ly->addWidget(&label3a);

        w.show();
        return a.exec();
}

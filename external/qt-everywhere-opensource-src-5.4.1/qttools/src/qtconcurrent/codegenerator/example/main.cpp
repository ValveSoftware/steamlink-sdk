/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the tools applications of the Qt Toolkit.
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
#include <QDebug>
#include "codegenerator.h"
using namespace CodeGenerator;

int main()
{
    // The code generator works on items. Each item has a generate() function:
    Item item("");
    qDebug() << item.generate(); // produces "".

    // There are several Item subclasses. Text items contains a text string which they
    // reproduce when generate is called:
    Text text(" Hi there");
    qDebug() << text.generate(); // produces " Hi there".

    // Items can be concatenated:
    Item sentence = text + Text(" Bye there") ;
    qDebug() << sentence.generate(); // produces "Hi there Bye there".
    // (Internally, this creates a tree of items, and generate is called recursively
    // for items that have children.)

    // Repeater items repeat their content when generate is called:
    Repeater repeater = text;
    repeater.setRepeatCount(3);
    qDebug() << repeater.generate(); // produces "Hi there Hi there Hi there".

    // Counters evaluate to the current repeat index.
    Repeater repeater2 = text + Counter();
    repeater2.setRepeatCount(3);
    qDebug() << repeater2.generate(); // produces "Hi there0 Hi there1 Hi there2".

    // Groups provide sub-groups which are repeated according to the current repeat index.
    // Counters inside Groups evaluate to the local repeat index for the Group.
    Group arguments("Arg" + Counter()  + " arg" + Counter());
    Repeater function("void foo(" + arguments + ");\n");
    function.setRepeatCount(3);
    qDebug() << function.generate();

    // Produces:
    // void foo(Arg1 arg1);
    // void foo(Arg1 arg1, Arg2 arg2);
    // void foo(Arg1 arg1, Arg2 arg2, Arg3 arg3);
}

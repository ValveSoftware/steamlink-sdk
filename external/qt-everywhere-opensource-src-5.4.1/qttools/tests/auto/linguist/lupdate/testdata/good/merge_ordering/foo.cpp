
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

// The first line in this file should always be empty, its part of the test!!
class Foo : public QObject
{
    Q_OBJECT
public:
    Foo();
};

Foo::Foo(MainWindow *parent)
    : QObject(parent)
{
    tr("This is the first entry.");
    tr("A second message."); tr("And a second one on the same line.");
    tr("This string did move from the bottom.");
    tr("This tr is new.");
    tr("This one moved in from another file.");
    tr("Now again one which is just where it was.");

    tr("Just as this one.");
    tr("Another alien.");
    tr("This is from the bottom, too.");
    tr("Third string from the bottom.");
    tr("Fourth one!");
    tr("They are coming!");
    tr("They are everywhere!");
    tr("An earthling again.");
}

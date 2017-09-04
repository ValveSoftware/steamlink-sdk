
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

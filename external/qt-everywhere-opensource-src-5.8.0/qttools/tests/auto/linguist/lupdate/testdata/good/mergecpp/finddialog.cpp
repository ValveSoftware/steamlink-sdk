
// The first line in this file should always be empty, its part of the test!!

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

class FindDialog : public QDialog
{
    Q_OBJECT
public:
    FindDialog(MainWindow *parent);
    void reset();
};

FindDialog::FindDialog(MainWindow *parent)
    : QDialog(parent)
{
    QString trans = tr("Enter the text you want to find.");
    trans = tr("Search reached end of the document");
    trans = tr("Search reached start of the document");
    trans = tr( "Text not found" );
}

void FindDialog::reset()
{
    tr("%n item(s)", "merge from singular to plural form", 4);
    tr("%n item(s)", "merge from a finished singular form to an unfinished plural form", 4);


    //~ meta matter
    //% "Hello"
    qtTrId("xx_hello");

    //% "New world"
    qtTrId("xx_world");


    //= new_id
    tr("this is just some text");


    //: A message without source string
    qtTrId("qtn_virtual");
}

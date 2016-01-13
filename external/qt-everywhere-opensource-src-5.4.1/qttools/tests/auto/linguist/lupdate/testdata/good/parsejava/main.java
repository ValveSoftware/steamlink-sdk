/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtTools of the Qt Toolkit.
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

// IMPORTANT!!!! If you want to add testdata to this file,
// always add it to the end in order to not change the linenumbers of translations!!!

package com.trolltech.examples;

public class I18N extends QDialog {

    private class MainWindow extends QMainWindow {
        private String foo = tr("pack class class");

        //: extra comment for t-tor
        private String bar = tr("pack class class extra");

        public MainWindow(QWidget parent) {
            super(parent);

            listWidget = new QListWidget();
            listWidget.addItem(tr("pack class class method"));

        }
    }

    public I18N(QWidget parent) {
        super(parent, new Qt.WindowFlags(Qt.WindowType.WindowStaysOnTopHint));

        tr("pack class method");

        tr("QT_LAYOUT_DIRECTION",
           "Translate this string to the string 'LTR' in left-to-right" +
           " languages or to 'RTL' in right-to-left languages (such as Hebrew" +
           " and Arabic) to get proper widget layout.");

        tr("%n files", "plural form", n);
        tr("%n cars", null, n);
        tr("Age: %1");
        tr("There are %n house(s)", "Plurals and function call", getCount());

        QTranslator trans;
        trans.translate("QTranslator", "Simple");
        trans.translate("QTranslator", "Simple", null);
        trans.translate("QTranslator", "Simple with comment", "with comment");
        trans.translate("QTranslator", "Plural without comment", null, 1);
        trans.translate("QTranslator", "Plural with comment", "comment 1", n);
        trans.translate("QTranslator", "Plural with comment", "comment 2", getCount());

        /*: with extra comment! */
        QCoreApplication.translate("Plurals, QCoreApplication", "%n house(s)", "Plurals and identifier", n);

        // FIXME: This will fail.
        //QApplication.tr("QT_LAYOUT_DIRECTION", "scoped to qapp");

    }

}

/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Graphical Effects module.
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

#include <QtCore/QCoreApplication>
#include "tst_imagecompare.h"

int main(int argc, char *argv[]){

    QCoreApplication a(argc, argv);

    tst_imagecompare test;

    // Tolerance for pixel ARGB component comparison can be set using command line argument -t with tolerance integer value
    // E.g. [program name] -t [tolerance]
    int tolerance = 0;
    for(int i = 0; i < argc; i++){
        if (strcmp(argv[i], "-t") == 0){
            tolerance = atoi(argv[i+1]);

            // Tolerance arguments are removed from the argument list so the list can be sent to QTestLib
            for(i; i < argc; i++){
                argv[i] = argv[i+2];
            }
            argc -= 2;
            break;
        }
    }
    test.setDiffTolerance(tolerance);

    // To be enabled/uncommented if test result output is needed in XML format
    //QStringList arguments;
    //arguments << " " << "-o" << "results.xml" << "-xml";
    //QTest::qExec(&test, arguments);

    QTest::qExec(&test, argc, argv);

    return 0;
}

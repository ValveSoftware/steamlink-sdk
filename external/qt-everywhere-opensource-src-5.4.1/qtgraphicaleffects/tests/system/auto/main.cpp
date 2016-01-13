/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Graphical Effects module.
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

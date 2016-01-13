/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

// This JavaScript file is a single, small, imported script.
// It imports many other (non-nested) single, small, scripts,
// and also imports many other nested, small scripts.

.import "msdsi4.js" as Msdsi4
.import "msdsi9.js" as Msdsi9
.import "msbsi1.js" as Msbsi1
.import "msbsi2.js" as Msbsi2
.import "msbsi3.js" as Msbsi3
.import "msbsi4.js" as Msbsi4
.import "msbsi5.js" as Msbsi5
.import "msbsi6.js" as Msbsi6
.import "msbsi7.js" as Msbsi7
.import "msbsi8.js" as Msbsi8
.import "msbsi9.js" as Msbsi9
.import "msbsi10.js" as Msbsi10
.import "msbsi11.js" as Msbsi11
.import "msbsi12.js" as Msbsi12
.import "msbsi13.js" as Msbsi13
.import "msbsi14.js" as Msbsi14
.import "msbsi15.js" as Msbsi15

function testFunc(seedValue) {
    var retn = 10 * (seedValue * 0.85);
    retn += Msbsi1.testFunc(seedValue);
    retn += Msbsi2.testFunc(seedValue);
    retn += Msbsi3.testFunc(retn);
    retn += Msbsi4.testFunc(seedValue);
    retn += Msbsi5.testFunc(seedValue);
    retn += Msbsi6.testFunc(seedValue);
    retn *= Msdsi9.testFunc(retn);
    retn += Msbsi7.testFunc(seedValue);
    retn += Msbsi8.testFunc(retn);
    retn += Msbsi9.testFunc(seedValue);
    retn += Msbsi10.testFunc(seedValue);
    retn += Msbsi11.testFunc(seedValue);
    retn *= Msdsi4.testFunc(retn);
    retn += Msbsi12.testFunc(seedValue);
    retn += Msbsi13.testFunc(retn);
    retn += Msbsi14.testFunc(seedValue);
    retn += Msbsi15.testFunc(seedValue);
    return retn;
}

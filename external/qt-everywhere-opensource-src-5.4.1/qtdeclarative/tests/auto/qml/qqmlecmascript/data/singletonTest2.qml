/****************************************************************************
**
** Copyright (C) 2013 Canonical Limited and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.0
import Test 1.0

Item {
    property bool myInheritedQmlObjectTest1: false
    property bool myInheritedQmlObjectTest2: false
    property bool myInheritedQmlObjectTest3: false
    property bool myQmlObjectTest1: false
    property bool myQmlObjectTest2: false
    property bool myQmlObjectTest3: false
    property bool qobjectTest1: false
    property bool qobjectTest2: false
    property bool qobjectTest3: false
    property bool singletonEqualToItself: true

    Component.onCompleted: {
        MyInheritedQmlObjectSingleton.myInheritedQmlObjectProperty = MyInheritedQmlObjectSingleton;
        myInheritedQmlObjectTest1 = MyInheritedQmlObjectSingleton.myInheritedQmlObjectProperty == MyInheritedQmlObjectSingleton;
        myInheritedQmlObjectTest2 = MyInheritedQmlObjectSingleton.myInheritedQmlObjectProperty == MyInheritedQmlObjectSingleton.myInheritedQmlObjectProperty;
        myInheritedQmlObjectTest3 = MyInheritedQmlObjectSingleton == MyInheritedQmlObjectSingleton.myInheritedQmlObjectProperty;

        MyInheritedQmlObjectSingleton.myQmlObjectProperty = MyInheritedQmlObjectSingleton;
        myQmlObjectTest1 = MyInheritedQmlObjectSingleton.myQmlObjectProperty == MyInheritedQmlObjectSingleton;
        myQmlObjectTest2 = MyInheritedQmlObjectSingleton.myQmlObjectProperty == MyInheritedQmlObjectSingleton.myQmlObjectProperty;
        myQmlObjectTest3 = MyInheritedQmlObjectSingleton == MyInheritedQmlObjectSingleton.myQmlObjectProperty;

        MyInheritedQmlObjectSingleton.qobjectProperty = MyInheritedQmlObjectSingleton;
        qobjectTest1 = MyInheritedQmlObjectSingleton.qobjectProperty == MyInheritedQmlObjectSingleton;
        qobjectTest2 = MyInheritedQmlObjectSingleton.qobjectProperty == MyInheritedQmlObjectSingleton.qobjectProperty;
        qobjectTest3 = MyInheritedQmlObjectSingleton == MyInheritedQmlObjectSingleton.qobjectProperty;

        singletonEqualToItself = MyInheritedQmlObjectSingleton == MyInheritedQmlObjectSingleton;
    }
}

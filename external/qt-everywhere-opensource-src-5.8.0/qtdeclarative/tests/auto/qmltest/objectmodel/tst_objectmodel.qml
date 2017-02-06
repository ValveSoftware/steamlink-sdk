/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
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
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
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

import QtQml 2.1
import QtQml.Models 2.3
import QtTest 1.1

TestCase {
    name: "ObjectModel"

    ObjectModel {
        id: model
        QtObject { id: static0 }
        QtObject { id: static1 }
        QtObject { id: static2 }
    }

    Component { id: object; QtObject { } }

    function test_attached_index() {
        compare(model.count, 3)
        compare(static0.ObjectModel.index, 0)
        compare(static1.ObjectModel.index, 1)
        compare(static2.ObjectModel.index, 2)

        var dynamic0 = object.createObject(model, {objectName: "dynamic0"})
        compare(dynamic0.ObjectModel.index, -1)
        model.append(dynamic0) // -> [static0, static1, static2, dynamic0]
        compare(model.count, 4)
        for (var i = 0; i < model.count; ++i)
            compare(model.get(i).ObjectModel.index, i)

        var dynamic1 = object.createObject(model, {objectName: "dynamic1"})
        compare(dynamic1.ObjectModel.index, -1)
        model.insert(0, dynamic1) // -> [dynamic1, static0, static1, static2, dynamic0]
        compare(model.count, 5)
        for (i = 0; i < model.count; ++i)
            compare(model.get(i).ObjectModel.index, i)

        model.move(1, 3) // -> [dynamic1, static1, static2, static0, dynamic0]
        compare(model.count, 5)
        for (i = 0; i < model.count; ++i)
            compare(model.get(i).ObjectModel.index, i)

        model.move(4, 0) // -> [dynamic0, dynamic1, static1, static2, static0]
        compare(model.count, 5)
        for (i = 0; i < model.count; ++i)
            compare(model.get(i).ObjectModel.index, i)

        model.remove(1) // -> [dynamic0, static1, static2, static0]
        compare(model.count, 4)
        compare(dynamic1.ObjectModel.index, -1)
        for (i = 0; i < model.count; ++i)
            compare(model.get(i).ObjectModel.index, i)

        model.clear()
        compare(static0.ObjectModel.index, -1)
        compare(static1.ObjectModel.index, -1)
        compare(static2.ObjectModel.index, -1)
        compare(dynamic0.ObjectModel.index, -1)
        compare(dynamic1.ObjectModel.index, -1)
    }
}

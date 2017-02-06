/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the documentation of the Qt Toolkit.
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

#include "avatarExample.h"
#include <QQmlEngine>
#include <QQmlComponent>

void registerTypes()
{
//![0]
    qmlRegisterType<AvatarExample>("Qt.example", 1, 0, "AvatarExample");
//![0]
}

void expectOne()
{
//![1]
QQmlComponent component(&engine, "exampleOne.qml");
QObject *object = component.create();
// The scarce resource will have been released automatically
// by this point, after the binding expression was evaluated.
delete object;
//![1]
}

void expectTwo()
{
//![2]
QQmlComponent component(&engine, "exampleTwo.qml");
QObject *object = component.create();
// The scarce resource will not have been released automatically
// after the binding expression was evaluated.
// Since the scarce resource was not released explicitly prior
// to the binding expression being evaluated, we get:
bool expectedResult = (object->property("avatar").isValid() == true);
delete object;
//![2]
}

void expectThree()
{
//![3]
QQmlComponent component(&engine, "exampleThree.qml");
QObject *object = component.create();
// The resource was preserved explicitly during evaluation of the
// JavaScript expression.  Thus, during property assignment, the
// scarce resource was still valid, and so we get:
bool expectedResult = (object->property("avatar").isValid() == true);
// The scarce resource will not be released until all references to
// the resource are released, and the JavaScript garbage collector runs.
delete object;
//![3]
}

void expectFour()
{
//![4]
QQmlComponent component(&engine, "exampleFour.qml");
QObject *object = component.create();
// The scarce resource was explicitly preserved by the client during
// the importAvatar() function, and so the scarce resource
// remains valid until the explicit call to releaseAvatar().  As such,
// we get the expected results:
bool expectedResultOne = (object->property("avatarOne").isValid() == true);
bool expectedResultTwo = (object->property("avatarTwo").isValid() == false);
// Because the scarce resource referenced by avatarTwo was released explicitly,
// it will no longer be consuming any system resources (beyond what a normal
// JS Object would; that small overhead will exist until the JS GC runs, as per
// any other JavaScript object).
delete object;
//![4]
}

void expectFive()
{
//![5]
QQmlComponent component(&engine, "exampleFive.qml");
QObject *object = component.create();
// We have the expected results:
bool expectedResultOne = (object->property("avatarOne").isValid() == false);
bool expectedResultTwo = (object->property("avatarTwo").isValid() == false);
// Because although only avatarTwo was explicitly released,
// avatarOne and avatarTwo were referencing the same
// scarce resource.
delete object;
//![5]
}

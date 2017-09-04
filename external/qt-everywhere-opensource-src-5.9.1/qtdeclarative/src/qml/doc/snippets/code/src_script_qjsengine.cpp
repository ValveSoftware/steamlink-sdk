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

//! [0]
QJSEngine myEngine;
QJSValue three = myEngine.evaluate("1 + 2");
//! [0]


//! [1]
QJSValue fun = myEngine.evaluate("(function(a, b) { return a + b; })");
QJSValueList args;
args << 1 << 2;
QJSValue threeAgain = fun.call(QJSValue(), args);
//! [1]


//! [2]
QString fileName = "helloworld.qs";
QFile scriptFile(fileName);
if (!scriptFile.open(QIODevice::ReadOnly))
    // handle error
QTextStream stream(&scriptFile);
QString contents = stream.readAll();
scriptFile.close();
myEngine.evaluate(contents, fileName);
//! [2]


//! [3]
myEngine.globalObject().setProperty("myNumber", 123);
...
QJSValue myNumberPlusOne = myEngine.evaluate("myNumber + 1");
//! [3]


//! [4]
QJSValue result = myEngine.evaluate(...);
if (result.isError())
    qDebug()
            << "Uncaught exception at line"
            << result.property("lineNumber").toInt()
            << ":" << result.toString();
//! [4]


//! [5]
QPushButton *button = new QPushButton;
QJSValue scriptButton = myEngine.newQObject(button);
myEngine.globalObject().setProperty("button", scriptButton);

myEngine.evaluate("button.checkable = true");

qDebug() << scriptButton.property("checkable").toBool();
scriptButton.property("show").call(); // call the show() slot
//! [5]

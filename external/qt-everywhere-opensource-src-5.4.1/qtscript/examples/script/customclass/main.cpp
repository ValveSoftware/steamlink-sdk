/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
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

#include <QCoreApplication>
#include <QtDebug>
#include <QtScript>
#include "bytearrayclass.h"

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    QScriptEngine eng;

    ByteArrayClass *baClass = new ByteArrayClass(&eng);
    eng.globalObject().setProperty("ByteArray", baClass->constructor());

    qDebug() << "ba = new ByteArray(4):" << eng.evaluate("ba = new ByteArray(4)").toString();
    qDebug() << "ba instanceof ByteArray:" << eng.evaluate("ba instanceof ByteArray").toBool();
    qDebug() << "ba.length:" << eng.evaluate("ba.length").toNumber();
    qDebug() << "ba[1] = 123; ba[1]:" << eng.evaluate("ba[1] = 123; ba[1]").toNumber();
    qDebug() << "ba[7] = 224; ba.length:" << eng.evaluate("ba[7] = 224; ba.length").toNumber();
    qDebug() << "for-in loop:" << eng.evaluate("result = '';\n"
                                               "for (var p in ba) {\n"
                                               "  if (result.length > 0)\n"
                                               "    result += ', ';\n"
                                               "  result += '(' + p + ',' + ba[p] + ')';\n"
                                               "} result").toString();
    qDebug() << "ba.toBase64():" << eng.evaluate("b64 = ba.toBase64()").toString();
    qDebug() << "ba.toBase64().toLatin1String():" << eng.evaluate("b64.toLatin1String()").toString();
    qDebug() << "ba.valueOf():" << eng.evaluate("ba.valueOf()").toString();
    qDebug() << "ba.chop(2); ba.length:" << eng.evaluate("ba.chop(2); ba.length").toNumber();
    qDebug() << "ba2 = new ByteArray(ba):" << eng.evaluate("ba2 = new ByteArray(ba)").toString();
    qDebug() << "ba2.equals(ba):" << eng.evaluate("ba2.equals(ba)").toBool();
    qDebug() << "ba2.equals(new ByteArray()):" << eng.evaluate("ba2.equals(new ByteArray())").toBool();

    return 0;
}

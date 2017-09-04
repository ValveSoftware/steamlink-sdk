/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
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

#include <QtCore>
#include <QtScript>

template <class Container>
QScriptValue toScriptValue(QScriptEngine *eng, const Container &cont)
{
    QScriptValue a = eng->newArray();
    typename Container::const_iterator begin = cont.begin();
    typename Container::const_iterator end = cont.end();
    typename Container::const_iterator it;
    for (it = begin; it != end; ++it)
        a.setProperty(quint32(it - begin), eng->toScriptValue(*it));
    return a;
}

template <class Container>
void fromScriptValue(const QScriptValue &value, Container &cont)
{
    quint32 len = value.property("length").toUInt32();
    for (quint32 i = 0; i < len; ++i) {
        QScriptValue item = value.property(i);
        typedef typename Container::value_type ContainerValue;
        cont.push_back(qscriptvalue_cast<ContainerValue>(item));
    }
}

typedef QVector<int> IntVector;
typedef QVector<QString> StringVector;

Q_DECLARE_METATYPE(IntVector)
Q_DECLARE_METATYPE(StringVector)

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QScriptEngine eng;
    // register our custom types
    qScriptRegisterMetaType<IntVector>(&eng, toScriptValue, fromScriptValue);
    qScriptRegisterMetaType<StringVector>(&eng, toScriptValue, fromScriptValue);

    QScriptValue val = eng.evaluate("[1, 4, 7, 11, 50, 3, 19, 60]");

    fprintf(stdout, "Script array: %s\n", qPrintable(val.toString()));

    IntVector iv = qscriptvalue_cast<IntVector>(val);

    fprintf(stdout, "qscriptvalue_cast to QVector<int>: ");
    for (int i = 0; i < iv.size(); ++i)
        fprintf(stdout, "%s%d", (i > 0) ? "," : "", iv.at(i));
    fprintf(stdout, "\n");

    val = eng.evaluate("[9, 'foo', 46.5, 'bar', 'Qt', 555, 'hello']");

    fprintf(stdout, "Script array: %s\n", qPrintable(val.toString()));

    StringVector sv = qscriptvalue_cast<StringVector>(val);

    fprintf(stdout, "qscriptvalue_cast to QVector<QString>: ");
    for (int i = 0; i < sv.size(); ++i)
        fprintf(stdout, "%s%s", (i > 0) ? "," : "", qPrintable(sv.at(i)));
    fprintf(stdout, "\n");

    return 0;
}

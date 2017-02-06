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


#include <QtCore/qfile.h>
#include <QtCore/qtextstream.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qscopedpointer.h>

#include <QtCore/QCoreApplication>

#include <QtQml/qjsengine.h>

#include <stdlib.h>


class CommandInterface : public QObject
{
    Q_OBJECT
public:
    Q_INVOKABLE void quit() { m_wantsToQuit = true; }
    static bool wantsToQuit() { return m_wantsToQuit; }
private:
    static bool m_wantsToQuit;
};

bool CommandInterface::m_wantsToQuit = false;


static void interactive(QJSEngine *eng)
{
    QTextStream qin(stdin, QFile::ReadOnly);
    const char *prompt = "qs> ";

    forever {
        QString line;

        printf("%s", prompt);
        fflush(stdout);

        line = qin.readLine();
        if (line.isNull())
            break;

        if (line.trimmed().isEmpty())
            continue;

        line += QLatin1Char('\n');

        QJSValue result = eng->evaluate(line, QLatin1String("typein"));

        fprintf(stderr, "%s\n", qPrintable(result.toString()));

        if (CommandInterface::wantsToQuit())
            break;
    }
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QScopedPointer<QJSEngine> eng(new QJSEngine());
    {
        QJSValue globalObject = eng->globalObject();
        QJSValue interface = eng->newQObject(new CommandInterface);
        globalObject.setProperty("qt", interface);
    }

    if (! *++argv) {
        interactive(eng.data());
        return EXIT_SUCCESS;
    }

    while (const char *arg = *argv++) {
        QString fileName = QString::fromLocal8Bit(arg);

        if (fileName == QLatin1String("-i")) {
            interactive(eng.data());
            break;
        }

        QString contents;
        int lineNumber = 1;

        if (fileName == QLatin1String("-")) {
            QTextStream stream(stdin, QFile::ReadOnly);
            contents = stream.readAll();
        } else {
            QFile file(fileName);
            if (file.open(QFile::ReadOnly)) {
                QTextStream stream(&file);
                contents = stream.readAll();
                file.close();

                // strip off #!/usr/bin/env qjs line
                if (contents.startsWith("#!")) {
                    contents.remove(0, contents.indexOf("\n"));
                    ++lineNumber;
                }
            }
        }

        if (contents.isEmpty())
            continue;

        QJSValue result = eng->evaluate(contents, fileName, lineNumber);
        if (result.isError()) {
            fprintf (stderr, "    %s\n\n", qPrintable(result.toString()));
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}

#include <main.moc>

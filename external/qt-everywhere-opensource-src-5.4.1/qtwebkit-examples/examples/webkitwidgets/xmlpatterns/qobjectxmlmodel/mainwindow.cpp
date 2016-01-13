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

#include <QtWidgets>
#include <QtXmlPatterns>

#include "mainwindow.h"
#include "qobjectxmlmodel.h"
#include "xmlsyntaxhighlighter.h"

MainWindow::MainWindow()
{
    setupUi(this);

    new XmlSyntaxHighlighter(wholeTreeOutput->document());

    /* Setup the font. */
    {
        QFont font("Courier");
        font.setFixedPitch(true);

        wholeTree->setFont(font);
        wholeTreeOutput->setFont(font);
        htmlQueryEdit->setFont(font);
    }

    QXmlNamePool namePool;
    QObjectXmlModel qObjectModel(this, namePool);
    QXmlQuery query(namePool);

    /* The QObject tree as XML view. */
    {
        query.bindVariable("root", qObjectModel.root());
        query.setQuery(QUrl("qrc:/queries/wholeTree.xq"));

        Q_ASSERT(query.isValid());
        QByteArray output;
        QBuffer buffer(&output);
        buffer.open(QIODevice::WriteOnly);

        /* Let's the use the formatter, so it's a bit easier to read. */
        QXmlFormatter serializer(query, &buffer);

        query.evaluateTo(&serializer);
        buffer.close();

        {
            QFile queryFile(":/queries/wholeTree.xq");
            queryFile.open(QIODevice::ReadOnly);
            wholeTree->setPlainText(QString::fromUtf8(queryFile.readAll()));
            wholeTreeOutput->setPlainText(QString::fromUtf8(output.constData()));
        }
    }

    /* The QObject occurrence statistics as HTML view. */
    {
        query.setQuery(QUrl("qrc:/queries/statisticsInHTML.xq"));
        Q_ASSERT(query.isValid());

        QByteArray output;
        QBuffer buffer(&output);
        buffer.open(QIODevice::WriteOnly);

        /* Let's the use the serializer, so we gain a bit of speed. */
        QXmlSerializer serializer(query, &buffer);

        query.evaluateTo(&serializer);
        buffer.close();

        {
            QFile queryFile(":/queries/statisticsInHTML.xq");
            queryFile.open(QIODevice::ReadOnly);
            htmlQueryEdit->setPlainText(QString::fromUtf8(queryFile.readAll()));
            htmlOutput->setHtml(QString(output));
        }
    }
}

void MainWindow::on_actionAbout_triggered()
{
    QMessageBox::about(this,
                       tr("About QObject XML Model"),
                       tr("<p>The <b>QObject XML Model</b> example shows "
                          "how to use XQuery on top of data of your choice "
                          "without converting it to an XML document.</p>"
                          "<p>In this example a QSimpleXmlNodeModel subclass "
                          "makes it possible to query a QObject tree using "
                          "XQuery and retrieve the result as pointers to "
                          "QObjects, or as XML.</p>"
                          "<p>A possible use case of this could be to write "
                          "an application that tests a graphical interface "
                          "against Human Interface Guidelines, or that "
                          "queries an application's data which is modeled "
                          "using a QObject tree and dynamic properties."));
}



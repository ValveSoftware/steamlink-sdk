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

#include <QMessageBox>
#include <QFileDialog>
#include <QtXmlPatterns>

#include "querymainwindow.h"
#include "xmlsyntaxhighlighter.h"

//! [0]
QueryMainWindow::QueryMainWindow() : ui_defaultQueries(0)
{
    setupUi(this);

    new XmlSyntaxHighlighter(findChild<QTextEdit*>("inputTextEdit")->document());
    new XmlSyntaxHighlighter(findChild<QTextEdit*>("outputTextEdit")->document());

    ui_defaultQueries = findChild<QComboBox*>("defaultQueries");
    QMetaObject::connectSlotsByName(this);
    connect(ui_defaultQueries, SIGNAL(currentIndexChanged(int)), SLOT(displayQuery(int)));

    loadInputFile();
    const QStringList queries(QDir(":/files/", "*.xq").entryList());
    int len = queries.count();
    for(int i = 0; i < len; ++i)
        ui_defaultQueries->addItem(queries.at(i));
    if (len > 0)
        displayQuery(0);
}
//! [0]


//! [1]
void QueryMainWindow::displayQuery(int index)
{
    QFile queryFile(QString(":files/") + ui_defaultQueries->itemText(index));
    queryFile.open(QIODevice::ReadOnly);
    const QString query(QString::fromLatin1(queryFile.readAll()));
    findChild<QTextEdit*>("queryTextEdit")->setPlainText(query);

    evaluate(query);
}
//! [1]


void QueryMainWindow::loadInputFile()
{
    QFile forView;
    forView.setFileName(":/files/cookbook.xml");
    if (!forView.open(QIODevice::ReadOnly)) {
        QMessageBox::information(this,
            tr("Unable to open file"), forView.errorString());
        return;
    }

    QTextStream in(&forView);
    QString inputDocument = in.readAll();
    findChild<QTextEdit*>("inputTextEdit")->setPlainText(inputDocument);
}


//! [2]
void QueryMainWindow::evaluate(const QString &str)
{
    QFile sourceDocument;
    sourceDocument.setFileName(":/files/cookbook.xml");
    sourceDocument.open(QIODevice::ReadOnly);

    QByteArray outArray;
    QBuffer buffer(&outArray);
    buffer.open(QIODevice::ReadWrite);

    QXmlQuery query;
    query.bindVariable("inputDocument", &sourceDocument);
    query.setQuery(str);
    if (!query.isValid())
        return;

    QXmlFormatter formatter(query, &buffer);
    if (!query.evaluateTo(&formatter))
        return;

    buffer.close();
    findChild<QTextEdit*>("outputTextEdit")->setPlainText(QString::fromUtf8(outArray.constData()));

}
//! [2]


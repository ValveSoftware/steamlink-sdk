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

#include <QFileDialog>
#include <QMessageBox>
#include <QLibraryInfo>
#include <QtXmlPatterns>

#include "mainwindow.h"
#include "xmlsyntaxhighlighter.h"

//! [0]
MainWindow::MainWindow() : m_fileTree(m_namePool)
{
    setupUi(this);

    new XmlSyntaxHighlighter(fileTree->document());

    // Set up the font.
    {
        QFont font("Courier",10);
        font.setFixedPitch(true);

        fileTree->setFont(font);
        queryEdit->setFont(font);
        output->setFont(font);
    }

    const QString dir(QLibraryInfo::location(QLibraryInfo::ExamplesPath) + "/xmlpatterns/filetree");
    qDebug() << dir;

    if (QDir(dir).exists())
        loadDirectory(dir);
    else
        fileTree->setPlainText(tr("Use the Open menu entry to select a directory."));

    const QStringList queries(QDir(":/queries/", "*.xq").entryList());
    int len = queries.count();

    for (int i = 0; i < len; ++i)
        queryBox->addItem(queries.at(i));

}
//! [0]

//! [2]
void MainWindow::on_queryBox_currentIndexChanged(const QString &currentText)
{
    QFile queryFile(":/queries/" + currentText);
    queryFile.open(QIODevice::ReadOnly);

    queryEdit->setPlainText(QString::fromLatin1(queryFile.readAll()));
    evaluateResult();
}
//! [2]

//! [3]
void MainWindow::evaluateResult()
{
    if (queryBox->currentText().isEmpty() || m_fileNode.isNull())
        return;

    QXmlQuery query(m_namePool);
    query.bindVariable("fileTree", m_fileNode);
    query.setQuery(QUrl("qrc:/queries/" + queryBox->currentText()));

    QByteArray formatterOutput;
    QBuffer buffer(&formatterOutput);
    buffer.open(QIODevice::WriteOnly);

    QXmlFormatter formatter(query, &buffer);
    query.evaluateTo(&formatter);

    output->setText(QString::fromLatin1(formatterOutput.constData()));
}
//! [3]

//! [1]
void MainWindow::on_actionOpenDirectory_triggered()
{
    const QString directoryName = QFileDialog::getExistingDirectory(this);
    if (!directoryName.isEmpty())
        loadDirectory(directoryName);
}
//! [1]

//! [4]
//! [5]
void MainWindow::loadDirectory(const QString &directory)
{
    Q_ASSERT(QDir(directory).exists());

    m_fileNode = m_fileTree.nodeFor(directory);
//! [5]

    QXmlQuery query(m_namePool);
    query.bindVariable("fileTree", m_fileNode);
    query.setQuery(QUrl("qrc:/queries/wholeTree.xq"));

    QByteArray output;
    QBuffer buffer(&output);
    buffer.open(QIODevice::WriteOnly);

    QXmlFormatter formatter(query, &buffer);
    query.evaluateTo(&formatter);

    treeInfo->setText(tr("Model of %1 output as XML.").arg(directory));
    fileTree->setText(QString::fromLatin1(output.constData()));
    evaluateResult();
//! [6]
}
//! [6]
//! [4]

void MainWindow::on_actionAbout_triggered()
{
    QMessageBox::about(this, tr("About File Tree"),
                   tr("<p>Select <b>File->Open Directory</b> and "
                      "choose a directory. The directory is then "
                      "loaded into the model, and the model is "
                      "displayed on the left as XML.</p>"

                      "<p>From the query menu on the right, select "
                      "a query. The query is displayed and then run "
                      "on the model. The results are displayed below "
                      "the query.</p>"));
}



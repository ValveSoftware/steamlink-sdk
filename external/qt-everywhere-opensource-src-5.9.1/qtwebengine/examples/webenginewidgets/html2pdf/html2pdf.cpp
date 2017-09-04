/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
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

#include <QApplication>
#include <QCommandLineParser>
#include <QFile>
#include <QTextStream>
#include <QWebEnginePage>

#include <functional>

using namespace std;
using namespace std::placeholders;

class Html2PdfConverter : public QObject
{
    Q_OBJECT
public:
    explicit Html2PdfConverter(QString inputPath, QString outputPath);
    int run();

private slots:
    void loadFinished(bool ok);
    void pdfPrintingFinished(const QString &filePath, bool success);

private:
    QString m_inputPath;
    QString m_outputPath;
    QScopedPointer<QWebEnginePage> m_page;
};

Html2PdfConverter::Html2PdfConverter(QString inputPath, QString outputPath)
    : m_inputPath(move(inputPath))
    , m_outputPath(move(outputPath))
    , m_page(new QWebEnginePage)
{
    connect(m_page.data(), &QWebEnginePage::loadFinished,
            this, &Html2PdfConverter::loadFinished);
    connect(m_page.data(), &QWebEnginePage::pdfPrintingFinished,
            this, &Html2PdfConverter::pdfPrintingFinished);
}

int Html2PdfConverter::run()
{
    m_page->load(QUrl::fromUserInput(m_inputPath));
    return QApplication::exec();
}

void Html2PdfConverter::loadFinished(bool ok)
{
    if (!ok) {
        QTextStream(stderr)
            << tr("failed to load URL '%1'").arg(m_inputPath) << "\n";
        QCoreApplication::exit(1);
        return;
    }

    m_page->printToPdf(m_outputPath);
}

void Html2PdfConverter::pdfPrintingFinished(const QString &filePath, bool success)
{
    if (!success) {
        QTextStream(stderr)
            << tr("failed to print to output file '%1'").arg(filePath) << "\n";
        QCoreApplication::exit(1);
    } else {
        QCoreApplication::quit();
    }
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName("html2pdf");
    QCoreApplication::setApplicationVersion(QT_VERSION_STR);

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QCoreApplication::translate("main", "Converts the web page INPUT into the PDF file OUTPUT."));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(
        QCoreApplication::translate("main", "INPUT"),
        QCoreApplication::translate("main", "Input URL for PDF conversion."));
    parser.addPositionalArgument(
        QCoreApplication::translate("main", "OUTPUT"),
        QCoreApplication::translate("main", "Output file name for PDF conversion."));

    parser.process(QCoreApplication::arguments());

    const QStringList requiredArguments = parser.positionalArguments();
    if (requiredArguments.size() != 2)
        parser.showHelp(1);

    Html2PdfConverter converter(requiredArguments.at(0), requiredArguments.at(1));
    return converter.run();
}

#include "html2pdf.moc"

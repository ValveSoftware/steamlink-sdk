/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the documentation of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
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
LoginWidget::LoginWidget()
{
    QLabel *label = new QLabel(tr("Password:"));
    ...
}
//! [0]


//! [1]
void some_global_function(LoginWidget *logwid)
{
    QLabel *label = new QLabel(
                LoginWidget::tr("Password:"), logwid);
}

void same_global_function(LoginWidget *logwid)
{
    QLabel *label = new QLabel(
                QCoreApplication::translate("LoginWidget", "Password:"), logwid);
}
//! [1]


//! [2]
QString FriendlyConversation::greeting(int type)
{
    static const char *greeting_strings[] = {
        QT_TR_NOOP("Hello"),
        QT_TR_NOOP("Goodbye")
    };
    return tr(greeting_strings[type]);
}
//! [2]


//! [3]
static const char *greeting_strings[] = {
    QT_TRANSLATE_NOOP("FriendlyConversation", "Hello"),
    QT_TRANSLATE_NOOP("FriendlyConversation", "Goodbye")
};

QString FriendlyConversation::greeting(int type)
{
    return tr(greeting_strings[type]);
}

QString global_greeting(int type)
{
    return QCoreApplication::translate("FriendlyConversation",
                                       greeting_strings[type]);
}
//! [3]


//! [4]
void FileCopier::showProgress(int done, int total,
                              const QString &currentFile)
{
    label.setText(tr("%1 of %2 files copied.\nCopying: %3")
                  .arg(done)
                  .arg(total)
                  .arg(currentFile));
}
//! [4]


//! [5]
QString s1 = "%1 of %2 files copied. Copying: %3";
QString s2 = "Kopierer nu %3. Av totalt %2 filer er %1 kopiert.";

qDebug() << s1.arg(5).arg(10).arg("somefile.txt");
qDebug() << s2.arg(5).arg(10).arg("somefile.txt");
//! [5]


//! [8]
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QTranslator qtTranslator;
    qtTranslator.load("qt_" + QLocale::system().name(),
            QLibraryInfo::location(QLibraryInfo::TranslationsPath));
    app.installTranslator(&qtTranslator);

    QTranslator myappTranslator;
    myappTranslator.load("myapp_" + QLocale::system().name());
    app.installTranslator(&myappTranslator);

    ...
    return app.exec();
}
//! [8]


//! [9]
QString string = ...; // some Unicode text

QTextCodec *codec = QTextCodec::codecForName("ISO 8859-5");
QByteArray encodedString = codec->fromUnicode(string);
//! [9]


//! [10]
QByteArray encodedString = ...; // some ISO 8859-5 encoded text

QTextCodec *codec = QTextCodec::codecForName("ISO 8859-5");
QString string = codec->toUnicode(encodedString);
//! [10]


//! [11]
void Clock::setTime(const QTime &time)
{
    if (tr("AMPM") == "AMPM") {
        // 12-hour clock
    } else {
        // 24-hour clock
    }
}
//! [11]


//! [12]
void MyWidget::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange) {
        titleLabel->setText(tr("Document Title"));
        ...
        okPushButton->setText(tr("&OK"));
    } else
        QWidget::changeEvent(event);
}
//! [12]


//! [13]
void some_global_function(LoginWidget *logwid)
{
    QLabel *label = new QLabel(
            LoginWidget::tr("Password:"), logwid);
}

void same_global_function(LoginWidget *logwid)
{
    QLabel *label = new QLabel(
            QCoreApplication::translate("LoginWidget", "Password:"),
            logwid);
}
//! [13]

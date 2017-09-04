/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Network Auth module of the Qt Toolkit.
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

#include "ui_twitterdialog.h"
#include "twittertimelinemodel.h"

#include <functional>

#include <QUrl>
#include <QApplication>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QCommandLineParser>
#include <QCommandLineOption>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    app.setApplicationName("Twitter Timeline");
    app.setApplicationDisplayName("Twitter Timeline");
    app.setOrganizationDomain("qt.io");
    app.setOrganizationName("The Qt Company");

    QCommandLineParser parser;
    QCommandLineOption token(QStringList() << "k" << "consumer-key",
                             "Application consumer key", "key");
    QCommandLineOption secret(QStringList() << "s" << "consumer-secret",
                              "Application consumer secret", "secret");
    QCommandLineOption connect(QStringList() << "c" << "connect",
                               "Connects to twitter. Requires consumer-key and consumer-secret");

    parser.addOptions({ token, secret, connect });
    parser.process(app);

    struct TwitterDialog : QDialog, Ui::TwitterDialog {
        TwitterTimelineModel model;

        TwitterDialog()
            : QDialog()
        {
            setupUi(this);
            view->setModel(&model);
            view->horizontalHeader()->hideSection(0);
            view->horizontalHeader()->hideSection(1);
        }
    } twitterDialog;

    const auto authenticate = [&]() {
        const auto clientIdentifier = twitterDialog.clientIdLineEdit->text();
        const auto clientSharedSecret = twitterDialog.clientSecretLineEdit->text();
        twitterDialog.model.authenticate(qMakePair(clientIdentifier, clientSharedSecret));
    };
    const auto buttonSlot = [&]() {
        if (twitterDialog.model.status() == Twitter::Status::Granted)
            twitterDialog.model.updateTimeline();
        else
            authenticate();
    };

    twitterDialog.clientIdLineEdit->setText(parser.value(token));
    twitterDialog.clientSecretLineEdit->setText(parser.value(secret));
    if (parser.isSet(connect)) {
        if (parser.value(token).isEmpty() || parser.value(secret).isEmpty()) {
            parser.showHelp();
        } else {
            authenticate();
            twitterDialog.view->setFocus();
        }
    }

    QObject::connect(twitterDialog.pushButton, &QPushButton::clicked, buttonSlot);
    QObject::connect(&twitterDialog.model, &TwitterTimelineModel::authenticated,
                     std::bind(&QPushButton::setText, twitterDialog.pushButton, "&Update"));

    twitterDialog.show();
    return app.exec();
}

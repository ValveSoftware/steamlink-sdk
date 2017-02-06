/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Network Auth module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "ui_twitterdialog.h"
#include "twittertimelinemodel.h"

#include <functional>

#include <QUrl>
#include <QProcess>
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

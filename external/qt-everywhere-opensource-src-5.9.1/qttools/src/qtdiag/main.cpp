/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the tools applications of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
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
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qtdiag.h"

#include <QtGui/QGuiApplication>
#include <QtCore/QCommandLineParser>

#include <iostream>
#include <string>

QT_USE_NAMESPACE

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);

    QCoreApplication::setApplicationName(QStringLiteral("qtdiag"));
    QCoreApplication::setApplicationVersion(QLatin1String(QT_VERSION_STR));
    QCoreApplication::setOrganizationName(QStringLiteral("QtProject"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("qt-project.org"));

    QCommandLineParser commandLineParser;
    const QCommandLineOption noGlOption(QStringLiteral("no-gl"), QStringLiteral("Do not output GL information"));
    const QCommandLineOption glExtensionOption(QStringLiteral("gl-extensions"), QStringLiteral("List GL extensions"));
    const QCommandLineOption fontOption(QStringLiteral("fonts"), QStringLiteral("Output list of fonts"));
    commandLineParser.setApplicationDescription(QStringLiteral("Prints diagnostic output about the Qt library."));
    commandLineParser.addOption(noGlOption);
    commandLineParser.addOption(glExtensionOption);
    commandLineParser.addOption(fontOption);
    commandLineParser.addHelpOption();
    commandLineParser.process(app);
    unsigned flags = commandLineParser.isSet(noGlOption) ? 0u : unsigned(QtDiagGl);
    if (commandLineParser.isSet(glExtensionOption))
        flags |= QtDiagGlExtensions;
    if (commandLineParser.isSet(fontOption))
        flags |= QtDiagFonts;

    std::wcout << qtDiag(flags).toStdWString();
    return 0;
}

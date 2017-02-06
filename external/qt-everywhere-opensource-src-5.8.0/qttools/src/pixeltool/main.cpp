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

#include "qpixeltool.h"

#include <qapplication.h>
#include <qcommandlineparser.h>
#include <qcommandlineoption.h>
#include <qfileinfo.h>

QT_USE_NAMESPACE

static bool isOptionSet(int argc, char *argv[], const char *option)
{
    for (int i = 1; i < argc; ++i) {
        if (!qstrcmp(argv[i], option))
            return true;
    }
    return false;
}

int main(int argc, char **argv)
{
    if (isOptionSet(argc, argv, "--no-scaling"))
        QCoreApplication::setAttribute(Qt::AA_DisableHighDpiScaling);

    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QLatin1String("PixelTool"));
    QCoreApplication::setApplicationVersion(QLatin1String(QT_VERSION_STR));
    QCoreApplication::setOrganizationName(QLatin1String("QtProject"));

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption noScalingDummy(QStringLiteral("no-scaling"),
                                      QStringLiteral("Set Qt::AA_DisableHighDpiScaling."));
    parser.addOption(noScalingDummy);
    parser.addPositionalArgument(QLatin1String("preview"),
                                 QLatin1String("The preview image to show."));

    parser.process(app);

    QPixelTool pixelTool;

    if (!parser.positionalArguments().isEmpty()) {
        const QString previewImageFileName = parser.positionalArguments().first();
        if (QFileInfo(previewImageFileName).exists()) {
            QImage previewImage(previewImageFileName);
            if (!previewImage.size().isEmpty())
                pixelTool.setPreviewImage(previewImage);
        }
    }

    pixelTool.show();

    QObject::connect(&app, &QApplication::lastWindowClosed,
                     &app, &QCoreApplication::quit);

    return app.exec();
}

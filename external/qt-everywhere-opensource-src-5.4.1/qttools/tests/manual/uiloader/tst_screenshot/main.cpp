/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

/*
 * This program takes a *.ui file and an output dir as argument in order to
 * create a screenshot of the widget defined in the ui file.
 *
 * The screenshot is saved in the output dir (default current dir), ".png" is
 * appended to the ui file name.
 */

#include <QApplication>
#include <QWidget>
#include <QFile>
#include <QDebug>

#include <QDir>

#include <iostream>

using namespace std;


#ifdef Q_WS_QWS
// we don't compile designer on embedded...

int main(int argc, char **argv)
{
    return 0;
}


#else

#include <QUiLoader>

/*
 * Take the path of an ui file and return appropriate QWidget.
 */

QWidget* getWidgetFromUiFile(const QString& fileNameUiFile)
{
    qDebug() << "\t\t\t...loading ui file" << fileNameUiFile;

    QUiLoader loader;
    QFile uiFile(fileNameUiFile);
    if (!uiFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
               qDebug("\t\tError: QFile.open() failed.");
               exit(EXIT_FAILURE);
       }

    QFileInfo fileInfo(fileNameUiFile);
    QDir::setCurrent(fileInfo.absolutePath()); //for the stylesheet to find their images

    QWidget *ui = loader.load(&uiFile);
       if (!ui) {
               qDebug("\t\tError: Quilodader.load() returned NULL pointer.");
               exit(EXIT_FAILURE);
       }
    uiFile.close();

    return ui;
}



/*
 * Takes the actual screenshot.
 *
 * Hint: provide filename without extension, ".png" will be added
 */

void makeScreenshot(QWidget* widget, const QString& fileName, const QString& pathOutputDir)
{
    QFileInfo fileInfo(fileName);
    QString realFileName = fileInfo.completeBaseName() + "." + fileInfo.suffix() + ".png";
    QString realPath = pathOutputDir + "/" + realFileName;


    //QString realFileName = fileName + ".png";
    qDebug() << "\t\t\t...Taking screenshot" << fileInfo.absoluteFilePath();

    //widget->show();
    qApp->processEvents();
    QImage originalPixmap(widget->size(),QImage::Format_ARGB32);
    widget->render(&originalPixmap);
       if ( originalPixmap.isNull() ) {
               qDebug("\t\tError: QPixmap::grabWidget() returned a NULL QPixmap.");
               exit(EXIT_FAILURE);
       }
    //QString fileName = QDir::currentPath() + "/secondwidget." + format;
    if ( !originalPixmap.save(realPath, "PNG") ) {
               qDebug("\t\tError: QPixmap.save() failed.");
               exit(EXIT_FAILURE);
       }
    qDebug() << "\t\t\t...Screenshot saved in" << realPath;

    widget->close();
}



/*
 * Call this if you just want to pass the ui file name and the output dir.
 */

void createScreenshotFromUiFile(const QString& fileNameUiFile, const QString pathOutputDir)
{
    qDebug() << "\t\tCreating screenshot from widget defined in" << fileNameUiFile;

    QWidget* w = getWidgetFromUiFile(fileNameUiFile);
    makeScreenshot(w, fileNameUiFile, pathOutputDir);
}



/*
 * Start here.
 */

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    // check for necessary arguments
    if (argc == 1) {
        cout << "Syntax: " << argv[0] << " <path to *.ui file> [output directory]" << endl;
        cout << "" << endl;
        cout << "Takes a *.ui file and an output dir as argument in order to" << endl;
        cout << "create a screenshot of the widget defined in the ui file." << endl;
        cout << "" << endl;
        cout << "The screenshot is saved in the output dir (default current dir)," << endl;
        cout << "'.png' is appended to the ui file name." << endl;
        exit(EXIT_FAILURE);
    }


    // check for *.ui
    QString fileName = app.arguments().value(1);
    if ( !fileName.endsWith(".ui") ) {
        qDebug() << fileName + " is not a *.ui file.";
        exit(EXIT_FAILURE);
    }

    // does the file exist?
    QFile uiFile(fileName);
    if ( !uiFile.exists() ) {
        qDebug() << fileName + " does not exist.";
        exit(EXIT_FAILURE);
    }

    // check output directory
    QString pathOutputDir = QDir::currentPath();

    if (argc >= 3 ) {
        QDir outputDir = app.arguments().value(2);
        if ( outputDir.exists() ) {
            pathOutputDir = outputDir.absolutePath();
        } else {
            qDebug() << outputDir.absolutePath() + " does not exist or is not a directory.";
            exit(EXIT_FAILURE);
        }
    }

    // take the screenshot
    createScreenshotFromUiFile(fileName, pathOutputDir);

    app.quit();
    return 0;
}

#endif

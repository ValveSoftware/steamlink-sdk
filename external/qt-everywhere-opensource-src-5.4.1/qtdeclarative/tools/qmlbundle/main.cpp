/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the tools applications of the Qt Toolkit.
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

#include <private/qqmlbundle_p.h>
#include <QtCore/QCoreApplication>
#include <QtCore/QSet>
#include <QtCore/QStringList>
#include <iostream>

static bool createBundle(const QString &fileName, const QStringList &fileNames)
{
    QQmlBundle bundle(fileName);
    if (!bundle.open(QFile::WriteOnly))
        return false;
    foreach (const QString &fileName, fileNames)
        bundle.add(fileName);
    return true;
}

static bool removeFiles(const QString &fileName, const QStringList &fileNames)
{
    const QSet<QString> filesToRemove = QSet<QString>::fromList(fileNames);

    QQmlBundle bundle(fileName);
    bundle.open(QFile::ReadWrite);
    foreach (const QQmlBundle::FileEntry *entry, bundle.files()) {
        if (filesToRemove.contains(entry->fileName()))
            bundle.remove(entry);
    }
    return true;
}

static void showHelp()
{
    std::cerr << "Usage: qmlbundle <command> [<args>]" << std::endl
              << std::endl
              << "The commands are:" << std::endl
              << "  create     Create a new bundle" << std::endl
              << "  add        Add files to the bundle" << std::endl
              << "  rm         Remove files from the bundle" << std::endl
              << "  update     Add files to the bundle or update them if they are already added" << std::endl
              << "  ls         List the files in the bundle" << std::endl
              << "  cat        Concatenates files and print on the standard output" << std::endl
              << "  optimize   Insert optimization data for all recognised content" << std::endl
              << std::endl
              << "See 'qmlbundle help <command>' for more information on a specific command." << std::endl;
}

static void usage(const QString &action, const QString &error = QString())
{
    if (! error.isEmpty())
        std::cerr << qPrintable(error) << std::endl << std::endl;

    if (action == QLatin1String("create")) {
        std::cerr << "usage: qmlbundle create <bundle name> [files]" << std::endl;
    } else if (action == QLatin1String("add")) {
        std::cerr << "usage: qmlbundle add <bundle name> [files]" << std::endl;
    } else if (action == QLatin1String("rm")) {
        std::cerr << "usage: qmlbundle rm <bundle name> [files]" << std::endl;
    } else if (action == QLatin1String("update")) {
        std::cerr << "usage: qmlbundle update <bundle name> [files]" << std::endl;
    } else if (action == QLatin1String("ls")) {
        std::cerr << "usage: qmlbundle ls <bundle name>" << std::endl;
    } else if (action == QLatin1String("cat")) {
        std::cerr << "usage: qmlbundle cat <bundle name> [files]" << std::endl;
    } else {
        showHelp();
    }
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QStringList args = app.arguments();
    /*const QString exeName =*/ args.takeFirst();

    if (args.isEmpty()) {
        showHelp();
        return 0;
    }

    const QString action = args.takeFirst();

    if (action == QLatin1String("help")) {
        if (args.empty())
            showHelp();
        else
            usage(args.takeFirst());
    } else if (action == QLatin1String("ls")) {
        if (args.isEmpty()) {
            usage(action, "You must specify a bundle");
            return EXIT_FAILURE;
        }
        QQmlBundle bundle(args.takeFirst());
        if (bundle.open(QFile::ReadOnly)) {
            foreach (const QQmlBundle::FileEntry *fileEntry, bundle.files())
                std::cout << qPrintable(fileEntry->fileName()) << std::endl;
        }
    } else if (action == QLatin1String("create")) {
        if (args.isEmpty()) {
            usage(action, "You must specify a bundle");
            return EXIT_FAILURE;
        }
        const QString bundleFileName = args.takeFirst();
        createBundle(bundleFileName, args);
    } else if (action == QLatin1String("add")) {
        if (args.isEmpty()) {
            usage(action, "You must specify a bundle");
            return EXIT_FAILURE;
        }
        const QString bundleFileName = args.takeFirst();
        QQmlBundle bundle(bundleFileName);
        bundle.open();
        foreach (const QString &fileName, args) {
            if (! bundle.add(fileName))
                std::cerr << "cannot add file " << qPrintable(fileName) << " to " << qPrintable(bundleFileName) << std::endl;
        }
    } else if (action == QLatin1String("rm")) {
        if (args.isEmpty()) {
            usage(action, "You must specify a bundle");
            return EXIT_FAILURE;
        }
        const QString bundleFileName = args.takeFirst();
        removeFiles(bundleFileName, args);
    } else if (action == QLatin1String("update")) {
        if (args.isEmpty()) {
            usage(action, "You must specify a bundle");
            return EXIT_FAILURE;
        }
        const QString bundleFileName = args.takeFirst();
        removeFiles(bundleFileName, args);
        QQmlBundle bundle(bundleFileName);
        bundle.open();
        foreach (const QString &fileName, args) {
            if (! bundle.add(fileName))
                std::cerr << "cannot add file " << qPrintable(fileName) << " to " << qPrintable(bundleFileName) << std::endl;
        }
    } else if (action == QLatin1String("cat")) {
        if (args.isEmpty()) {
            usage(action, "You must specify a bundle");
            return EXIT_FAILURE;
        }
        const QString bundleFileName = args.takeFirst();
        QQmlBundle bundle(bundleFileName);
        if (bundle.open(QFile::ReadOnly)) {
            const QSet<QString> filesToShow = QSet<QString>::fromList(args);

            foreach (const QQmlBundle::FileEntry *fileEntry, bundle.files()) {
                if (filesToShow.contains(fileEntry->fileName()))
                    std::cout.write(fileEntry->contents(), fileEntry->fileSize());
            }
        }
    } else {
        showHelp();
    }

    return 0;
}

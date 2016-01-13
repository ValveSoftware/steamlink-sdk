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
#include <QString>
#include <QStringList>
#include <QDebug>
#include <iostream>
#include <QProcess>
#include <QDir>
#include <QRegExp>
#include <QSet>
#include <QStack>
#include <QDirIterator>
#include <QLibraryInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include "shared.h"

bool runStripEnabled = true;
bool alwaysOwerwriteEnabled = false;
bool runCodesign = false;
QString codesignIdentiy;
int logLevel = 1;

using std::cout;
using std::endl;

bool operator==(const FrameworkInfo &a, const FrameworkInfo &b)
{
    return ((a.frameworkPath == b.frameworkPath) && (a.binaryPath == b.binaryPath));
}

QDebug operator<<(QDebug debug, const FrameworkInfo &info)
{
    debug << "Framework name" << info.frameworkName << "\n";
    debug << "Framework directory" << info.frameworkDirectory << "\n";
    debug << "Framework path" << info.frameworkPath << "\n";
    debug << "Binary directory" << info.binaryDirectory << "\n";
    debug << "Binary name" << info.binaryName << "\n";
    debug << "Binary path" << info.binaryPath << "\n";
    debug << "Version" << info.version << "\n";
    debug << "Install name" << info.installName << "\n";
    debug << "Deployed install name" << info.deployedInstallName << "\n";
    debug << "Source file Path" << info.sourceFilePath << "\n";
    debug << "Framework Destination Directory (relative to bundle)" << info.frameworkDestinationDirectory << "\n";
    debug << "Binary Destination Directory (relative to bundle)" << info.binaryDestinationDirectory << "\n";

    return debug;
}

const QString bundleFrameworkDirectory = "Contents/Frameworks";
const QString bundleBinaryDirectory = "Contents/MacOS";

inline QDebug operator<<(QDebug debug, const ApplicationBundleInfo &info)
{
    debug << "Application bundle path" << info.path << "\n";
    debug << "Binary path" << info.binaryPath << "\n";
    debug << "Additional libraries" << info.libraryPaths << "\n";
    return debug;
}

bool copyFilePrintStatus(const QString &from, const QString &to)
{
    if (QFile(to).exists()) {
        if (alwaysOwerwriteEnabled) {
            QFile(to).remove();
        } else {
            qDebug() << "File exists, skip copy:" << to;
            return false;
        }
    }

    if (QFile::copy(from, to)) {
        QFile dest(to);
        dest.setPermissions(dest.permissions() | QFile::WriteOwner | QFile::WriteUser);
        LogNormal() << " copied:" << from;
        LogNormal() << " to" << to;

        // The source file might not have write permissions set. Set the
        // write permission on the target file to make sure we can use
        // install_name_tool on it later.
        QFile toFile(to);
        if (toFile.permissions() & QFile::WriteOwner)
            return true;

        if (!toFile.setPermissions(toFile.permissions() | QFile::WriteOwner)) {
            LogError() << "Failed to set u+w permissions on target file: " << to;
            return false;
        }

        return true;
    } else {
        LogError() << "file copy failed from" << from;
        LogError() << " to" << to;
        return false;
    }
}

bool linkFilePrintStatus(const QString &file, const QString &link)
{
    if (QFile(link).exists()) {
        if (QFile(link).symLinkTarget().isEmpty())
            LogError() << link << "exists but it's a file.";
        else
            LogNormal() << "Symlink exists, skipping:" << link;
        return false;
    } else if (QFile::link(file, link)) {
        LogNormal() << " symlink" << link;
        LogNormal() << " points to" << file;
        return true;
    } else {
        LogError() << "failed to symlink" << link;
        LogError() << " to" << file;
        return false;
    }
}

void patch_debugInInfoPlist(const QString &infoPlistPath)
{
    // Older versions of qmake may have the "_debug" binary as
    // the value for CFBundleExecutable. Remove it.
    QFile infoPlist(infoPlistPath);
    infoPlist.open(QIODevice::ReadOnly);
    QByteArray contents = infoPlist.readAll();
    infoPlist.close();
    infoPlist.open(QIODevice::WriteOnly | QIODevice::Truncate);
    contents.replace("_debug", ""); // surely there are no legit uses of "_debug" in an Info.plist
    infoPlist.write(contents);
}

FrameworkInfo parseOtoolLibraryLine(const QString &line, bool useDebugLibs)
{
    FrameworkInfo info;
    QString trimmed = line.trimmed();

    if (trimmed.isEmpty())
        return info;

    // Don't deploy system libraries.
    if (trimmed.startsWith("/System/Library/") ||
        (trimmed.startsWith("/usr/lib/") && trimmed.contains("libQt") == false) // exception for libQtuitools and libQtlucene
        || trimmed.startsWith("@executable_path") || trimmed.startsWith("@loader_path") || trimmed.startsWith("@rpath"))
        return info;

    enum State {QtPath, FrameworkName, DylibName, Version, End};
    State state = QtPath;
    int part = 0;
    QString name;
    QString qtPath;
    QString suffix = useDebugLibs ? "_debug" : "";

    // Split the line into [Qt-path]/lib/qt[Module].framework/Versions/[Version]/
    QStringList parts = trimmed.split("/");
    while (part < parts.count()) {
        const QString currentPart = parts.at(part).simplified() ;
        ++part;
        if (currentPart == "")
            continue;

        if (state == QtPath) {
            // Check for library name part
            if (part < parts.count() && parts.at(part).contains(".dylib ")) {
                info.installName += "/" + (qtPath + currentPart + "/").simplified();
                info.frameworkDirectory = info.installName;
                state = DylibName;
                continue;
            } else if (part < parts.count() && parts.at(part).endsWith(".framework")) {
                info.installName += "/" + (qtPath + "lib/").simplified();
                info.frameworkDirectory = info.installName;
                state = FrameworkName;
                continue;
            } else if (trimmed.startsWith("/") == false) {      // If the line does not contain a full path, the app is using a binary Qt package.
                if (currentPart.contains(".framework")) {
                    info.frameworkDirectory = "/Library/Frameworks/";
                    state = FrameworkName;
                } else {
                    info.frameworkDirectory = "/usr/lib/";
                    state = DylibName;
                }

                --part;
                continue;
            }
            qtPath += (currentPart + "/");

        } if (state == FrameworkName) {
            // remove ".framework"
            name = currentPart;
            name.chop(QString(".framework").length());
            info.isDylib = false;
            info.frameworkName = currentPart;
            state = Version;
            ++part;
            continue;
        } if (state == DylibName) {
            name = currentPart.split(" (compatibility").at(0);
            info.isDylib = true;
            info.frameworkName = name;
            info.binaryName = name.left(name.indexOf('.')) + suffix + name.mid(name.indexOf('.'));
            info.installName += name;
            info.deployedInstallName = "@executable_path/../Frameworks/" + info.binaryName;
            info.frameworkPath = info.frameworkDirectory + info.binaryName;
            info.sourceFilePath = info.frameworkPath;
            info.frameworkDestinationDirectory = bundleFrameworkDirectory + "/";
            info.binaryDestinationDirectory = info.frameworkDestinationDirectory;
            info.binaryDirectory = info.frameworkDirectory;
            info.binaryPath = info.frameworkPath;
            state = End;
            ++part;
            continue;
        } else if (state == Version) {
            info.version = currentPart;
            info.binaryDirectory = "Versions/" + info.version;
            info.binaryName = name + suffix;
            info.binaryPath = "/" + info.binaryDirectory + "/" + info.binaryName;
            info.installName += info.frameworkName + "/" + info.binaryDirectory + "/" + name;
            info.deployedInstallName = "@executable_path/../Frameworks/" + info.frameworkName + info.binaryPath;
            info.frameworkPath = info.frameworkDirectory + info.frameworkName;
            info.sourceFilePath = info.frameworkPath + info.binaryPath;
            info.frameworkDestinationDirectory = bundleFrameworkDirectory + "/" + info.frameworkName;
            info.binaryDestinationDirectory = info.frameworkDestinationDirectory + "/" + info.binaryDirectory;
            state = End;
        } else if (state == End) {
            break;
        }
    }

    return info;
}

QString findAppBinary(const QString &appBundlePath)
{
    QString appName = QFileInfo(appBundlePath).completeBaseName();
    QString binaryPath = appBundlePath  + "/Contents/MacOS/" + appName;

    if (QFile::exists(binaryPath))
        return binaryPath;
    LogError() << "Could not find bundle binary for" << appBundlePath;
    return QString();
}

QStringList findAppLibraries(const QString &appBundlePath)
{
    QStringList result;
    // dylibs
    QDirIterator iter(appBundlePath, QStringList() << QString::fromLatin1("*.dylib"),
            QDir::Files, QDirIterator::Subdirectories);

    while (iter.hasNext()) {
        iter.next();
        result << iter.fileInfo().filePath();
    }
    return result;
}

QStringList findAppBundleFiles(const QString &appBundlePath)
{
    QStringList result;

    QDirIterator iter(appBundlePath, QStringList() << QString::fromLatin1("*"),
            QDir::Files, QDirIterator::Subdirectories);

    while (iter.hasNext()) {
        iter.next();
        if (iter.fileInfo().isSymLink())
            continue;
        result << iter.fileInfo().filePath();
    }

    return result;
}

QList<FrameworkInfo> getQtFrameworks(const QStringList &otoolLines, bool useDebugLibs)
{
    QList<FrameworkInfo> libraries;
    foreach(const QString line, otoolLines) {
        FrameworkInfo info = parseOtoolLibraryLine(line, useDebugLibs);
        if (info.frameworkName.isEmpty() == false) {
            LogDebug() << "Adding framework:";
            LogDebug() << info;
            libraries.append(info);
        }
    }
    return libraries;
};

QList<FrameworkInfo> getQtFrameworks(const QString &path, bool useDebugLibs)
{
    LogDebug() << "Using otool:";
    LogDebug() << " inspecting" << path;
    QProcess otool;
    otool.start("otool", QStringList() << "-L" << path);
    otool.waitForFinished();

    if (otool.exitCode() != 0) {
        LogError() << otool.readAllStandardError();
    }

    QString output = otool.readAllStandardOutput();
    QStringList outputLines = output.split("\n");
    outputLines.removeFirst(); // remove line containing the binary path
    if (path.contains(".framework") || path.contains(".dylib"))
        outputLines.removeFirst(); // frameworks and dylibs print install name of themselves first.

    return getQtFrameworks(outputLines, useDebugLibs);
}

QList<FrameworkInfo> getQtFrameworksForPaths(const QStringList &paths, bool useDebugLibs)
{
    QList<FrameworkInfo> result;
    QSet<QString> existing;
    foreach (const QString &path, paths) {
        foreach (const FrameworkInfo &info, getQtFrameworks(path, useDebugLibs)) {
            if (!existing.contains(info.frameworkPath)) { // avoid duplicates
                existing.insert(info.frameworkPath);
                result << info;
            }
        }
    }
    return result;
}

QStringList getBinaryDependencies(const QString executablePath, const QString &path)
{
    QStringList binaries;

    QProcess otool;
    otool.start("otool", QStringList() << "-L" << path);
    otool.waitForFinished();

    if (otool.exitCode() != 0) {
        LogError() << otool.readAllStandardError();
    }

    QString output = otool.readAllStandardOutput();
    QStringList outputLines = output.split("\n");
    outputLines.removeFirst(); // remove line containing the binary path

    // return bundle-local dependencies. (those starting with @executable_path)
    foreach (const QString &line, outputLines) {
        QString trimmedLine = line.mid(0, line.indexOf("(")).trimmed(); // remove "(compatibility version ...)" and whitespace
        if (trimmedLine.startsWith("@executable_path/")) {
            QString binary = QDir::cleanPath(executablePath + trimmedLine.mid(QStringLiteral("@executable_path/").length()));
            if (binary != path)
                binaries.append(binary);
        }
    }

    return binaries;
}

// copies everything _inside_ sourcePath to destinationPath
bool recursiveCopy(const QString &sourcePath, const QString &destinationPath)
{
    if (!QDir(sourcePath).exists())
        return false;
    QDir().mkpath(destinationPath);

    LogNormal() << "copy:" << sourcePath << destinationPath;

    QStringList files = QDir(sourcePath).entryList(QStringList() << "*", QDir::Files | QDir::NoDotAndDotDot);
    foreach (QString file, files) {
        const QString fileSourcePath = sourcePath + "/" + file;
        const QString fileDestinationPath = destinationPath + "/" + file;
        copyFilePrintStatus(fileSourcePath, fileDestinationPath);
    }

    QStringList subdirs = QDir(sourcePath).entryList(QStringList() << "*", QDir::Dirs | QDir::NoDotAndDotDot);
    foreach (QString dir, subdirs) {
        recursiveCopy(sourcePath + "/" + dir, destinationPath + "/" + dir);
    }
    return true;
}

void recursiveCopyAndDeploy(const QString &appBundlePath, const QString &sourcePath, const QString &destinationPath)
{
    QDir().mkpath(destinationPath);

    LogNormal() << "copy:" << sourcePath << destinationPath;

    QStringList files = QDir(sourcePath).entryList(QStringList() << QStringLiteral("*"), QDir::Files | QDir::NoDotAndDotDot);
    foreach (QString file, files) {
        const QString fileSourcePath = sourcePath + QLatin1Char('/') + file;

        if (file.endsWith("_debug.dylib")) {
            continue; // Skip debug versions
        } else if (file.endsWith(QStringLiteral(".dylib"))) {
            // App store code signing rules forbids code binaries in Contents/Resources/,
            // which poses a problem for deploying mixed .qml/.dylib Qt Quick imports.
            // Solve this by placing the dylibs in Contents/PlugIns/quick, and then
            // creting a symlink to there from the Qt Quick import in Contents/Resources/.
            //
            // Example:
            // MyApp.app/Contents/Resources/qml/QtQuick/Controls/libqtquickcontrolsplugin.dylib ->
            // ../../../../PlugIns/quick/libqtquickcontrolsplugin.dylib
            //

            // The .dylib destination path:
            QString fileDestinationDir = appBundlePath + QStringLiteral("/Contents/PlugIns/quick/");
            QDir().mkpath(fileDestinationDir);
            QString fileDestinationPath = fileDestinationDir + file;

            // The .dylib symlink destination path:
            QString linkDestinationPath = destinationPath + QLatin1Char('/') + file;

            // The (relative) link; with a correct number of "../"'s.
            QString linkPath = QStringLiteral("PlugIns/quick/") + file;
            int cdupCount = linkDestinationPath.count(QStringLiteral("/"));
            for (int i = 0; i < cdupCount - 2; ++i)
                linkPath.prepend("../");

            if (copyFilePrintStatus(fileSourcePath, fileDestinationPath)) {
                linkFilePrintStatus(linkPath, linkDestinationPath);

                runStrip(fileDestinationPath);
                bool useDebugLibs = false;
                bool useLoaderPath = false;
                QList<FrameworkInfo> frameworks = getQtFrameworks(fileDestinationPath, useDebugLibs);
                deployQtFrameworks(frameworks, appBundlePath, QStringList(fileDestinationPath), useDebugLibs, useLoaderPath);
            }
        } else {
            QString fileDestinationPath = destinationPath + QLatin1Char('/') + file;
            copyFilePrintStatus(fileSourcePath, fileDestinationPath);
        }
    }

    QStringList subdirs = QDir(sourcePath).entryList(QStringList() << QStringLiteral("*"), QDir::Dirs | QDir::NoDotAndDotDot);
    foreach (QString dir, subdirs) {
        recursiveCopyAndDeploy(appBundlePath, sourcePath + QLatin1Char('/') + dir, destinationPath + QLatin1Char('/') + dir);
    }
}

QString copyDylib(const FrameworkInfo &framework, const QString path)
{
    if (!QFile::exists(framework.sourceFilePath)) {
        LogError() << "no file at" << framework.sourceFilePath;
        return QString();
    }

    // Construct destination paths. The full path typically looks like
    // MyApp.app/Contents/Frameworks/libfoo.dylib
    QString dylibDestinationDirectory = path + QLatin1Char('/') + framework.frameworkDestinationDirectory;
    QString dylibDestinationBinaryPath = dylibDestinationDirectory + QLatin1Char('/') + framework.binaryName;

    // Create destination directory
    if (!QDir().mkpath(dylibDestinationDirectory)) {
        LogError() << "could not create destination directory" << dylibDestinationDirectory;
        return QString();
    }

    // Retrun if the dylib has aleardy been deployed
    if (QFileInfo(dylibDestinationBinaryPath).exists() && !alwaysOwerwriteEnabled)
        return dylibDestinationBinaryPath;

    // Copy dylib binary
    copyFilePrintStatus(framework.sourceFilePath, dylibDestinationBinaryPath);
    return dylibDestinationBinaryPath;
}

QString copyFramework(const FrameworkInfo &framework, const QString path)
{
    if (!QFile::exists(framework.sourceFilePath)) {
        LogError() << "no file at" << framework.sourceFilePath;
        return QString();
    }

    // Construct destination paths. The full path typically looks like
    // MyApp.app/Contents/Frameworks/Foo.framework/Versions/5/QtFoo
    QString frameworkDestinationDirectory = path + QLatin1Char('/') + framework.frameworkDestinationDirectory;
    QString frameworkBinaryDestinationDirectory = frameworkDestinationDirectory + QLatin1Char('/') + framework.binaryDirectory;
    QString frameworkDestinationBinaryPath = frameworkBinaryDestinationDirectory + QLatin1Char('/') + framework.binaryName;

    // Return if the framework has aleardy been deployed
    if (QDir(frameworkDestinationDirectory).exists() && !alwaysOwerwriteEnabled)
        return QString();

    // Create destination directory
    if (!QDir().mkpath(frameworkBinaryDestinationDirectory)) {
        LogError() << "could not create destination directory" << frameworkBinaryDestinationDirectory;
        return QString();
    }

    // Now copy the framework. Some parts should be left out (headers/, .prl files).
    // Some parts should be included (Resources/, symlink structure). We want this
    // function to make as few assumtions about the framework as possible while at
    // the same time producing a codesign-compatible framework.

    // Copy framework binary
    copyFilePrintStatus(framework.sourceFilePath, frameworkDestinationBinaryPath);

    // Copy Resouces/, Libraries/ and Helpers/
    const QString resourcesSourcePath = framework.frameworkPath + "/Resources";
    const QString resourcesDestianationPath = frameworkDestinationDirectory + "/Versions/" + framework.version + "/Resources";
    recursiveCopy(resourcesSourcePath, resourcesDestianationPath);
    const QString librariesSourcePath = framework.frameworkPath + "/Libraries";
    const QString librariesDestianationPath = frameworkDestinationDirectory + "/Versions/" + framework.version + "/Libraries";
    bool createdLibraries = recursiveCopy(librariesSourcePath, librariesDestianationPath);
    const QString helpersSourcePath = framework.frameworkPath + "/Helpers";
    const QString helpersDestianationPath = frameworkDestinationDirectory + "/Versions/" + framework.version + "/Helpers";
    bool createdHelpers = recursiveCopy(helpersSourcePath, helpersDestianationPath);

    // Create symlink structure. Links at the framework root point to Versions/Current/
    // which again points to the actual version:
    // QtFoo.framework/QtFoo -> Versions/Current/QtFoo
    // QtFoo.framework/Resources -> Versions/Current/Resources
    // QtFoo.framework/Versions/Current -> 5
    linkFilePrintStatus("Versions/Current/" + framework.binaryName, frameworkDestinationDirectory + "/" + framework.binaryName);
    linkFilePrintStatus("Versions/Current/Resources", frameworkDestinationDirectory + "/Resources");
    if (createdLibraries)
        linkFilePrintStatus("Versions/Current/Libraries", frameworkDestinationDirectory + "/Libraries");
    if (createdHelpers)
        linkFilePrintStatus("Versions/Current/Helpers", frameworkDestinationDirectory + "/Helpers");
    linkFilePrintStatus(framework.version, frameworkDestinationDirectory + "/Versions/Current");

    // Correct Info.plist location for frameworks produced by older versions of qmake
    // Contents/Info.plist should be Versions/5/Resources/Info.plist
    const QString legacyInfoPlistPath = framework.frameworkPath + "/Contents/Info.plist";
    const QString correctInfoPlistPath = frameworkDestinationDirectory + "/Resources/Info.plist";
    if (QFile(legacyInfoPlistPath).exists()) {
        copyFilePrintStatus(legacyInfoPlistPath, correctInfoPlistPath);
        patch_debugInInfoPlist(correctInfoPlistPath);
    }
    return frameworkDestinationBinaryPath;
}

void runInstallNameTool(QStringList options)
{
    QProcess installNametool;
    installNametool.start("install_name_tool", options);
    installNametool.waitForFinished();
    if (installNametool.exitCode() != 0) {
        LogError() << installNametool.readAllStandardError();
        LogError() << installNametool.readAllStandardOutput();
    }
}

void changeIdentification(const QString &id, const QString &binaryPath)
{
    LogDebug() << "Using install_name_tool:";
    LogDebug() << " change identification in" << binaryPath;
    LogDebug() << " to" << id;
    runInstallNameTool(QStringList() << "-id" << id << binaryPath);
}

void changeInstallName(const QString &bundlePath, const FrameworkInfo &framework, const QStringList &binaryPaths, bool useLoaderPath)
{
    const QString absBundlePath = QFileInfo(bundlePath).absoluteFilePath();
    foreach (const QString &binary, binaryPaths) {
        QString deployedInstallName;
        if (useLoaderPath) {
            deployedInstallName = QLatin1String("@loader_path/")
                    + QFileInfo(binary).absoluteDir().relativeFilePath(absBundlePath + QLatin1Char('/') + framework.binaryDestinationDirectory + QLatin1Char('/') + framework.binaryName);
        } else {
            deployedInstallName = framework.deployedInstallName;
        }
        changeInstallName(framework.installName, deployedInstallName, binary);
    }
}

void changeInstallName(const QString &oldName, const QString &newName, const QString &binaryPath)
{
    LogDebug() << "Using install_name_tool:";
    LogDebug() << " in" << binaryPath;
    LogDebug() << " change reference" << oldName;
    LogDebug() << " to" << newName;
    runInstallNameTool(QStringList() << "-change" << oldName << newName << binaryPath);
}

void runStrip(const QString &binaryPath)
{
    if (runStripEnabled == false)
        return;

    LogDebug() << "Using strip:";
    LogDebug() << " stripped" << binaryPath;
    QProcess strip;
    strip.start("strip", QStringList() << "-x" << binaryPath);
    strip.waitForFinished();
    if (strip.exitCode() != 0) {
        LogError() << strip.readAllStandardError();
        LogError() << strip.readAllStandardOutput();
    }
}

/*
    Deploys the the listed frameworks listed into an app bundle.
    The frameworks are searched for dependencies, which are also deployed.
    (deploying Qt3Support will also deploy QtNetwork and QtSql for example.)
    Returns a DeploymentInfo structure containing the Qt path used and a
    a list of actually deployed frameworks.
*/
DeploymentInfo deployQtFrameworks(QList<FrameworkInfo> frameworks,
        const QString &bundlePath, const QStringList &binaryPaths, bool useDebugLibs,
                                  bool useLoaderPath)
{
    LogNormal();
    LogNormal() << "Deploying Qt frameworks found inside:" << binaryPaths;
    QStringList copiedFrameworks;
    DeploymentInfo deploymentInfo;
    deploymentInfo.useLoaderPath = useLoaderPath;

    while (frameworks.isEmpty() == false) {
        const FrameworkInfo framework = frameworks.takeFirst();
        copiedFrameworks.append(framework.frameworkName);

        // Get the qt path from one of the Qt frameworks;
        if (deploymentInfo.qtPath.isNull() && framework.frameworkName.contains("Qt")
            && framework.frameworkDirectory.contains("/lib"))
        {
            deploymentInfo.qtPath = framework.frameworkDirectory;
            deploymentInfo.qtPath.chop(5); // remove "/lib/"
        }

        if (framework.installName.startsWith("@executable_path/")) {
            LogError()  << framework.frameworkName << "already deployed, skipping.";
            continue;
        }

        // Install_name_tool the new id into the binaries
        changeInstallName(bundlePath, framework, binaryPaths, useLoaderPath);

        // Copy the framework/dylib to the app bundle.
        const QString deployedBinaryPath = framework.isDylib ? copyDylib(framework, bundlePath)
                                                             : copyFramework(framework, bundlePath);
        // Skip the rest if already was deployed.
        if (deployedBinaryPath.isNull())
            continue;

        runStrip(deployedBinaryPath);

        // Install_name_tool it a new id.
        changeIdentification(framework.deployedInstallName, deployedBinaryPath);


        // Check for framework dependencies
        QList<FrameworkInfo> dependencies = getQtFrameworks(deployedBinaryPath, useDebugLibs);

        foreach (FrameworkInfo dependency, dependencies) {
            changeInstallName(bundlePath, dependency, QStringList() << deployedBinaryPath, useLoaderPath);

            // Deploy framework if necessary.
            if (copiedFrameworks.contains(dependency.frameworkName) == false && frameworks.contains(dependency) == false) {
                frameworks.append(dependency);
            }
        }
    }
    deploymentInfo.deployedFrameworks = copiedFrameworks;
    return deploymentInfo;
}

DeploymentInfo deployQtFrameworks(const QString &appBundlePath, const QStringList &additionalExecutables, bool useDebugLibs)
{
   ApplicationBundleInfo applicationBundle;
   applicationBundle.path = appBundlePath;
   applicationBundle.binaryPath = findAppBinary(appBundlePath);
   applicationBundle.libraryPaths = findAppLibraries(appBundlePath);
   QStringList allBinaryPaths = QStringList() << applicationBundle.binaryPath << applicationBundle.libraryPaths
                                                 << additionalExecutables;
   QList<FrameworkInfo> frameworks = getQtFrameworksForPaths(allBinaryPaths, useDebugLibs);
   if (frameworks.isEmpty() && !alwaysOwerwriteEnabled) {
        LogWarning();
        LogWarning() << "Could not find any external Qt frameworks to deploy in" << appBundlePath;
        LogWarning() << "Perhaps macdeployqt was already used on" << appBundlePath << "?";
        LogWarning() << "If so, you will need to rebuild" << appBundlePath << "before trying again.";
        return DeploymentInfo();
   } else {
       return deployQtFrameworks(frameworks, applicationBundle.path, allBinaryPaths, useDebugLibs, !additionalExecutables.isEmpty());
   }
}

void deployPlugins(const ApplicationBundleInfo &appBundleInfo, const QString &pluginSourcePath,
        const QString pluginDestinationPath, DeploymentInfo deploymentInfo, bool useDebugLibs)
{
    LogNormal() << "Deploying plugins from" << pluginSourcePath;

    if (!pluginSourcePath.contains(deploymentInfo.pluginPath))
        return;

    // Plugin white list:
    QStringList pluginList;

    // Platform plugin:
    pluginList.append("platforms/libqcocoa.dylib");

    // Cocoa print support
    pluginList.append("printsupport/libcocoaprintersupport.dylib");

    // Network
    if (deploymentInfo.deployedFrameworks.contains(QStringLiteral("QtNetwork.framework"))) {
        QStringList bearerPlugins = QDir(pluginSourcePath +  QStringLiteral("/bearer")).entryList(QStringList() << QStringLiteral("*.dylib"));
        foreach (const QString &plugin, bearerPlugins) {
            if (!plugin.endsWith(QStringLiteral("_debug.dylib")))
                pluginList.append(QStringLiteral("bearer/") + plugin);
        }
    }

    // All image formats (svg if QtSvg.framework is used)
    QStringList imagePlugins = QDir(pluginSourcePath +  QStringLiteral("/imageformats")).entryList(QStringList() << QStringLiteral("*.dylib"));
    foreach (const QString &plugin, imagePlugins) {
        if (plugin.contains(QStringLiteral("qsvg"))) {
            if (deploymentInfo.deployedFrameworks.contains(QStringLiteral("QtSvg.framework")))
                pluginList.append(QStringLiteral("imageformats/") + plugin);
        } else if (!plugin.endsWith(QStringLiteral("_debug.dylib"))) {
            pluginList.append(QStringLiteral("imageformats/") + plugin);
        }
    }

    // Sql plugins if QtSql.framework is in use
    if (deploymentInfo.deployedFrameworks.contains(QStringLiteral("QtSql.framework"))) {
        QStringList sqlPlugins = QDir(pluginSourcePath +  QStringLiteral("/sqldrivers")).entryList(QStringList() << QStringLiteral("*.dylib"));
        foreach (const QString &plugin, sqlPlugins) {
            if (!plugin.endsWith(QStringLiteral("_debug.dylib")))
                pluginList.append(QStringLiteral("sqldrivers/") + plugin);
        }
    }

    // multimedia plugins if QtMultimedia.framework is in use
    if (deploymentInfo.deployedFrameworks.contains(QStringLiteral("QtMultimedia.framework"))) {
        QStringList plugins = QDir(pluginSourcePath + QStringLiteral("/mediaservice")).entryList(QStringList() << QStringLiteral("*.dylib"));
        foreach (const QString &plugin, plugins) {
            if (!plugin.endsWith(QStringLiteral("_debug.dylib")))
                pluginList.append(QStringLiteral("mediaservice/") + plugin);
        }
        plugins = QDir(pluginSourcePath + QStringLiteral("/audio")).entryList(QStringList() << QStringLiteral("*.dylib"));
        foreach (const QString &plugin, plugins) {
            if (!plugin.endsWith(QStringLiteral("_debug.dylib")))
                pluginList.append(QStringLiteral("audio/") + plugin);
        }
    }

    foreach (const QString &plugin, pluginList) {

        QString sourcePath = pluginSourcePath + "/" + plugin;
        if (useDebugLibs) {
            // Use debug plugins if found.
            QString debugSourcePath = sourcePath.replace(".dylib", "_debug.dylib");
            if (QFile::exists(debugSourcePath))
                sourcePath = debugSourcePath;
        }

        const QString destinationPath = pluginDestinationPath + "/" + plugin;
        QDir dir;
        dir.mkpath(QFileInfo(destinationPath).path());

        if (copyFilePrintStatus(sourcePath, destinationPath)) {
            runStrip(destinationPath);

            QList<FrameworkInfo> frameworks = getQtFrameworks(destinationPath, useDebugLibs);
            deployQtFrameworks(frameworks, appBundleInfo.path, QStringList() << destinationPath, useDebugLibs, deploymentInfo.useLoaderPath);

        }
    }
}

void createQtConf(const QString &appBundlePath)
{
    // Set Plugins and imports paths. These are relative to App.app/Contents.
    QByteArray contents = "[Paths]\n"
                          "Plugins = PlugIns\n"
                          "Imports = Resources/qml\n"
                          "Qml2Imports = Resources/qml\n";

    QString filePath = appBundlePath + "/Contents/Resources/";
    QString fileName = filePath + "qt.conf";

    QDir().mkpath(filePath);

    QFile qtconf(fileName);
    if (qtconf.exists() && !alwaysOwerwriteEnabled) {
        LogWarning();
        LogWarning() << fileName << "already exists, will not overwrite.";
        LogWarning() << "To make sure the plugins are loaded from the correct location,";
        LogWarning() << "please make sure qt.conf contains the following lines:";
        LogWarning() << "[Paths]";
        LogWarning() << "  Plugins = PlugIns";
        return;
    }

    qtconf.open(QIODevice::WriteOnly);
    if (qtconf.write(contents) != -1) {
        LogNormal() << "Created configuration file:" << fileName;
        LogNormal() << "This file sets the plugin search path to" << appBundlePath + "/Contents/PlugIns";
    }
}

void deployPlugins(const QString &appBundlePath, DeploymentInfo deploymentInfo, bool useDebugLibs)
{
    ApplicationBundleInfo applicationBundle;
    applicationBundle.path = appBundlePath;
    applicationBundle.binaryPath = findAppBinary(appBundlePath);

    const QString pluginDestinationPath = appBundlePath + "/" + "Contents/PlugIns";
    deployPlugins(applicationBundle, deploymentInfo.pluginPath, pluginDestinationPath, deploymentInfo, useDebugLibs);
}

void deployQmlImport(const QString &appBundlePath, const QString &importSourcePath, const QString &importName)
{
    QString importDestinationPath = appBundlePath + "/Contents/Resources/qml/" + importName;

    // Skip already deployed imports. This can happen in cases like "QtQuick.Controls.Styles",
    // where deploying QtQuick.Controls will also deploy the "Styles" sub-import.
    if (QDir().exists(importDestinationPath))
        return;

    recursiveCopyAndDeploy(appBundlePath, importSourcePath, importDestinationPath);
}

// Scan qml files in qmldirs for import statements, deploy used imports from Qml2ImportsPath to Contents/Resources/qml.
void deployQmlImports(const QString &appBundlePath, QStringList &qmlDirs)
{
    LogNormal() << "";
    LogNormal() << "Deploying QML imports ";
    LogNormal() << "Application QML file search path(s) is" << qmlDirs;

    // verify that qmlimportscanner is in BinariesPath
    QString qmlImportScannerPath = QDir::cleanPath(QLibraryInfo::location(QLibraryInfo::BinariesPath) + "/qmlimportscanner");
    if (!QFile(qmlImportScannerPath).exists()) {
        LogError() << "qmlimportscanner not found at" << qmlImportScannerPath;
        LogError() << "Rebuild qtdeclarative/tools/qmlimportscanner";
        return;
    }

    // build argument list for qmlimportsanner: "-rootPath foo/ -rootPath bar/ -importPath path/to/qt/qml"
    // ("rootPath" points to a directory containing app qml, "importPath" is where the Qt imports are installed)
    QStringList argumentList;
    foreach (const QString &qmlDir, qmlDirs) {
        argumentList.append("-rootPath");
        argumentList.append(qmlDir);
    }
    QString qmlImportsPath = QLibraryInfo::location(QLibraryInfo::Qml2ImportsPath);
    argumentList.append( "-importPath");
    argumentList.append(qmlImportsPath);

    // run qmlimportscanner
    QProcess qmlImportScanner;
    qmlImportScanner.start(qmlImportScannerPath, argumentList);
    if (!qmlImportScanner.waitForStarted()) {
        LogError() << "Could not start qmlimpoortscanner. Process error is" << qmlImportScanner.errorString();
        return;
    }
    qmlImportScanner.waitForFinished();

    // log qmlimportscanner errors
    qmlImportScanner.setReadChannel(QProcess::StandardError);
    QByteArray errors = qmlImportScanner.readAll();
    if (!errors.isEmpty()) {
        LogWarning() << "QML file parse error (deployment will continue):";
        LogWarning() << errors;
    }

    // parse qmlimportscanner json
    qmlImportScanner.setReadChannel(QProcess::StandardOutput);
    QByteArray json = qmlImportScanner.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isArray()) {
        LogError() << "qmlimportscanner output error. Expected json array, got:";
        LogError() << json;
        return;
    }

    // deploy each import
    foreach (const QJsonValue &importValue, doc.array()) {
        if (!importValue.isObject())
            continue;

        QJsonObject import = importValue.toObject();
        QString name = import["name"].toString();
        QString path = import["path"].toString();
        QString type = import["type"].toString();

        LogNormal() << "Deploying QML import" << name;

        // Skip imports with missing info - path will be empty if the import is not found.
        if (name.isEmpty() || path.isEmpty()) {
            LogNormal() << "  Skip import: name or path is empty";
            LogNormal() << "";
            continue;
        }

        // Deploy module imports only, skip directory (local/remote) and js imports. These
        // should be deployed as a part of the application build.
        if (type != QStringLiteral("module")) {
            LogNormal() << "  Skip non-module import";
            LogNormal() << "";
            continue;
        }

        // Create the destination path from the name
        // and version (grabbed from the source path)
        // ### let qmlimportscanner provide this.
        name.replace(QLatin1Char('.'), QLatin1Char('/'));
        int secondTolast = path.length() - 2;
        QString version = path.mid(secondTolast);
        if (version.startsWith(QLatin1Char('.')))
            name.append(version);

        deployQmlImport(appBundlePath, path, name);
        LogNormal() << "";
    }
}

void changeQtFrameworks(const QList<FrameworkInfo> frameworks, const QStringList &binaryPaths, const QString &absoluteQtPath)
{
    LogNormal() << "Changing" << binaryPaths << "to link against";
    LogNormal() << "Qt in" << absoluteQtPath;
    QString finalQtPath = absoluteQtPath;

    if (!absoluteQtPath.startsWith("/Library/Frameworks"))
        finalQtPath += "/lib/";

    foreach (FrameworkInfo framework, frameworks) {
        const QString oldBinaryId = framework.installName;
        const QString newBinaryId = finalQtPath + framework.frameworkName +  framework.binaryPath;
        foreach (const QString &binary, binaryPaths)
            changeInstallName(oldBinaryId, newBinaryId, binary);
    }
}

void changeQtFrameworks(const QString appPath, const QString &qtPath, bool useDebugLibs)
{
    const QString appBinaryPath = findAppBinary(appPath);
    const QStringList libraryPaths = findAppLibraries(appPath);
    const QList<FrameworkInfo> frameworks = getQtFrameworksForPaths(QStringList() << appBinaryPath << libraryPaths, useDebugLibs);
    if (frameworks.isEmpty()) {
        LogWarning();
        LogWarning() << "Could not find any _external_ Qt frameworks to change in" << appPath;
        return;
    } else {
        const QString absoluteQtPath = QDir(qtPath).absolutePath();
        changeQtFrameworks(frameworks, QStringList() << appBinaryPath << libraryPaths, absoluteQtPath);
    }
}

void codesignFile(const QString &identity, const QString &filePath)
{
    if (!runCodesign)
        return;

    LogNormal() << "codesign" << filePath;

    QProcess codesign;
    codesign.start("codesign", QStringList() << "--preserve-metadata=identifier,entitlements,resource-rules"
                                             << "--force" << "-s" << identity << filePath);
    codesign.waitForFinished(-1);

    QByteArray err = codesign.readAllStandardError();
    if (codesign.exitCode() > 0) {
        LogError() << "Codesign signing error:";
        LogError() << err;
    } else if (!err.isEmpty()) {
        LogDebug() << err;
    }
}

void codesign(const QString &identity, const QString &appBundlePath)
{
    // Code sign all binaries in the app bundle. This needs to
    // be done inside-out, e.g sign framework dependencies
    // before the main app binary. The codesign tool itself has
    // a "--deep" option to do this, but usage when signing is
    // not recommended: "Signing with --deep is for emergency
    // repairs and temporary adjustments only."

    LogNormal() << "";
    LogNormal() << "Signing" << appBundlePath << "with identity" << identity;

    QStack<QString> pendingBinaries;
    QSet<QString> signedBinaries;

    // Create the root code-binary set. This set consists of the application
    // executable(s) and the plugins.
    QString rootBinariesPath = appBundlePath + "/Contents/MacOS/";
    QStringList foundRootBinaries = QDir(rootBinariesPath).entryList(QStringList() << "*", QDir::Files);
    foreach (const QString &binary, foundRootBinaries)
        pendingBinaries.push(rootBinariesPath + binary);

    QStringList foundPluginBinaries = findAppBundleFiles(appBundlePath + "/Contents/PlugIns/");
    foreach (const QString &binary, foundPluginBinaries)
         pendingBinaries.push(binary);


    // Sign all binares; use otool to find and sign dependencies first.
    while (!pendingBinaries.isEmpty()) {
        QString binary = pendingBinaries.pop();
        if (signedBinaries.contains(binary))
            continue;

        // Check if there are unsigned dependencies, sign these first
        QStringList dependencies = getBinaryDependencies(rootBinariesPath, binary).toSet().subtract(signedBinaries).toList();
        if (!dependencies.isEmpty()) {
            pendingBinaries.push(binary);
            foreach (const QString &dependency, dependencies)
                pendingBinaries.push(dependency);
            continue;
        }
        // All dependencies are signed, now sign this binary
        codesignFile(identity, binary);
        signedBinaries.insert(binary);
    }

    // Verify code signature
    QProcess codesign;
    codesign.start("codesign", QStringList() << "--deep" << "-v" << appBundlePath);
    codesign.waitForFinished(-1);
    QByteArray err = codesign.readAllStandardError();
    if (codesign.exitCode() > 0) {
        LogError() << "codesign verification error:";
        LogError() << err;
    } else if (!err.isEmpty()) {
        LogDebug() << err;
    }
}

void createDiskImage(const QString &appBundlePath)
{
    QString appBaseName = appBundlePath;
    appBaseName.chop(4); // remove ".app" from end

    QString dmgName = appBaseName + ".dmg";

    QFile dmg(dmgName);

    if (dmg.exists() && alwaysOwerwriteEnabled)
        dmg.remove();

    if (dmg.exists()) {
        LogNormal() << "Disk image already exists, skipping .dmg creation for" << dmg.fileName();
    } else {
        LogNormal() << "Creating disk image (.dmg) for" << appBundlePath;
    }

    // More dmg options can be found in the hdiutil man page.
    QStringList options = QStringList()
            << "create" << dmgName
            << "-srcfolder" << appBundlePath
            << "-format" << "UDZO"
            << "-volname" << appBaseName;

    QProcess hdutil;
    hdutil.start("hdiutil", options);
    hdutil.waitForFinished(-1);
}

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
#include <qtest.h>
#include <QDeclarativeView>
#include <QApplication>
#include <QLibraryInfo>
#include <QDir>
#include <QDebug>
#include <QProcess>
#include <QFile>

enum Mode { Record, RecordNoVisuals, RecordSnapshot, Play, TestVisuals, RemoveVisuals, UpdateVisuals, UpdatePlatformVisuals, Test };

static QString testdir;
class tst_qmlvisual : public QObject
{
    Q_OBJECT
public:
    tst_qmlvisual();

    static QString toTestScript(const QString &, Mode=Test);
    static QString viewer();

    static QStringList findQmlFiles(const QDir &d);
private slots:
    void initTestCase();
    void visual_data();
    void visual();

private:
    const QString qmlruntime;
};


tst_qmlvisual::tst_qmlvisual() :
    qmlruntime(viewer())
{
}

QString tst_qmlvisual::viewer()
{
    QDir binaryFolder = QLibraryInfo::location(QLibraryInfo::BinariesPath);

#if defined(Q_OS_MAC)
    return binaryFolder.absoluteFilePath(QStringLiteral("QMLViewer.app/Contents/MacOS/QMLViewer"));
#elif defined(Q_OS_WIN)
    return binaryFolder.absoluteFilePath(QStringLiteral("qmlviewer.exe"));
#else
    return binaryFolder.absoluteFilePath(QStringLiteral("qmlviewer"));
#endif
}

void tst_qmlvisual::initTestCase()
{
    QVERIFY2(QFileInfo(qmlruntime).isExecutable(),
             qPrintable(QString::fromLatin1("'%1' does not exist or is not executable").arg(qmlruntime)));
}

void tst_qmlvisual::visual_data()
{
    QTest::addColumn<QString>("file");
    QTest::addColumn<QString>("testdata");

    QStringList files;
    files << findQmlFiles(QDir(QT_TEST_SOURCE_DIR));
    if (qgetenv("QMLVISUAL_ALL") != "1") {
#if defined(Q_OS_MAC)
        //Text on Mac varies per version. Only check the text on 10.6
        if(QSysInfo::MacintoshVersion != QSysInfo::MV_10_6)
            foreach(const QString &str, files.filter(QRegExp(".*text.*")))
                files.removeAll(str);
#endif
    }

    foreach (const QString &file, files) {
        QString testdata = toTestScript(file);
        if (testdata.isEmpty())
            continue;

        QTest::newRow(file.toLatin1().constData()) << file << testdata;
    }
}

static inline QByteArray msgQmlViewerFail(const QString &runTime,
                                          const QStringList &arguments,
                                          const QByteArray &output,
                                          const QString &why)
{
    const QString result =
        QString::fromLatin1("The command '%1 %2' failed: %3\nOutput:\n%4")
            .arg(QDir::toNativeSeparators(runTime),
                 arguments.join(QLatin1String(" ")),
                 why,
                 QString::fromLocal8Bit(output));
    return result.toLocal8Bit();
}

void tst_qmlvisual::visual()
{
    QFETCH(QString, file);
    QFETCH(QString, testdata);

    QStringList arguments;
#ifdef Q_OS_MAC
    arguments << "-no-opengl";
#endif
    arguments << "-script" << testdata
              << "-scriptopts" << "play,testimages,testerror,testskip,exitoncomplete,exitonfailure"
              << file;

    QProcess p;
    p.start(qmlruntime, arguments);
    QVERIFY2(p.waitForStarted(),
             qPrintable(QString::fromLatin1("Cannot start '%1': %2").
                        arg(qmlruntime, p.errorString())));
    bool finished = p.waitForFinished();
    QByteArray output = p.readAllStandardOutput() + p.readAllStandardError();
    QVERIFY2(finished, msgQmlViewerFail(qmlruntime, arguments, output, QStringLiteral("timeout")).constData());
    QVERIFY2(p.exitStatus() == QProcess::NormalExit, msgQmlViewerFail(qmlruntime, arguments, output, QStringLiteral("A crash occurred")).constData());
    QVERIFY2(p.exitCode() == 0, msgQmlViewerFail(qmlruntime, arguments, output, QString::fromLatin1("Exit code %1").arg(p.exitCode())).constData());
}

QString tst_qmlvisual::toTestScript(const QString &file, Mode mode)
{
    if (!file.endsWith(".qml"))
        return QString();

    int index = file.lastIndexOf(QDir::separator());
    if (index == -1)
        index = file.lastIndexOf('/');
    if (index == -1)
        return QString();

    const char* platformsuffix=0; // platforms with different fonts
#if defined(Q_OS_OSX)
    platformsuffix = "-MAC";
#elif defined(Q_OS_WIN)
    platformsuffix = "-WIN";
#else
    platformsuffix = "-X11";
#endif

    QString testdata = file.left(index + 1) +
                       QString("data");
    QString testname = file.mid(index + 1, file.length() - index - 5);

    if (platformsuffix && (mode == UpdatePlatformVisuals || QFile::exists(testdata+QLatin1String(platformsuffix)+QDir::separator()+testname+".qml"))) {
        QString platformdir = testdata + QLatin1String(platformsuffix);
        if (mode == UpdatePlatformVisuals) {
            if (!QDir().mkpath(platformdir)) {
                qFatal("Cannot make path %s", qPrintable(platformdir));
            }
            // Copy from base
            QDir dir(testdata,testname+".*");
            dir.setFilter(QDir::Files);
            QFileInfoList list = dir.entryInfoList();
            for (int i = 0; i < list.size(); ++i) {
                QFile in(list.at(i).filePath());
                if (!in.open(QIODevice::ReadOnly)) {
                    qFatal("Cannot open file %s: %s", qPrintable(in.fileName()), qPrintable(in.errorString()));
                }
                QFile out(platformdir + QDir::separator() + list.at(i).fileName());
                if (!out.open(QIODevice::WriteOnly)) {
                    qFatal("Cannot open file %s: %s", qPrintable(out.fileName()), qPrintable(out.errorString()));
                }
                out.write(in.readAll());
            }
        }
        testdata = platformdir;
    }

    testdata += QDir::separator() + testname;

    return testdata;
}

QStringList tst_qmlvisual::findQmlFiles(const QDir &d)
{
    QStringList rv;

    QStringList files = d.entryList(QStringList() << QLatin1String("*.qml"),
                                    QDir::Files);
    foreach (const QString &file, files) {
        if (file.at(0).isLower()) {
            rv << d.absoluteFilePath(file);
        }
    }

    QStringList dirs = d.entryList(QDir::Dirs | QDir::NoDotAndDotDot |
                                   QDir::NoSymLinks);
    foreach (const QString &dir, dirs) {
        if (dir.left(4) == "data")
            continue;

        QDir sub = d;
        sub.cd(dir);
        rv << findQmlFiles(sub);
    }

    return rv;
}

void action(Mode mode, const QString &file)
{
    QString testdata = tst_qmlvisual::toTestScript(file,mode);

    QStringList arguments;
#ifdef Q_OS_MAC
    arguments << "-no-opengl";
#endif
    switch (mode) {
        case Test:
            // Don't run qml
            break;
        case Record:
            arguments << "-script" << testdata
                  << "-scriptopts" << "record,testimages,saveonexit"
                  << file;
            break;
        case RecordNoVisuals:
            arguments << "-script" << testdata
                  << "-scriptopts" << "record,saveonexit"
                  << file;
            break;
        case RecordSnapshot:
            arguments << "-script" << testdata
                  << "-scriptopts" << "record,testimages,snapshot,saveonexit"
                  << file;
            break;
        case Play:
            arguments << "-script" << testdata
                  << "-scriptopts" << "play,testimages,testerror,testskip,exitoncomplete"
                  << file;
            break;
        case TestVisuals:
        arguments << "-script" << testdata
                  << "-scriptopts" << "play"
                  << file;
            break;
        case UpdateVisuals:
        case UpdatePlatformVisuals:
            arguments << "-script" << testdata
                  << "-scriptopts" << "play,record,testimages,exitoncomplete,saveonexit"
                  << file;
            break;
        case RemoveVisuals:
            arguments << "-script" << testdata
                  << "-scriptopts" << "play,record,exitoncomplete,saveonexit"
                  << file;
            break;
    }
    if (!arguments.isEmpty()) {
        QProcess p;
        p.setProcessChannelMode(QProcess::ForwardedChannels);
        const QString qmlRunTime = tst_qmlvisual::viewer();
        p.start(qmlRunTime, arguments);
        QVERIFY2(p.waitForStarted(),
                 qPrintable(QString::fromLatin1("Cannot start '%1': %2").
                            arg(qmlRunTime, p.errorString())));

        p.waitForFinished();
    }
}

void usage()
{
    fprintf(stderr, "\n");
    fprintf(stderr, "QML related options\n");
    fprintf(stderr, " -listtests                  : list all the tests seen by tst_qmlvisual, and then exit immediately\n");
    fprintf(stderr, " -record file                : record new test data for file\n");
    fprintf(stderr, " -recordnovisuals file       : record new test data for file, but ignore visuals\n");
    fprintf(stderr, " -recordsnapshot file        : record new snapshot for file (like record but only records a single frame and then exits)\n");
    fprintf(stderr, " -play file                  : playback test data for file, printing errors\n");
    fprintf(stderr, " -testvisuals file           : playback test data for file, without errors\n");
    fprintf(stderr, " -updatevisuals file         : playback test data for file, accept new visuals for file\n");
    fprintf(stderr, " -updateplatformvisuals file : playback test data for file, accept new visuals for file only on current platform (MacOSX/Win32/X11/QWS/S60)\n");
    fprintf(stderr, "\n"
        "Visual tests are recordings of manual interactions with a QML test,\n"
        "that can then be run automatically. To record a new test, run:\n"
        "\n"
        "  tst_qmlvisual -record yourtestdir/yourtest.qml\n"
        "\n"
        "This records everything you do (try to keep it short).\n"
        "To play back a test, run:\n"
        "\n"
        "  tst_qmlvisual -play yourtestdir/yourtest.qml\n"
        "\n"
        "Your test may include QML code to test itself, reporting any error to an\n"
        "'error' property on the root object - the test will fail if this property\n"
        "gets set to anything non-empty.\n"
        "\n"
        "If your test changes slightly but is still correct (check with -play), you\n"
        "can update the visuals by running:\n"
        "\n"
        "  tst_qmlvisual -updatevisuals yourtestdir/yourtest.qml\n"
        "\n"
        "If your test includes platform-sensitive visuals (eg. text in system fonts),\n"
        "you should create platform-specific visuals, using -updateplatformvisuals\n"
        "instead.\n"
        "\n"
        "If you ONLY wish to use the 'error' property, you can record your test with\n"
        "-recordnovisuals, or discard existing visuals with -removevisuals; the test\n"
        "will then only fail on a syntax error, crash, or non-empty 'error' property.\n"
        "\n"
        "If your test has anything set to the 'skip' property on the root object then\n"
        "test failures will be ignored. This allows for an opt-out of automated\n"
        "aggregation of test results. The value of the 'skip' property (usually a\n"
        "string) will then be printed to stdout when the test is run as part of the\n"
        "message saying the test has been skipped.\n"
    );
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    Mode mode = Test;
    QString file;
    bool showHelp = false;

    int newArgc = 1;
    char **newArgv = new char*[argc];
    newArgv[0] = argv[0];

    for (int ii = 1; ii < argc; ++ii) {
        QString arg(argv[ii]);
        if (arg == "-play" && (ii + 1) < argc) {
            mode = Play;
            file = argv[++ii];
        } else if (arg == "-record" && (ii + 1) < argc) {
            mode = Record;
            file = argv[++ii];
        } else if (arg == "-recordnovisuals" && (ii + 1) < argc) {
            mode = RecordNoVisuals;
            file = argv[++ii];
        }  else if (arg == "-recordsnapshot" && (ii + 1) < argc) {
            mode = RecordSnapshot;
            file = argv[++ii];
        } else if (arg == "-testvisuals" && (ii + 1) < argc) {
            mode = TestVisuals;
            file = argv[++ii];
        } else if (arg == "-removevisuals" && (ii + 1) < argc) {
            mode = RemoveVisuals;
            file = argv[++ii];
        } else if (arg == "-updatevisuals" && (ii + 1) < argc) {
            mode = UpdateVisuals;
            file = argv[++ii];
        } else if (arg == "-updateplatformvisuals" && (ii + 1) < argc) {
            mode = UpdatePlatformVisuals;
            file = argv[++ii];
        } else {
            newArgv[newArgc++] = argv[ii];
        }

        if (arg == "-help" || arg == "-?" || arg == "--help") {
            atexit(usage);
            showHelp = true;
        }

        if (arg == "-listtests") {
            QStringList list = tst_qmlvisual::findQmlFiles(QDir(QT_TEST_SOURCE_DIR));
            foreach (QString test, list) {
                qWarning() << qPrintable(test);
            }
            return 0;
        }
    }

    if (mode == Test || showHelp) {
        tst_qmlvisual tc;
        return QTest::qExec(&tc, newArgc, newArgv);
    } else {
        if (!file.endsWith(QLatin1String(".qml"))) {
            qWarning() << "Error: Invalid file name (must end in .qml)";
            return -1;
        }
        QDir d = QDir::current();
        QString absFile = d.absoluteFilePath(file);
        if (!QFile::exists(absFile)) {
            qWarning() << "Error: File does not exist";
            return -1;
        }

        action(mode, absFile);
        return 0;
    }
}

#include "tst_qmlvisual.moc"

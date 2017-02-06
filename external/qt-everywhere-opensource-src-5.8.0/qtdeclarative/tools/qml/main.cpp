/****************************************************************************
**
** Copyright (C) 2016 Research In Motion.
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

#include "conf.h"

#include <QCoreApplication>

#ifdef QT_GUI_LIB
#include <QGuiApplication>
#include <QWindow>
#include <QFileOpenEvent>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#ifdef QT_WIDGETS_LIB
#include <QApplication>
#endif // QT_WIDGETS_LIB
#endif // QT_GUI_LIB

#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStringList>
#include <QScopedPointer>
#include <QDebug>
#include <QStandardPaths>
#include <QTranslator>
#include <QtGlobal>
#include <QLibraryInfo>
#include <qqml.h>
#include <qqmldebug.h>
#include <private/qabstractanimation_p.h>

#include <cstdio>
#include <cstring>
#include <cstdlib>

#define VERSION_MAJ 1
#define VERSION_MIN 1
#define VERSION_STR "1.1"

#define FILE_OPEN_EVENT_WAIT_TIME 3000 // ms

static Config *conf = 0;
static QQmlApplicationEngine *qae = 0;
static int exitTimerId = -1;
bool verboseMode = false;

static void loadConf(const QString &override, bool quiet) // Terminates app on failure
{
    const QString defaultFileName = QLatin1String("configuration.qml");
    QUrl settingsUrl;
    bool builtIn = false; //just for keeping track of the warning
    if (override.isEmpty()) {
        QFileInfo fi;
        fi.setFile(QStandardPaths::locate(QStandardPaths::DataLocation, defaultFileName));
        if (fi.exists()) {
            settingsUrl = QUrl::fromLocalFile(fi.absoluteFilePath());
        } else {
            // ### If different built-in configs are needed per-platform, just apply QFileSelector to the qrc conf.qml path
            settingsUrl = QUrl(QLatin1String("qrc:///qt-project.org/QmlRuntime/conf/") + defaultFileName);
            builtIn = true;
        }
    } else {
        QFileInfo fi;
        fi.setFile(override);
        if (!fi.exists()) {
            printf("qml: Couldn't find required configuration file: %s\n",
                    qPrintable(QDir::toNativeSeparators(fi.absoluteFilePath())));
            exit(1);
        }
        settingsUrl = QUrl::fromLocalFile(fi.absoluteFilePath());
    }

    if (!quiet) {
        printf("qml: %s\n", QLibraryInfo::build());
        if (builtIn)
            printf("qml: Using built-in configuration.\n");
        else
            printf("qml: Using configuration file: %s\n",
                    qPrintable(settingsUrl.isLocalFile()
                    ? QDir::toNativeSeparators(settingsUrl.toLocalFile())
                    : settingsUrl.toString()));
    }

    // TODO: When we have better engine control, ban QtQuick* imports on this engine
    QQmlEngine e2;
    QQmlComponent c2(&e2, settingsUrl);
    conf = qobject_cast<Config*>(c2.create());

    if (!conf){
        printf("qml: Error loading configuration file: %s\n", qPrintable(c2.errorString()));
        exit(1);
    }
}

#ifdef QT_GUI_LIB

void noFilesGiven();

// Loads qml after receiving a QFileOpenEvent
class LoaderApplication : public QGuiApplication
{
public:
    LoaderApplication(int& argc, char **argv) : QGuiApplication(argc, argv) {}

    bool event(QEvent *ev)
    {
        if (ev->type() == QEvent::FileOpen) {
            if (exitTimerId >= 0) {
                killTimer(exitTimerId);
                exitTimerId = -1;
            }
            qae->load(static_cast<QFileOpenEvent *>(ev)->url());
        }
        else
            return QGuiApplication::event(ev);
        return true;
    }

    void timerEvent(QTimerEvent *) {
        noFilesGiven();
    }
};

#endif // QT_GUI_LIB

// Listens to the appEngine signals to determine if all files failed to load
class LoadWatcher : public QObject
{
    Q_OBJECT
public:
    LoadWatcher(QQmlApplicationEngine *e, int expected)
        : QObject(e)
        , earlyExit(false)
        , returnCode(0)
        , expect(expected)
        , haveOne(false)
    {
        connect(e, SIGNAL(objectCreated(QObject*,QUrl)),
            this, SLOT(checkFinished(QObject*)));
        // QQmlApplicationEngine also connects quit() to QCoreApplication::quit
        // and exit() to QCoreApplication::exit but if called before exec()
        // then QCoreApplication::quit or QCoreApplication::exit does nothing
        connect(e, SIGNAL(quit()),
            this, SLOT(quit()));
        connect(e, &QQmlEngine::exit,
            this, &LoadWatcher::exit);
    }

    bool earlyExit;
    int returnCode;

private:
    void contain(QObject *o, const QUrl &containPath);
    void checkForWindow(QObject *o);

    int expect;
    bool haveOne;

public Q_SLOTS:
    void checkFinished(QObject *o)
    {
        if (o) {
            checkForWindow(o);
            haveOne = true;
            if (conf && qae)
                for (PartialScene *ps : qAsConst(conf->completers))
                    if (o->inherits(ps->itemType().toUtf8().constData()))
                        contain(o, ps->container());
        }
        if (haveOne)
            return;

        if (! --expect) {
            printf("qml: Did not load any objects, exiting.\n");
            std::exit(2);//Different return code from qFatal
        }
    }

    void quit() {
        //Will be checked before calling exec()
        earlyExit = true;
        returnCode = 0;
    }
    void exit(int retCode) {
        earlyExit = true;
        returnCode = retCode;
    }

#if defined(QT_GUI_LIB) && !defined(QT_NO_OPENGL)
    void onOpenGlContextCreated(QOpenGLContext *context);
#endif
};

void LoadWatcher::contain(QObject *o, const QUrl &containPath)
{
    QQmlComponent c(qae, containPath);
    QObject *o2 = c.create();
    if (!o2)
        return;
    checkForWindow(o2);
    bool success = false;
    int idx;
    if ((idx = o2->metaObject()->indexOfProperty("containedObject")) != -1)
        success = o2->metaObject()->property(idx).write(o2, QVariant::fromValue<QObject*>(o));
    if (!success)
        o->setParent(o2); //Set QObject parent, and assume container will react as needed
}

void LoadWatcher::checkForWindow(QObject *o)
{
#if defined(QT_GUI_LIB) && !defined(QT_NO_OPENGL)
    if (verboseMode && o->isWindowType() && o->inherits("QQuickWindow")) {
        connect(o, SIGNAL(openglContextCreated(QOpenGLContext*)),
                this, SLOT(onOpenGlContextCreated(QOpenGLContext*)));
    }
#else
    Q_UNUSED(o)
#endif // QT_GUI_LIB && !QT_NO_OPENGL
}

#if defined(QT_GUI_LIB) && !defined(QT_NO_OPENGL)
void LoadWatcher::onOpenGlContextCreated(QOpenGLContext *context)
{
    context->makeCurrent(qobject_cast<QWindow *>(sender()));
    QOpenGLFunctions functions(context);
    QByteArray output = "Vendor  : ";
    output += reinterpret_cast<const char *>(functions.glGetString(GL_VENDOR));
    output += "\nRenderer: ";
    output += reinterpret_cast<const char *>(functions.glGetString(GL_RENDERER));
    output += "\nVersion : ";
    output += reinterpret_cast<const char *>(functions.glGetString(GL_VERSION));
    output += "\nLanguage: ";
    output += reinterpret_cast<const char *>(functions.glGetString(GL_SHADING_LANGUAGE_VERSION));
    puts(output.constData());
    context->doneCurrent();
}
#endif // QT_GUI_LIB && !QT_NO_OPENGL

void quietMessageHandler(QtMsgType type, const QMessageLogContext &ctxt, const QString &msg)
{
    Q_UNUSED(ctxt);
    Q_UNUSED(msg);
    //Doesn't print anything
    switch (type) {
    case QtFatalMsg:
        exit(-1);
    case QtCriticalMsg:
    case QtDebugMsg:
    case QtWarningMsg:
    default:
        ;
    }
}


// ### Should command line arguments have translations? Qt creator doesn't, so maybe it's not worth it.
enum QmlApplicationType {
    QmlApplicationTypeUnknown
    , QmlApplicationTypeCore
#ifdef QT_GUI_LIB
    , QmlApplicationTypeGui
#ifdef QT_WIDGETS_LIB
    , QmlApplicationTypeWidget
#endif // QT_WIDGETS_LIB
#endif // QT_GUI_LIB
};

#ifndef QT_GUI_LIB
QmlApplicationType applicationType = QmlApplicationTypeCore;
#else
QmlApplicationType applicationType = QmlApplicationTypeGui;
#endif // QT_GUI_LIB
bool quietMode = false;
void printVersion()
{
    printf("qml binary version ");
    printf(VERSION_STR);
    printf("\nbuilt with Qt version ");
    printf(QT_VERSION_STR);
    printf("\n");
    exit(0);
}

void printUsage()
{
    printf("Usage: qml [options] [files] [-- args]\n");
    printf("\n");
    printf("Any unknown argument before '--' will be treated as a QML file to be loaded.\n");
    printf("Any number of QML files can be loaded. They will share the same engine.\n");
    printf("'gui' application type is only available if the QtGui module is available.\n");
    printf("'widget' application type is only available if the QtWidgets module is available.\n");
    printf("\n");
    printf("General Options:\n");
    printf("\t-h, -help..................... Print this usage information and exit.\n");
    printf("\t-v, -version.................. Print the version information and exit.\n");
#ifdef QT_GUI_LIB
#ifndef QT_WIDGETS_LIB
    printf("\t-apptype [core|gui] .......... Select which application class to use. Default is gui.\n");
#else
    printf("\t-apptype [core|gui|widget] ... Select which application class to use. Default is gui.\n");
#endif // QT_WIDGETS_LIB
#endif // QT_GUI_LIB
    printf("\t-quiet ....................... Suppress all output.\n");
    printf("\t-I [path] .................... Prepend the given path to the import paths.\n");
    printf("\t-f [file] .................... Load the given file as a QML file.\n");
    printf("\t-config [file] ............... Load the given file as the configuration file.\n");
    printf("\t-- ........................... Arguments after this one are ignored by the launcher, but may be used within the QML application.\n");
    printf("\tGL options:\n");
    printf("\t-desktop.......................Force use of desktop GL (AA_UseDesktopOpenGL)\n");
    printf("\t-gles..........................Force use of GLES (AA_UseOpenGLES)\n");
    printf("\t-software......................Force use of software rendering (AA_UseOpenGLES)\n");
    printf("\t-scaling.......................Enable High DPI scaling (AA_EnableHighDpiScaling)\n");
    printf("\t-no-scaling....................Disable High DPI scaling (AA_DisableHighDpiScaling)\n");
    printf("\tDebugging options:\n");
    printf("\t-verbose ..................... Print information about what qml is doing, like specific file urls being loaded.\n");
    printf("\t-translation [file] .......... Load the given file as the translations file.\n");
    printf("\t-dummy-data [directory] ...... Load QML files from the given directory as context properties.\n");
    printf("\t-slow-animations ............. Run all animations in slow motion.\n");
    printf("\t-fixed-animations ............ Run animations off animation tick rather than wall time.\n");
    exit(0);
}

void noFilesGiven()
{
    if (!quietMode)
        printf("qml: No files specified. Terminating.\n");
    exit(1);
}

//Called before application initialization, removes arguments it uses
void getAppFlags(int &argc, char **argv)
{
#ifdef QT_GUI_LIB
    for (int i=0; i<argc; i++) {
        if (!strcmp(argv[i], "-apptype")) { // Must be done before application, as it selects application
            applicationType = QmlApplicationTypeUnknown;
            if (i+1 < argc) {
                if (!strcmp(argv[i+1], "core"))
                    applicationType = QmlApplicationTypeCore;
                else if (!strcmp(argv[i+1], "gui"))
                    applicationType = QmlApplicationTypeGui;
#ifdef QT_WIDGETS_LIB
                else if (!strcmp(argv[i+1], "widget"))
                    applicationType = QmlApplicationTypeWidget;
#endif // QT_WIDGETS_LIB
            }

            if (applicationType == QmlApplicationTypeUnknown) {
#ifndef QT_WIDGETS_LIB
                printf("-apptype must be followed by one of the following: core gui\n");
#else
                printf("-apptype must be followed by one of the following: core gui widget\n");
#endif // QT_WIDGETS_LIB
                printUsage();
            }
            for (int j=i; j<argc-2; j++)
                argv[j] = argv[j+2];
            argc -= 2;
        }
    }
#endif // QT_GUI_LIB
}

bool getFileSansBangLine(const QString &path, QByteArray &output)
{
    QFile f(path);
    if (!f.open(QFile::ReadOnly | QFile::Text))
        return false;
    output = f.readAll();
    if (output.startsWith("#!")) {//Remove first line in this case (except \n, to avoid disturbing line count)
        output.remove(0, output.indexOf('\n'));
        return true;
    }
    return false;
}

static void loadDummyDataFiles(QQmlEngine &engine, const QString& directory)
{
    QDir dir(directory+"/dummydata", "*.qml");
    QStringList list = dir.entryList();
    for (int i = 0; i < list.size(); ++i) {
        QString qml = list.at(i);
        QQmlComponent comp(&engine, dir.filePath(qml));
        QObject *dummyData = comp.create();

        if (comp.isError()) {
            const QList<QQmlError> errors = comp.errors();
            for (const QQmlError &error : errors)
                qWarning() << error;
        }

        if (dummyData && !quietMode) {
            printf("qml: Loaded dummy data: %s\n",  qPrintable(dir.filePath(qml)));
            qml.truncate(qml.length()-4);
            engine.rootContext()->setContextProperty(qml, dummyData);
            dummyData->setParent(&engine);
        }
    }
}

int main(int argc, char *argv[])
{
    getAppFlags(argc, argv);
    QCoreApplication *app = 0;
    switch (applicationType) {
    case QmlApplicationTypeCore:
        app = new QCoreApplication(argc, argv);
        break;
#ifdef QT_GUI_LIB
    case QmlApplicationTypeGui:
        app = new LoaderApplication(argc, argv);
        break;
#ifdef QT_WIDGETS_LIB
    case QmlApplicationTypeWidget:
        app = new QApplication(argc, argv);
        break;
#endif // QT_WIDGETS_LIB
#endif // QT_GUI_LIB
    default:
        Q_ASSERT_X(false, Q_FUNC_INFO, "impossible case");
        break;
    }

    app->setApplicationName("Qml Runtime");
    app->setOrganizationName("QtProject");
    app->setOrganizationDomain("qt-project.org");
    QCoreApplication::setApplicationVersion(QLatin1String(QT_VERSION_STR));

    qmlRegisterType<Config>("QmlRuntime.Config", 1, 0, "Configuration");
    qmlRegisterType<PartialScene>("QmlRuntime.Config", 1, 0, "PartialScene");
    QQmlApplicationEngine e;
    QStringList files;
    QString confFile;
    QString translationFile;
    QString dummyDir;

    //Handle main arguments
    QStringList argList = app->arguments();
    for (int i = 1; i < argList.count(); i++) {
        const QString &arg = argList[i];
        if (arg == QLatin1String("-quiet"))
            quietMode = true;
        else if (arg == QLatin1String("-v") || arg == QLatin1String("-version"))
            printVersion();
        else if (arg == QLatin1String("-h") || arg == QLatin1String("-help"))
            printUsage();
        else if (arg == QLatin1String("--"))
            break;
        else if (arg == QLatin1String("-verbose"))
            verboseMode = true;
        else if (arg == QLatin1String("-slow-animations"))
            QUnifiedTimer::instance()->setSlowModeEnabled(true);
        else if (arg == QLatin1String("-fixed-animations"))
            QUnifiedTimer::instance()->setConsistentTiming(true);
        else if (arg == QLatin1String("-I")) {
            if (i+1 == argList.count())
                continue;//Invalid usage, but just ignore it
            e.addImportPath(argList[i+1]);
            i++;
        } else if (arg == QLatin1String("-f")) {
            if (i+1 == argList.count())
                continue;//Invalid usage, but just ignore it
            files << argList[i+1];
            i++;
        } else if (arg == QLatin1String("-config")){
            if (i+1 == argList.count())
                continue;//Invalid usage, but just ignore it
            confFile = argList[i+1];
            i++;
        } else if (arg == QLatin1String("-translation")){
            if (i+1 == argList.count())
                continue;//Invalid usage, but just ignore it
            translationFile = argList[i+1];
            i++;
        } else if (arg == QLatin1String("-dummy-data")){
            if (i+1 == argList.count())
                continue;//Invalid usage, but just ignore it
            dummyDir = argList[i+1];
            i++;
        } else if (arg == QLatin1String("-gles")) {
            QCoreApplication::setAttribute(Qt::AA_UseOpenGLES);
        } else if (arg == QLatin1String("-software")) {
            QCoreApplication::setAttribute(Qt::AA_UseSoftwareOpenGL);
        } else if (arg == QLatin1String("-desktop")) {
            QCoreApplication::setAttribute(Qt::AA_UseDesktopOpenGL);
        } else if (arg == QLatin1String("-scaling")) {
            QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
        } else if (arg == QLatin1String("-no-scaling")) {
            QCoreApplication::setAttribute(Qt::AA_DisableHighDpiScaling);
        } else {
            files << arg;
        }
    }

    if (quietMode && verboseMode)
        verboseMode = false;

#ifndef QT_NO_TRANSLATION
    //qt_ translations loaded by QQmlApplicationEngine
    QString sysLocale = QLocale::system().name();

    if (!translationFile.isEmpty()) { //Note: installed before QQmlApplicationEngine's automatic translation loading
        QTranslator translator;

        if (translator.load(translationFile)) {
            app->installTranslator(&translator);
            if (verboseMode)
                printf("qml: Loaded translation file %s\n", qPrintable(QDir::toNativeSeparators(translationFile)));
        } else {
            if (!quietMode)
                printf("qml: Could not load the translation file %s\n", qPrintable(QDir::toNativeSeparators(translationFile)));
        }
    }
#else
    if (!translationFile.isEmpty() && !quietMode)
        printf("qml: Translation file specified, but Qt built without translation support.\n");
#endif

    if (quietMode)
        qInstallMessageHandler(quietMessageHandler);

    if (files.count() <= 0) {
#if defined(Q_OS_MAC)
        if (applicationType == QmlApplicationTypeGui)
            exitTimerId = static_cast<LoaderApplication *>(app)->startTimer(FILE_OPEN_EVENT_WAIT_TIME);
        else
#endif
        noFilesGiven();
    }

    qae = &e;
    loadConf(confFile, !verboseMode);

    //Load files
    QScopedPointer<LoadWatcher> lw(new LoadWatcher(&e, files.count()));

    // Load dummy data before loading QML-files
    if (!dummyDir.isEmpty() && QFileInfo (dummyDir).isDir())
        loadDummyDataFiles(e, dummyDir);

    for (const QString &path : qAsConst(files)) {
        //QUrl::fromUserInput doesn't treat no scheme as relative file paths
#ifndef QT_NO_REGULAREXPRESSION
        QRegularExpression urlRe("[[:word:]]+://.*");
        if (urlRe.match(path).hasMatch()) { //Treat as a URL
            QUrl url = QUrl::fromUserInput(path);
            if (verboseMode)
                printf("qml: loading %s\n",
                        qPrintable(url.isLocalFile()
                        ? QDir::toNativeSeparators(url.toLocalFile())
                        : url.toString()));
            e.load(url);
        } else
#endif
        { //Local file path
            if (verboseMode)
                printf("qml: loading %s\n", qPrintable(QDir::toNativeSeparators(path)));

            QByteArray strippedFile;
            if (getFileSansBangLine(path, strippedFile))
                e.loadData(strippedFile, e.baseUrl().resolved(QUrl::fromLocalFile(path))); //QQmlComponent won't resolve it for us, it doesn't know it's a valid file if we loadData
            else //Errors or no bang line
                e.load(path);
        }
    }

    if (lw->earlyExit)
        return lw->returnCode;

    return app->exec();
}

#include "main.moc"

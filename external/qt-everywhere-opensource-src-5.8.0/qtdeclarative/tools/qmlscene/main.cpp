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

#include <QtCore/qabstractanimation.h>
#include <QtCore/qdir.h>
#include <QtCore/qmath.h>
#include <QtCore/qdatetime.h>
#include <QtCore/qpointer.h>
#include <QtCore/qscopedpointer.h>
#include <QtCore/qtextstream.h>

#include <QtGui/QGuiApplication>
#include <QtGui/QOpenGLFunctions>

#include <QtQml/qqml.h>
#include <QtQml/qqmlengine.h>
#include <QtQml/qqmlcomponent.h>
#include <QtQml/qqmlcontext.h>

#include <QtQuick/qquickitem.h>
#include <QtQuick/qquickview.h>

#include <private/qabstractanimation_p.h>
#include <private/qopenglcontext_p.h>

#ifdef QT_WIDGETS_LIB
#include <QtWidgets/QApplication>
#include <QtWidgets/QFileDialog>
#endif

#include <QtCore/QTranslator>
#include <QtCore/QLibraryInfo>

#ifdef QML_RUNTIME_TESTING
class RenderStatistics
{
public:
    static void updateStats();
    static void printTotalStats();
private:
    static QVector<qreal> timePerFrame;
    static QVector<int> timesPerFrames;
};

QVector<qreal> RenderStatistics::timePerFrame;
QVector<int> RenderStatistics::timesPerFrames;

void RenderStatistics::updateStats()
{
    static QTime time;
    static int frames;
    static int lastTime;

    if (frames == 0) {
        time.start();
    } else {
        int elapsed = time.elapsed();
        timesPerFrames.append(elapsed - lastTime);
        lastTime = elapsed;

        if (elapsed > 5000) {
            qreal avgtime = elapsed / (qreal) frames;
            qreal var = 0;
            for (int i = 0; i < timesPerFrames.size(); ++i) {
                qreal diff = timesPerFrames.at(i) - avgtime;
                var += diff * diff;
            }
            var /= timesPerFrames.size();

            printf("Average time per frame: %f ms (%i fps), std.dev: %f ms\n", avgtime, qRound(1000. / avgtime), qSqrt(var));

            timePerFrame.append(avgtime);
            timesPerFrames.clear();
            time.start();
            lastTime = 0;
            frames = 0;
        }
    }
    ++frames;
}

void RenderStatistics::printTotalStats()
{
    int count = timePerFrame.count();
    if (count == 0)
        return;

    qreal minTime = 0;
    qreal maxTime = 0;
    qreal avg = 0;
    for (int i = 0; i < count; ++i) {
        minTime = minTime == 0 ? timePerFrame.at(i) : qMin(minTime, timePerFrame.at(i));
        maxTime = qMax(maxTime, timePerFrame.at(i));
        avg += timePerFrame.at(i);
    }
    avg /= count;

    puts(" ");
    puts("----- Statistics -----");
    printf("Average time per frame: %f ms (%i fps)\n", avg, qRound(1000. / avg));
    printf("Best time per frame: %f ms (%i fps)\n", minTime, int(1000 / minTime));
    printf("Worst time per frame: %f ms (%i fps)\n", maxTime, int(1000 / maxTime));
    puts("----------------------");
    puts(" ");
}
#endif

struct Options
{
    Options()
        : originalQml(false)
        , originalQmlRaster(false)
        , maximized(false)
        , fullscreen(false)
        , transparent(false)
        , clip(false)
        , versionDetection(true)
        , slowAnimations(false)
        , quitImmediately(false)
        , resizeViewToRootItem(false)
        , multisample(false)
        , verbose(false)
    {
        // QtWebEngine needs a shared context in order for the GPU thread to
        // upload textures.
        applicationAttributes.append(Qt::AA_ShareOpenGLContexts);
    }

    QUrl url;
    bool originalQml;
    bool originalQmlRaster;
    bool maximized;
    bool fullscreen;
    bool transparent;
    bool clip;
    bool versionDetection;
    bool slowAnimations;
    bool quitImmediately;
    bool resizeViewToRootItem;
    bool multisample;
    bool verbose;
    QVector<Qt::ApplicationAttribute> applicationAttributes;
    QString translationFile;
};

#if defined(QMLSCENE_BUNDLE)
QFileInfoList findQmlFiles(const QString &dirName)
{
    QDir dir(dirName);

    QFileInfoList ret;
    if (dir.exists()) {
        const QFileInfoList fileInfos = dir.entryInfoList(QStringList() << "*.qml",
                                                          QDir::Files | QDir::AllDirs | QDir::NoDotAndDotDot);

        for (const QFileInfo &fileInfo : fileInfos) {
            if (fileInfo.isDir())
                ret += findQmlFiles(fileInfo.filePath());
            else if (fileInfo.fileName().length() > 0 && fileInfo.fileName().at(0).isLower())
                ret.append(fileInfo);
        }
    }

    return ret;
}

static int displayOptionsDialog(Options *options)
{
    QDialog dialog;

    QFormLayout *layout = new QFormLayout(&dialog);

    QComboBox *qmlFileComboBox = new QComboBox(&dialog);
    const QFileInfoList fileInfos = findQmlFiles(":/bundle") + findQmlFiles("./qmlscene-resources");

    for (const QFileInfo &fileInfo : fileInfos)
        qmlFileComboBox->addItem(fileInfo.dir().dirName() + QLatin1Char('/') + fileInfo.fileName(), QVariant::fromValue(fileInfo));

    QCheckBox *originalCheckBox = new QCheckBox(&dialog);
    originalCheckBox->setText("Use original QML viewer");
    originalCheckBox->setChecked(options->originalQml);

    QCheckBox *fullscreenCheckBox = new QCheckBox(&dialog);
    fullscreenCheckBox->setText("Start fullscreen");
    fullscreenCheckBox->setChecked(options->fullscreen);

    QCheckBox *maximizedCheckBox = new QCheckBox(&dialog);
    maximizedCheckBox->setText("Start maximized");
    maximizedCheckBox->setChecked(options->maximized);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                                       Qt::Horizontal,
                                                       &dialog);
    QObject::connect(buttonBox, SIGNAL(accepted()), &dialog, SLOT(accept()));
    QObject::connect(buttonBox, SIGNAL(rejected()), &dialog, SLOT(reject()));

    layout->addRow("Qml file:", qmlFileComboBox);
    layout->addWidget(originalCheckBox);
    layout->addWidget(maximizedCheckBox);
    layout->addWidget(fullscreenCheckBox);
    layout->addWidget(buttonBox);

    int result = dialog.exec();
    if (result == QDialog::Accepted) {
        QVariant variant = qmlFileComboBox->itemData(qmlFileComboBox->currentIndex());
        QFileInfo fileInfo = variant.value<QFileInfo>();

        if (fileInfo.canonicalFilePath().startsWith(QLatin1Char(':')))
            options->file = QUrl("qrc" + fileInfo.canonicalFilePath());
        else
            options->file = QUrl::fromLocalFile(fileInfo.canonicalFilePath());
        options->originalQml = originalCheckBox->isChecked();
        options->maximized = maximizedCheckBox->isChecked();
        options->fullscreen = fullscreenCheckBox->isChecked();
    }
    return result;
}
#endif

static bool checkVersion(const QUrl &url)
{
    if (!qgetenv("QMLSCENE_IMPORT_NAME").isEmpty())
        fprintf(stderr, "QMLSCENE_IMPORT_NAME is no longer supported.\n");

    if (!url.isLocalFile())
        return true;

    const QString fileName = url.toLocalFile();
    QFile f(fileName);
    if (!f.open(QFile::ReadOnly | QFile::Text)) {
        fprintf(stderr, "qmlscene: failed to check version of file '%s', could not open...\n",
                 qPrintable(fileName));
        return false;
    }

    QRegExp quick1("^\\s*import +QtQuick +1\\.\\w*");
    QRegExp qt47("^\\s*import +Qt +4\\.7");

    QTextStream stream(&f);
    bool codeFound= false;
    while (!codeFound) {
        QString line = stream.readLine();
        if (line.contains(QLatin1Char('{'))) {
            codeFound = true;
        } else {
            QString import;
            if (quick1.indexIn(line) >= 0)
                import = quick1.cap(0).trimmed();
            else if (qt47.indexIn(line) >= 0)
                import = qt47.cap(0).trimmed();

            if (!import.isNull()) {
                fprintf(stderr, "qmlscene: '%s' is no longer supported.\n"
                         "Use qmlviewer to load file '%s'.\n",
                         qPrintable(import),
                         qPrintable(fileName));
                return false;
            }
        }
    }

    return true;
}

static void displayFileDialog(Options *options)
{
#if defined(QT_WIDGETS_LIB) && !defined(QT_NO_FILEDIALOG)
    QString fileName = QFileDialog::getOpenFileName(0, "Open QML file", QString(), "QML Files (*.qml)");
    if (!fileName.isEmpty()) {
        QFileInfo fi(fileName);
        options->url = QUrl::fromLocalFile(fi.canonicalFilePath());
    }
#else
    Q_UNUSED(options);
    puts("No filename specified...");
#endif
}

#ifndef QT_NO_TRANSLATION
static void loadTranslationFile(QTranslator &translator, const QString& directory)
{
    translator.load(QLatin1String("qml_" )+QLocale::system().name(), directory + QLatin1String("/i18n"));
    QCoreApplication::installTranslator(&translator);
}
#endif

static void loadDummyDataFiles(QQmlEngine &engine, const QString& directory)
{
    QDir dir(directory+"/dummydata", "*.qml");
    QStringList list = dir.entryList();
    for (int i = 0; i < list.size(); ++i) {
        QString qml = list.at(i);
        QQmlComponent comp(&engine, dir.filePath(qml));
        QObject *dummyData = comp.create();

        if(comp.isError()) {
            const QList<QQmlError> errors = comp.errors();
            for (const QQmlError &error : errors)
                fprintf(stderr, "%s\n", qPrintable(error.toString()));
        }

        if (dummyData) {
            fprintf(stderr, "Loaded dummy data: %s\n", qPrintable(dir.filePath(qml)));
            qml.truncate(qml.length()-4);
            engine.rootContext()->setContextProperty(qml, dummyData);
            dummyData->setParent(&engine);
        }
    }
}

static void usage()
{
    puts("Usage: qmlscene [options] <filename>");
    puts(" ");
    puts(" Options:");
    puts("  --maximized ...................... Run maximized");
    puts("  --fullscreen ..................... Run fullscreen");
    puts("  --transparent .................... Make the window transparent");
    puts("  --multisample .................... Enable multisampling (OpenGL anti-aliasing)");
    puts("  --no-version-detection ........... Do not try to detect the version of the .qml file");
    puts("  --slow-animations ................ Run all animations in slow motion");
    puts("  --resize-to-root ................. Resize the window to the size of the root item");
    puts("  --quit ........................... Quit immediately after starting");
    puts("  --disable-context-sharing ........ Disable the use of a shared GL context for QtQuick Windows\n"
         "                            .........(remove AA_ShareOpenGLContexts)");
    puts("  --desktop..........................Force use of desktop GL (AA_UseDesktopOpenGL)");
    puts("  --gles.............................Force use of GLES (AA_UseOpenGLES)");
    puts("  --software.........................Force use of software rendering (AA_UseOpenGLES)");
    puts("  --scaling..........................Enable High DPI scaling (AA_EnableHighDpiScaling)");
    puts("  --no-scaling.......................Disable High DPI scaling (AA_DisableHighDpiScaling)");
    puts("  --verbose..........................Print version and graphical diagnostics for the run-time");
    puts("  -I <path> ........................ Add <path> to the list of import paths");
    puts("  -P <path> ........................ Add <path> to the list of plugin paths");
    puts("  -translation <translationfile> ... Set the language to run in");

    puts(" ");
    exit(1);
}
#ifndef QT_NO_OPENGL
// Listen on GL context creation of the QQuickWindow in order to print diagnostic output.
class DiagnosticGlContextCreationListener : public QObject {
    Q_OBJECT
public:
    explicit DiagnosticGlContextCreationListener(QQuickWindow *window) : QObject(window)
    {
        connect(window, &QQuickWindow::openglContextCreated,
                this, &DiagnosticGlContextCreationListener::onOpenGlContextCreated);
    }

private slots:
    void onOpenGlContextCreated(QOpenGLContext *context)
    {
        context->makeCurrent(qobject_cast<QQuickWindow *>(parent()));
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
        deleteLater();
    }

};
#endif

static void setWindowTitle(bool verbose, const QObject *topLevel, QWindow *window)
{
    const QString oldTitle = window->title();
    QString newTitle = oldTitle;
    if (newTitle.isEmpty()) {
        newTitle = QLatin1String("qmlscene");
        if (!qobject_cast<const QWindow *>(topLevel) && !topLevel->objectName().isEmpty())
            newTitle += QLatin1String(": ") + topLevel->objectName();
    }
    if (verbose) {
        newTitle += QLatin1String(" [Qt ") + QLatin1String(QT_VERSION_STR) + QLatin1Char(' ')
            + QGuiApplication::platformName() + QLatin1Char(' ');
#ifndef QT_NO_OPENGL
        newTitle += QOpenGLContext::openGLModuleType() == QOpenGLContext::LibGL
            ? QLatin1String("GL") : QLatin1String("GLES");
#endif
        newTitle += QLatin1Char(']');
    }
    if (oldTitle != newTitle)
        window->setTitle(newTitle);
}

static QUrl parseUrlArgument(const QString &arg)
{
    const QUrl url = QUrl::fromUserInput(arg, QDir::currentPath(), QUrl::AssumeLocalFile);
    if (!url.isValid()) {
        fprintf(stderr, "Invalid URL: \"%s\"\n", qPrintable(arg));
        return QUrl();
    }
    if (url.isLocalFile()) {
        const QFileInfo fi(url.toLocalFile());
        if (!fi.exists()) {
            fprintf(stderr, "\"%s\" does not exist.\n",
                    qPrintable(QDir::toNativeSeparators(fi.absoluteFilePath())));
            return QUrl();
        }
    }
    return url;
}

int main(int argc, char ** argv)
{
    Options options;

    QStringList imports;
    QStringList pluginPaths;

    // Parse arguments for application attributes to be applied before Q[Gui]Application creation.
    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (!qstrcmp(arg, "--disable-context-sharing"))
            options.applicationAttributes.removeAll(Qt::AA_ShareOpenGLContexts);
        else if (!qstrcmp(arg, "--gles"))
            options.applicationAttributes.append(Qt::AA_UseOpenGLES);
        else if (!qstrcmp(arg, "--software"))
            options.applicationAttributes.append(Qt::AA_UseSoftwareOpenGL);
        else if (!qstrcmp(arg, "--desktop"))
            options.applicationAttributes.append(Qt::AA_UseDesktopOpenGL);
        else if (!qstrcmp(arg, "--scaling"))
            options.applicationAttributes.append(Qt::AA_EnableHighDpiScaling);
        else if (!qstrcmp(arg, "--no-scaling"))
            options.applicationAttributes.append(Qt::AA_DisableHighDpiScaling);
    }

    for (Qt::ApplicationAttribute a : qAsConst(options.applicationAttributes))
        QCoreApplication::setAttribute(a);
#ifdef QT_WIDGETS_LIB
    QApplication app(argc, argv);
#else
    QGuiApplication app(argc, argv);
#endif
    app.setApplicationName("QtQmlViewer");
    app.setOrganizationName("QtProject");
    app.setOrganizationDomain("qt-project.org");
    QCoreApplication::setApplicationVersion(QLatin1String(QT_VERSION_STR));

    const QStringList arguments = QCoreApplication::arguments();
    for (int i = 1, size = arguments.size(); i < size; ++i) {
        if (!arguments.at(i).startsWith(QLatin1Char('-'))) {
            options.url = parseUrlArgument(arguments.at(i));
        } else {
            const QString lowerArgument = arguments.at(i).toLower();
            if (lowerArgument == QLatin1String("--maximized"))
                options.maximized = true;
            else if (lowerArgument == QLatin1String("--fullscreen"))
                options.fullscreen = true;
            else if (lowerArgument == QLatin1String("--transparent"))
                options.transparent = true;
            else if (lowerArgument == QLatin1String("--clip"))
                options.clip = true;
            else if (lowerArgument == QLatin1String("--no-version-detection"))
                options.versionDetection = false;
            else if (lowerArgument == QLatin1String("--slow-animations"))
                options.slowAnimations = true;
            else if (lowerArgument == QLatin1String("--quit"))
                options.quitImmediately = true;
           else if (lowerArgument == QLatin1String("-translation"))
                options.translationFile = QLatin1String(argv[++i]);
            else if (lowerArgument == QLatin1String("--resize-to-root"))
                options.resizeViewToRootItem = true;
            else if (lowerArgument == QLatin1String("--multisample"))
                options.multisample = true;
            else if (lowerArgument == QLatin1String("--verbose"))
                options.verbose = true;
            else if (lowerArgument == QLatin1String("-i") && i + 1 < size)
                imports.append(arguments.at(++i));
            else if (lowerArgument == QLatin1String("-p") && i + 1 < size)
                pluginPaths.append(arguments.at(++i));
            else if (lowerArgument == QLatin1String("--help")
                     || lowerArgument == QLatin1String("-help")
                     || lowerArgument == QLatin1String("--h")
                     || lowerArgument == QLatin1String("-h"))
                usage();
        }
    }

#ifndef QT_NO_TRANSLATION
    QTranslator translator;
    QTranslator qtTranslator;
    QString sysLocale = QLocale::system().name();
    if (qtTranslator.load(QLatin1String("qt_") + sysLocale, QLibraryInfo::location(QLibraryInfo::TranslationsPath)))
        app.installTranslator(&qtTranslator);
    if (translator.load(QLatin1String("qmlscene_") + sysLocale, QLibraryInfo::location(QLibraryInfo::TranslationsPath)))
        app.installTranslator(&translator);

    QTranslator qmlTranslator;
    if (!options.translationFile.isEmpty()) {
        if (qmlTranslator.load(options.translationFile)) {
            app.installTranslator(&qmlTranslator);
        } else {
            fprintf(stderr, "Could not load the translation file \"%s\"\n",
                    qPrintable(options.translationFile));
        }
    }
#endif

    QUnifiedTimer::instance()->setSlowModeEnabled(options.slowAnimations);

    if (options.url.isEmpty())
#if defined(QMLSCENE_BUNDLE)
        displayOptionsDialog(&options);
#else
        displayFileDialog(&options);
#endif

    int exitCode = 0;

    if (options.verbose)
        puts(QLibraryInfo::build());

    if (!options.url.isEmpty()) {
        if (!options.versionDetection || checkVersion(options.url)) {
#ifndef QT_NO_TRANSLATION
            QTranslator translator;
#endif

            // TODO: as soon as the engine construction completes, the debug service is
            // listening for connections.  But actually we aren't ready to debug anything.
            QQmlEngine engine;
            QPointer<QQmlComponent> component = new QQmlComponent(&engine);
            for (int i = 0; i < imports.size(); ++i)
                engine.addImportPath(imports.at(i));
            for (int i = 0; i < pluginPaths.size(); ++i)
                engine.addPluginPath(pluginPaths.at(i));
            if (options.url.isLocalFile()) {
                QFileInfo fi(options.url.toLocalFile());
#ifndef QT_NO_TRANSLATION
                loadTranslationFile(translator, fi.path());
#endif
                loadDummyDataFiles(engine, fi.path());
            }
            QObject::connect(&engine, SIGNAL(quit()), QCoreApplication::instance(), SLOT(quit()));
            QObject::connect(&engine, &QQmlEngine::exit, QCoreApplication::instance(), &QCoreApplication::exit);
            component->loadUrl(options.url);
            while (component->isLoading())
                QCoreApplication::processEvents();
            if ( !component->isReady() ) {
                fprintf(stderr, "%s\n", qPrintable(component->errorString()));
                return -1;
            }

            QObject *topLevel = component->create();
            if (!topLevel && component->isError()) {
                fprintf(stderr, "%s\n", qPrintable(component->errorString()));
                return -1;
            }
            QScopedPointer<QQuickWindow> window(qobject_cast<QQuickWindow *>(topLevel));
            if (window) {
                engine.setIncubationController(window->incubationController());
            } else {
                QQuickItem *contentItem = qobject_cast<QQuickItem *>(topLevel);
                if (contentItem) {
                    QQuickView* qxView = new QQuickView(&engine, NULL);
                    window.reset(qxView);
                    // Set window default properties; the qml can still override them
                    if (options.resizeViewToRootItem)
                        qxView->setResizeMode(QQuickView::SizeViewToRootObject);
                    else
                        qxView->setResizeMode(QQuickView::SizeRootObjectToView);
                    qxView->setContent(options.url, component, contentItem);
                }
            }

            if (window) {
                setWindowTitle(options.verbose, topLevel, window.data());
#ifndef QT_NO_OPENGL
                if (options.verbose)
                    new DiagnosticGlContextCreationListener(window.data());
#endif
                QSurfaceFormat surfaceFormat = window->requestedFormat();
                if (options.multisample)
                    surfaceFormat.setSamples(16);
                if (options.transparent) {
                    surfaceFormat.setAlphaBufferSize(8);
                    window->setClearBeforeRendering(true);
                    window->setColor(QColor(Qt::transparent));
                    window->setFlags(Qt::FramelessWindowHint);
                }
                window->setFormat(surfaceFormat);

                if (window->flags() == Qt::Window) // Fix window flags unless set by QML.
                    window->setFlags(Qt::Window | Qt::WindowSystemMenuHint | Qt::WindowTitleHint | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint | Qt::WindowFullscreenButtonHint);

                if (options.fullscreen)
                    window->showFullScreen();
                else if (options.maximized)
                    window->showMaximized();
                else if (!window->isVisible())
                    window->show();
            }

            if (options.quitImmediately)
                QMetaObject::invokeMethod(QCoreApplication::instance(), "quit", Qt::QueuedConnection);

            // Now would be a good time to inform the debug service to start listening.

            exitCode = app.exec();

#ifdef QML_RUNTIME_TESTING
            RenderStatistics::printTotalStats();
#endif
            // Ready to exit. Notice that the component might be owned by
            // QQuickView if one was created. That case is tracked by
            // QPointer, so it is safe to delete the component here.
            delete component;
        }
    }

    return exitCode;
}

#include "main.moc"

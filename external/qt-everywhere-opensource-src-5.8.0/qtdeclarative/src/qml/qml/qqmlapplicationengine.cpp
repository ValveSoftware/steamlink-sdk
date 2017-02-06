/****************************************************************************
**
** Copyright (C) 2016 Research In Motion.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtCore/QCoreApplication>
#include <QtCore/QTranslator>
#include <QQmlComponent>
#include "qqmlapplicationengine.h"
#include "qqmlapplicationengine_p.h"
#include "qqmlfileselector.h"

QT_BEGIN_NAMESPACE

QQmlApplicationEnginePrivate::QQmlApplicationEnginePrivate(QQmlEngine *e)
    : QQmlEnginePrivate(e)
{
}

QQmlApplicationEnginePrivate::~QQmlApplicationEnginePrivate()
{
}

void QQmlApplicationEnginePrivate::cleanUp()
{
    qDeleteAll(objects);
#ifndef QT_NO_TRANSLATION
    qDeleteAll(translators);
#endif
}

void QQmlApplicationEnginePrivate::init()
{
    Q_Q(QQmlApplicationEngine);
    q->connect(&statusMapper, SIGNAL(mapped(QObject*)),
            q, SLOT(_q_finishLoad(QObject*)));
    q->connect(q, SIGNAL(quit()), QCoreApplication::instance(), SLOT(quit()));
    q->connect(q, &QQmlApplicationEngine::exit, QCoreApplication::instance(), &QCoreApplication::exit);
#ifndef QT_NO_TRANSLATION
    QTranslator* qtTranslator = new QTranslator;
    if (qtTranslator->load(QLatin1String("qt_") + QLocale::system().name(), QLibraryInfo::location(QLibraryInfo::TranslationsPath)))
        QCoreApplication::installTranslator(qtTranslator);
    translators << qtTranslator;
#endif
    new QQmlFileSelector(q,q);
    QCoreApplication::instance()->setProperty("__qml_using_qqmlapplicationengine", QVariant(true));
}

void QQmlApplicationEnginePrivate::loadTranslations(const QUrl &rootFile)
{
#ifndef QT_NO_TRANSLATION
    if (rootFile.scheme() != QLatin1String("file") && rootFile.scheme() != QLatin1String("qrc"))
        return;

    QFileInfo fi(rootFile.toLocalFile());

    QTranslator *translator = new QTranslator;
    if (translator->load(QLatin1String("qml_") + QLocale::system().name(), fi.path() + QLatin1String("/i18n"))) {
        QCoreApplication::installTranslator(translator);
        translators << translator;
    } else {
        delete translator;
    }
#else
    Q_UNUSED(rootFile)
#endif
}

void QQmlApplicationEnginePrivate::startLoad(const QUrl &url, const QByteArray &data, bool dataFlag)
{
    Q_Q(QQmlApplicationEngine);

    loadTranslations(url); //Translations must be loaded before the QML file is
    QQmlComponent *c = new QQmlComponent(q, q);

    if (dataFlag)
        c->setData(data,url);
    else
        c->loadUrl(url);

    if (!c->isLoading()) {
        _q_finishLoad(c);
        return;
    }
    statusMapper.setMapping(c, c);
    q->connect(c, SIGNAL(statusChanged(QQmlComponent::Status)),
        &statusMapper, SLOT(map()));
}

void QQmlApplicationEnginePrivate::_q_finishLoad(QObject *o)
{
    Q_Q(QQmlApplicationEngine);
    QQmlComponent *c = qobject_cast<QQmlComponent *>(o);
    if (!c)
        return;
    switch (c->status()) {
    case QQmlComponent::Error:
        qWarning() << "QQmlApplicationEngine failed to load component";
        qWarning() << qPrintable(c->errorString());
        q->objectCreated(0, c->url());
        break;
    case QQmlComponent::Ready:
        objects << c->create();
        q->objectCreated(objects.constLast(), c->url());
        break;
    case QQmlComponent::Loading:
    case QQmlComponent::Null:
        return; //These cases just wait for the next status update
    }

    c->deleteLater();
}

/*!
  \class QQmlApplicationEngine
  \since 5.1
  \inmodule QtQml
  \brief QQmlApplicationEngine provides a convenient way to load an application from a single QML file.

  This class combines a QQmlEngine and QQmlComponent to provide a convenient way to load a single QML file. It also exposes some central application functionality to QML, which a C++/QML hybrid application would normally control from C++.

  It can be used like so:

  \code
  #include <QGuiApplication>
  #include <QQmlApplicationEngine>

  int main(int argc, char *argv[])
  {
      QGuiApplication app(argc, argv);
      QQmlApplicationEngine engine("main.qml");
      return app.exec();
  }
  \endcode

  Unlike QQuickView, QQmlApplicationEngine does not automatically create a root
  window. If you are using visual items from Qt Quick, you will need to place
  them inside of a \l [QML] {Window}.

  You can also use QCoreApplication with QQmlApplicationEngine, if you are not using any QML modules which require a QGuiApplication (such as \c QtQuick).

  List of configuration changes from a default QQmlEngine:

  \list
  \li Connecting Qt.quit() to QCoreApplication::quit()
  \li Automatically loads translation files from an i18n directory adjacent to the main QML file.
  \li Automatically sets an incubation controller if the scene contains a QQuickWindow.
  \li Automatically sets a \c QQmlFileSelector as the url interceptor, applying file selectors to all
  QML files and assets.
  \endlist

  The engine behavior can be further tweaked by using the inherited methods from QQmlEngine.

*/

/*!
  \fn QQmlApplicationEngine::objectCreated(QObject *object, const QUrl &url)

  This signal is emitted when an object finishes loading. If loading was successful, \a object contains a pointer to the loaded object.
  Otherwise the pointer is NULL. The \a url loaded is also provided, note that if a QString file path was initially passed to the
  QQmlApplicationEngine, this url will be the equivalent of QUrl::fromLocalFile(filePath).
*/

/*!
  Create a new QQmlApplicationEngine with the given \a parent. You will have to call load() later in
  order to load a QML file.
*/
QQmlApplicationEngine::QQmlApplicationEngine(QObject *parent)
: QQmlEngine(*(new QQmlApplicationEnginePrivate(this)), parent)
{
    Q_D(QQmlApplicationEngine);
    d->init();
    QJSEnginePrivate::addToDebugServer(this);
}

/*!
  Create a new QQmlApplicationEngine and loads the QML file at the given \a url.
  This is provided as a convenience,  and is the same as using the empty constructor and calling load afterwards.
*/
QQmlApplicationEngine::QQmlApplicationEngine(const QUrl &url, QObject *parent)
    : QQmlApplicationEngine(parent)
{
    load(url);
}

/*!
  Create a new QQmlApplicationEngine and loads the QML file at the given
  \a filePath, which must be a local file path. If a relative path is
  given then it will be interpreted as relative to the working directory of the
  application.

  This is provided as a convenience, and is the same as using the empty constructor and calling load afterwards.
*/
QQmlApplicationEngine::QQmlApplicationEngine(const QString &filePath, QObject *parent)
    : QQmlApplicationEngine(QUrl::fromLocalFile(filePath), parent)
{
}

/*!
  Destroys the QQmlApplicationEngine and all QML objects it loaded.
*/
QQmlApplicationEngine::~QQmlApplicationEngine()
{
    Q_D(QQmlApplicationEngine);
    QJSEnginePrivate::removeFromDebugServer(this);
    d->cleanUp();//Instantiated root objects must be deleted before the engine
}

/*!
  Loads the root QML file located at \a url. The object tree defined by the file
  is created immediately for local file urls. Remote urls are loaded asynchronously,
  listen to the objectCreated signal to determine when the object
  tree is ready.

  If an error occurs, error messages are printed with qWarning.
*/
void QQmlApplicationEngine::load(const QUrl &url)
{
    Q_D(QQmlApplicationEngine);
    d->startLoad(url);
}

/*!
  Loads the root QML file located at \a filePath. \a filePath must be a path to
  a local file. If \a filePath is a relative path, it is taken as relative to
  the application's working directory. The object tree defined by the file is
  instantiated immediately.

  If an error occurs, error messages are printed with qWarning.
*/
void QQmlApplicationEngine::load(const QString &filePath)
{
    Q_D(QQmlApplicationEngine);
    d->startLoad(QUrl::fromLocalFile(filePath));
}

/*!
  Loads the QML given in \a data. The object tree defined by \a data is
  instantiated immediately.

  If a \a url is specified it is used as the base url of the component. This affects
  relative paths within the data and error messages.

  If an error occurs, error messages are printed with qWarning.
*/
void QQmlApplicationEngine::loadData(const QByteArray &data, const QUrl &url)
{
    Q_D(QQmlApplicationEngine);
    d->startLoad(url, data, true);
}

/*!
  Returns a list of all the root objects instantiated by the
  QQmlApplicationEngine. This will only contain objects loaded via load() or a
  convenience constructor.
*/

QList<QObject *> QQmlApplicationEngine::rootObjects()
{
    Q_D(QQmlApplicationEngine);
    return d->objects;
}

QT_END_NAMESPACE

#include "moc_qqmlapplicationengine.cpp"

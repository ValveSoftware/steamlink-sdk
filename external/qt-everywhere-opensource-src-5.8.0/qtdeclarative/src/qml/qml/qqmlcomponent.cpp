/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#include "qqmlcomponent.h"
#include "qqmlcomponent_p.h"
#include "qqmlcomponentattached_p.h"

#include "qqmlcontext_p.h"
#include "qqmlengine_p.h"
#include "qqmlvme_p.h"
#include "qqml.h"
#include "qqmlengine.h"
#include "qqmlbinding_p.h"
#include <private/qqmldebugconnector_p.h>
#include <private/qqmldebugserviceinterfaces_p.h>
#include "qqmlincubator.h"
#include "qqmlincubator_p.h"
#include <private/qqmljavascriptexpression_p.h>

#include <private/qv8engine_p.h>

#include <private/qv4functionobject_p.h>
#include <private/qv4script_p.h>
#include <private/qv4scopedvalue_p.h>
#include <private/qv4objectiterator_p.h>
#include <private/qv4qobjectwrapper_p.h>

#include <QDir>
#include <QStack>
#include <QStringList>
#include <QThreadStorage>
#include <QtCore/qdebug.h>
#include <qqmlinfo.h>
#include "qqmlmemoryprofiler_p.h"

namespace {
    QThreadStorage<int> creationDepth;
}

QT_BEGIN_NAMESPACE

class QQmlComponentExtension : public QV8Engine::Deletable
{
public:
    QQmlComponentExtension(QV4::ExecutionEngine *v4);
    virtual ~QQmlComponentExtension();

    QV4::PersistentValue incubationProto;
};
V4_DEFINE_EXTENSION(QQmlComponentExtension, componentExtension);

/*!
    \class QQmlComponent
    \since 5.0
    \inmodule QtQml

    \brief The QQmlComponent class encapsulates a QML component definition

    Components are reusable, encapsulated QML types with well-defined interfaces.

    A QQmlComponent instance can be created from a QML file.
    For example, if there is a \c main.qml file like this:

    \qml
    import QtQuick 2.0

    Item {
        width: 200
        height: 200
    }
    \endqml

    The following code loads this QML file as a component, creates an instance of
    this component using create(), and then queries the \l Item's \l {Item::}{width}
    value:

    \code
    QQmlEngine *engine = new QQmlEngine;
    QQmlComponent component(engine, QUrl::fromLocalFile("main.qml"));

    QObject *myObject = component.create();
    QQuickItem *item = qobject_cast<QQuickItem*>(myObject);
    int width = item->width();  // width = 200
    \endcode

    To create instances of a component in code where a QQmlEngine instance is
    not available, you can use \l qmlContext() or \l qmlEngine(). For example,
    in the scenario below, child items are being created within a QQuickItem
    subclass:

    \code
    void MyCppItem::init()
    {
        QQmlEngine *engine = qmlEngine(this);
        // Or:
        // QQmlEngine *engine = qmlContext(this)->engine();
        QQmlComponent component(engine, QUrl::fromLocalFile("MyItem.qml"));
        QQuickItem *childItem = qobject_cast<QQuickItem*>(component.create());
        childItem->setParentItem(this);
    }
    \endcode

    Note that these functions will return \c null when called inside the
    constructor of a QObject subclass, as the instance will not yet have
    a context nor engine.

    \section2 Network Components

    If the URL passed to QQmlComponent is a network resource, or if the QML document references a
    network resource, the QQmlComponent has to fetch the network data before it is able to create
    objects. In this case, the QQmlComponent will have a \l {QQmlComponent::Loading}{Loading}
    \l {QQmlComponent::status()}{status}. An application will have to wait until the component
    is \l {QQmlComponent::Ready}{Ready} before calling \l {QQmlComponent::create()}.

    The following example shows how to load a QML file from a network resource. After creating
    the QQmlComponent, it tests whether the component is loading. If it is, it connects to the
    QQmlComponent::statusChanged() signal and otherwise calls the \c {continueLoading()} method
    directly. Note that QQmlComponent::isLoading() may be false for a network component if the
    component has been cached and is ready immediately.

    \code
    MyApplication::MyApplication()
    {
        // ...
        component = new QQmlComponent(engine, QUrl("http://www.example.com/main.qml"));
        if (component->isLoading())
            QObject::connect(component, SIGNAL(statusChanged(QQmlComponent::Status)),
                             this, SLOT(continueLoading()));
        else
            continueLoading();
    }

    void MyApplication::continueLoading()
    {
        if (component->isError()) {
            qWarning() << component->errors();
        } else {
            QObject *myObject = component->create();
        }
    }
    \endcode

    Note that the \l {Qt Quick 1} version is named QDeclarativeComponent.
*/

/*!
    \qmltype Component
    \instantiates QQmlComponent
    \ingroup qml-utility-elements
    \inqmlmodule QtQml
    \brief Encapsulates a QML component definition

    Components are reusable, encapsulated QML types with well-defined interfaces.

    Components are often defined by \l {{QML Documents}}{component files} -
    that is, \c .qml files. The \e Component type essentially allows QML components
    to be defined inline, within a \l {QML Documents}{QML document}, rather than as a separate QML file.
    This may be useful for reusing a small component within a QML file, or for defining
    a component that logically belongs with other QML components within a file.

    For example, here is a component that is used by multiple \l Loader objects.
    It contains a single item, a \l Rectangle:

    \snippet qml/component.qml 0

    Notice that while a \l Rectangle by itself would be automatically
    rendered and displayed, this is not the case for the above rectangle
    because it is defined inside a \c Component. The component encapsulates the
    QML types within, as if they were defined in a separate QML
    file, and is not loaded until requested (in this case, by the
    two \l Loader objects). Because Component is not derived from Item, you cannot
    anchor anything to it.

    Defining a \c Component is similar to defining a \l {QML Documents}{QML document}.
    A QML document has a single top-level item that defines the behavior and
    properties of that component, and cannot define properties or behavior outside
    of that top-level item. In the same way, a \c Component definition contains a single
    top level item (which in the above example is a \l Rectangle) and cannot define any
    data outside of this item, with the exception of an \e id (which in the above example
    is \e redSquare).

    The \c Component type is commonly used to provide graphical components
    for views. For example, the ListView::delegate property requires a \c Component
    to specify how each list item is to be displayed.

    \c Component objects can also be created dynamically using
    \l{QtQml::Qt::createComponent()}{Qt.createComponent()}.

    \section2 Creation Context

    The creation context of a Component corresponds to the context where the Component was declared.
    This context is used as the parent context (creating a \l{qtqml-documents-scope.html#component-instance-hierarchy}{context hierarchy})
    when the component is instantiated by an object such as a ListView or a Loader.

    In the following example, \c comp1 is created within the root context of MyItem.qml, and any objects
    instantiated from this component will have access to the ids and properties within that context,
    such as \c internalSettings.color. When \c comp1 is used as a ListView delegate in another context
    (as in main.qml below), it will continue to have access to the properties of its creation context
    (which would otherwise be private to external users).

    \table
    \row
    \li MyItem.qml
    \li \snippet qml/component/MyItem.qml 0
    \row
    \li main.qml
    \li \snippet qml/component/main.qml 0
    \endtable
*/

/*!
    \qmlattachedsignal Component::completed()

    Emitted after the object has been instantiated. This can be used to
    execute script code at startup, once the full QML environment has been
    established.

    The corresponding handler is \c onCompleted. It can be declared on
    any object. The order of running the \c onCompleted handlers is
    undefined.

    \qml
    Rectangle {
        Component.onCompleted: console.log("Completed Running!")
        Rectangle {
            Component.onCompleted: console.log("Nested Completed Running!")
        }
    }
    \endqml
*/

/*!
    \qmlattachedsignal Component::destruction()

    Emitted as the object begins destruction. This can be used to undo
    work done in response to the \l {completed}{completed()} signal, or other
    imperative code in your application.

    The corresponding handler is \c onDestruction. It can be declared on
    any object. The order of running the \c onDestruction handlers is
    undefined.

    \qml
    Rectangle {
        Component.onDestruction: console.log("Destruction Beginning!")
        Rectangle {
            Component.onDestruction: console.log("Nested Destruction Beginning!")
        }
    }
    \endqml

    \sa {Qt QML}
*/

/*!
    \enum QQmlComponent::Status

    Specifies the loading status of the QQmlComponent.

    \value Null This QQmlComponent has no data. Call loadUrl() or setData() to add QML content.
    \value Ready This QQmlComponent is ready and create() may be called.
    \value Loading This QQmlComponent is loading network data.
    \value Error An error has occurred. Call errors() to retrieve a list of \l {QQmlError}{errors}.
*/

/*!
    \enum QQmlComponent::CompilationMode

    Specifies whether the QQmlComponent should load the component immediately, or asynchonously.

    \value PreferSynchronous Prefer loading/compiling the component immediately, blocking the thread.
    This is not always possible; for example, remote URLs will always load asynchronously.
    \value Asynchronous Load/compile the component in a background thread.
*/

void QQmlComponentPrivate::typeDataReady(QQmlTypeData *)
{
    Q_Q(QQmlComponent);

    Q_ASSERT(typeData);

    fromTypeData(typeData);
    typeData = 0;
    progress = 1.0;

    emit q->statusChanged(q->status());
    emit q->progressChanged(progress);
}

void QQmlComponentPrivate::typeDataProgress(QQmlTypeData *, qreal p)
{
    Q_Q(QQmlComponent);

    progress = p;

    emit q->progressChanged(p);
}

void QQmlComponentPrivate::fromTypeData(QQmlTypeData *data)
{
    url = data->finalUrl();
    compilationUnit = data->compilationUnit();

    if (!compilationUnit) {
        Q_ASSERT(data->isError());
        state.errors = data->errors();
    }

    data->release();
}

void QQmlComponentPrivate::clear()
{
    if (typeData) {
        typeData->unregisterCallback(this);
        typeData->release();
        typeData = 0;
    }

    compilationUnit = nullptr;
}

/*!
    \internal
*/
QQmlComponent::QQmlComponent(QObject *parent)
    : QObject(*(new QQmlComponentPrivate), parent)
{
}

/*!
    Destruct the QQmlComponent.
*/
QQmlComponent::~QQmlComponent()
{
    Q_D(QQmlComponent);

    if (d->state.completePending) {
        qWarning("QQmlComponent: Component destroyed while completion pending");

        if (isError()) {
            qWarning() << "This may have been caused by one of the following errors:";
            foreach (const QQmlError &error, d->state.errors)
                qWarning().nospace().noquote() << QLatin1String("    ") << error;
        }

        d->completeCreate();
    }

    if (d->typeData) {
        d->typeData->unregisterCallback(d);
        d->typeData->release();
    }
}

/*!
    \qmlproperty enumeration Component::status
    This property holds the status of component loading. The status can be one of the
    following:
    \list
    \li Component.Null - no data is available for the component
    \li Component.Ready - the component has been loaded, and can be used to create instances.
    \li Component.Loading - the component is currently being loaded
    \li Component.Error - an error occurred while loading the component.
               Calling errorString() will provide a human-readable description of any errors.
    \endlist
 */

/*!
    \property QQmlComponent::status
    The component's current \l{QQmlComponent::Status} {status}.
 */
QQmlComponent::Status QQmlComponent::status() const
{
    Q_D(const QQmlComponent);

    if (d->typeData)
        return Loading;
    else if (!d->state.errors.isEmpty())
        return Error;
    else if (d->engine && d->compilationUnit)
        return Ready;
    else
        return Null;
}

/*!
    Returns true if status() == QQmlComponent::Null.
*/
bool QQmlComponent::isNull() const
{
    return status() == Null;
}

/*!
    Returns true if status() == QQmlComponent::Ready.
*/
bool QQmlComponent::isReady() const
{
    return status() == Ready;
}

/*!
    Returns true if status() == QQmlComponent::Error.
*/
bool QQmlComponent::isError() const
{
    return status() == Error;
}

/*!
    Returns true if status() == QQmlComponent::Loading.
*/
bool QQmlComponent::isLoading() const
{
    return status() == Loading;
}

/*!
    \qmlproperty real Component::progress
    The progress of loading the component, from 0.0 (nothing loaded)
    to 1.0 (finished).
*/

/*!
    \property QQmlComponent::progress
    The progress of loading the component, from 0.0 (nothing loaded)
    to 1.0 (finished).
*/
qreal QQmlComponent::progress() const
{
    Q_D(const QQmlComponent);
    return d->progress;
}

/*!
    \fn void QQmlComponent::progressChanged(qreal progress)

    Emitted whenever the component's loading progress changes. \a progress will be the
    current progress between 0.0 (nothing loaded) and 1.0 (finished).
*/

/*!
    \fn void QQmlComponent::statusChanged(QQmlComponent::Status status)

    Emitted whenever the component's status changes. \a status will be the
    new status.
*/

/*!
    Create a QQmlComponent with no data and give it the specified
    \a engine and \a parent. Set the data with setData().
*/
QQmlComponent::QQmlComponent(QQmlEngine *engine, QObject *parent)
    : QObject(*(new QQmlComponentPrivate), parent)
{
    Q_D(QQmlComponent);
    d->engine = engine;
}

/*!
    Create a QQmlComponent from the given \a url and give it the
    specified \a parent and \a engine.

    Ensure that the URL provided is full and correct, in particular, use
    \l QUrl::fromLocalFile() when loading a file from the local filesystem.

    \sa loadUrl()
*/
QQmlComponent::QQmlComponent(QQmlEngine *engine, const QUrl &url, QObject *parent)
    : QQmlComponent(engine, url, QQmlComponent::PreferSynchronous, parent)
{
}

/*!
    Create a QQmlComponent from the given \a url and give it the
    specified \a parent and \a engine. If \a mode is \l Asynchronous,
    the component will be loaded and compiled asynchronously.

    Ensure that the URL provided is full and correct, in particular, use
    \l QUrl::fromLocalFile() when loading a file from the local filesystem.

    \sa loadUrl()
*/
QQmlComponent::QQmlComponent(QQmlEngine *engine, const QUrl &url, CompilationMode mode,
                             QObject *parent)
    : QQmlComponent(engine, parent)
{
    Q_D(QQmlComponent);
    d->loadUrl(url, mode);
}

/*!
    Create a QQmlComponent from the given \a fileName and give it the specified
    \a parent and \a engine.

    \sa loadUrl()
*/
QQmlComponent::QQmlComponent(QQmlEngine *engine, const QString &fileName,
                             QObject *parent)
    : QQmlComponent(engine, fileName, QQmlComponent::PreferSynchronous, parent)
{
}

/*!
    Create a QQmlComponent from the given \a fileName and give it the specified
    \a parent and \a engine. If \a mode is \l Asynchronous,
    the component will be loaded and compiled asynchronously.

    \sa loadUrl()
*/
QQmlComponent::QQmlComponent(QQmlEngine *engine, const QString &fileName,
                             CompilationMode mode, QObject *parent)
    : QQmlComponent(engine, parent)
{
    Q_D(QQmlComponent);
    const QUrl url = QDir::isAbsolutePath(fileName) ? QUrl::fromLocalFile(fileName) : d->engine->baseUrl().resolved(QUrl(fileName));
    d->loadUrl(url, mode);
}

/*!
    \internal
*/
QQmlComponent::QQmlComponent(QQmlEngine *engine, QV4::CompiledData::CompilationUnit *compilationUnit, int start, QObject *parent)
    : QQmlComponent(engine, parent)
{
    Q_D(QQmlComponent);
    d->compilationUnit = compilationUnit;
    d->start = start;
    d->url = compilationUnit->url();
    d->progress = 1.0;
}

/*!
    Sets the QQmlComponent to use the given QML \a data. If \a url
    is provided, it is used to set the component name and to provide
    a base path for items resolved by this component.
*/
void QQmlComponent::setData(const QByteArray &data, const QUrl &url)
{
    Q_D(QQmlComponent);

    d->clear();

    d->url = url;

    QQmlTypeData *typeData = QQmlEnginePrivate::get(d->engine)->typeLoader.getType(data, url);

    if (typeData->isCompleteOrError()) {
        d->fromTypeData(typeData);
    } else {
        d->typeData = typeData;
        d->typeData->registerCallback(d);
    }

    d->progress = 1.0;
    emit statusChanged(status());
    emit progressChanged(d->progress);
}

/*!
Returns the QQmlContext the component was created in. This is only
valid for components created directly from QML.
*/
QQmlContext *QQmlComponent::creationContext() const
{
    Q_D(const QQmlComponent);
    if(d->creationContext)
        return d->creationContext->asQQmlContext();

    return qmlContext(this);
}

/*!
    Load the QQmlComponent from the provided \a url.

    Ensure that the URL provided is full and correct, in particular, use
    \l QUrl::fromLocalFile() when loading a file from the local filesystem.
*/
void QQmlComponent::loadUrl(const QUrl &url)
{
    Q_D(QQmlComponent);
    d->loadUrl(url);
}

/*!
    Load the QQmlComponent from the provided \a url.
    If \a mode is \l Asynchronous, the component will be loaded and compiled asynchronously.

    Ensure that the URL provided is full and correct, in particular, use
    \l QUrl::fromLocalFile() when loading a file from the local filesystem.
*/
void QQmlComponent::loadUrl(const QUrl &url, QQmlComponent::CompilationMode mode)
{
    Q_D(QQmlComponent);
    d->loadUrl(url, mode);
}

void QQmlComponentPrivate::loadUrl(const QUrl &newUrl, QQmlComponent::CompilationMode mode)
{
    Q_Q(QQmlComponent);
    clear();

    if ((newUrl.isRelative() && !newUrl.isEmpty())
    || newUrl.scheme() == QLatin1String("file")) // Workaround QTBUG-11929
        url = engine->baseUrl().resolved(newUrl);
    else
        url = newUrl;

    if (newUrl.isEmpty()) {
        QQmlError error;
        error.setDescription(QQmlComponent::tr("Invalid empty URL"));
        state.errors << error;
        return;
    }

    if (progress != 0.0) {
        progress = 0.0;
        emit q->progressChanged(progress);
    }

    QQmlTypeLoader::Mode loaderMode = (mode == QQmlComponent::Asynchronous)
            ? QQmlTypeLoader::Asynchronous
            : QQmlTypeLoader::PreferSynchronous;

    QQmlTypeData *data = QQmlEnginePrivate::get(engine)->typeLoader.getType(url, loaderMode);

    if (data->isCompleteOrError()) {
        fromTypeData(data);
        progress = 1.0;
    } else {
        typeData = data;
        typeData->registerCallback(this);
        progress = data->progress();
    }

    emit q->statusChanged(q->status());
    if (progress != 0.0)
        emit q->progressChanged(progress);
}

/*!
    Returns the list of errors that occurred during the last compile or create
    operation. An empty list is returned if isError() is not set.
*/
QList<QQmlError> QQmlComponent::errors() const
{
    Q_D(const QQmlComponent);
    if (isError())
        return d->state.errors;
    else
        return QList<QQmlError>();
}

/*!
    \qmlmethod string Component::errorString()

    Returns a human-readable description of any error.

    The string includes the file, location, and description of each error.
    If multiple errors are present, they are separated by a newline character.

    If no errors are present, an empty string is returned.
*/

/*!
    \internal
    errorString is only meant as a way to get the errors in script
*/
QString QQmlComponent::errorString() const
{
    Q_D(const QQmlComponent);
    QString ret;
    if(!isError())
        return ret;
    foreach(const QQmlError &e, d->state.errors) {
        ret += e.url().toString() + QLatin1Char(':') +
               QString::number(e.line()) + QLatin1Char(' ') +
               e.description() + QLatin1Char('\n');
    }
    return ret;
}

/*!
    \qmlproperty url Component::url
    The component URL. This is the URL that was used to construct the component.
*/

/*!
    \property QQmlComponent::url
    The component URL. This is the URL passed to either the constructor,
    or the loadUrl(), or setData() methods.
*/
QUrl QQmlComponent::url() const
{
    Q_D(const QQmlComponent);
    return d->url;
}

/*!
    \internal
*/
QQmlComponent::QQmlComponent(QQmlComponentPrivate &dd, QObject *parent)
    : QObject(dd, parent)
{
}

/*!
    Create an object instance from this component. Returns 0 if creation
    failed. \a context specifies the context within which to create the object
    instance.

    If \a context is 0 (the default), it will create the instance in the
    engine' s \l {QQmlEngine::rootContext()}{root context}.

    The ownership of the returned object instance is transferred to the caller.

    If the object being created from this component is a visual item, it must
    have a visual parent, which can be set by calling
    QQuickItem::setParentItem(). See \l {Concepts - Visual Parent in Qt Quick}
    for more details.

    \sa QQmlEngine::ObjectOwnership
*/
QObject *QQmlComponent::create(QQmlContext *context)
{
    Q_D(QQmlComponent);
    QML_MEMORY_SCOPE_URL(url());

    if (!context)
        context = d->engine->rootContext();

    QObject *rv = beginCreate(context);
    if (rv)
        completeCreate();
    return rv;
}

/*!
    This method provides advanced control over component instance creation.
    In general, programmers should use QQmlComponent::create() to create object
    instances.

    Create an object instance from this component. Returns 0 if creation
    failed. \a publicContext specifies the context within which to create the object
    instance.

    When QQmlComponent constructs an instance, it occurs in three steps:
    \list 1
    \li The object hierarchy is created, and constant values are assigned.
    \li Property bindings are evaluated for the first time.
    \li If applicable, QQmlParserStatus::componentComplete() is called on objects.
    \endlist
    QQmlComponent::beginCreate() differs from QQmlComponent::create() in that it
    only performs step 1. QQmlComponent::completeCreate() must be called to
    complete steps 2 and 3.

    This breaking point is sometimes useful when using attached properties to
    communicate information to an instantiated component, as it allows their
    initial values to be configured before property bindings take effect.

    The ownership of the returned object instance is transferred to the caller.

    \sa completeCreate(), QQmlEngine::ObjectOwnership
*/
QObject *QQmlComponent::beginCreate(QQmlContext *publicContext)
{
    Q_D(QQmlComponent);

    Q_ASSERT(publicContext);
    QQmlContextData *context = QQmlContextData::get(publicContext);

    return d->beginCreate(context);
}

QObject *
QQmlComponentPrivate::beginCreate(QQmlContextData *context)
{
    Q_Q(QQmlComponent);
    if (!context) {
        qWarning("QQmlComponent: Cannot create a component in a null context");
        return 0;
    }

    if (!context->isValid()) {
        qWarning("QQmlComponent: Cannot create a component in an invalid context");
        return 0;
    }

    if (context->engine != engine) {
        qWarning("QQmlComponent: Must create component in context from the same QQmlEngine");
        return 0;
    }

    if (state.completePending) {
        qWarning("QQmlComponent: Cannot create new component instance before completing the previous");
        return 0;
    }

    if (!q->isReady()) {
        qWarning("QQmlComponent: Component is not ready");
        return 0;
    }

    // Do not create infinite recursion in object creation
    static const int maxCreationDepth = 10;
    if (++creationDepth.localData() >= maxCreationDepth) {
        qWarning("QQmlComponent: Component creation is recursing - aborting");
        --creationDepth.localData();
        return 0;
    }
    Q_ASSERT(creationDepth.localData() >= 1);
    depthIncreased = true;

    QQmlEnginePrivate *enginePriv = QQmlEnginePrivate::get(engine);

    enginePriv->inProgressCreations++;
    state.errors.clear();
    state.completePending = true;

    enginePriv->referenceScarceResources();
    QObject *rv = 0;
    state.creator.reset(new QQmlObjectCreator(context, compilationUnit, creationContext));
    rv = state.creator->create(start);
    if (!rv)
        state.errors = state.creator->errors;
    enginePriv->dereferenceScarceResources();

    if (rv) {
        QQmlData *ddata = QQmlData::get(rv);
        Q_ASSERT(ddata);
        //top level objects should never get JS ownership.
        //if JS ownership is needed this needs to be explicitly undone (like in component.createObject())
        ddata->indestructible = true;
        ddata->explicitIndestructibleSet = true;
        ddata->rootObjectInCreation = false;
    } else {
        Q_ASSERT(creationDepth.localData() >= 1);
        --creationDepth.localData();
        depthIncreased = false;
    }

    if (rv) {
        if (QQmlEngineDebugService *service =
                QQmlDebugConnector::service<QQmlEngineDebugService>()) {
            if (!context->isInternal)
                context->asQQmlContextPrivate()->instances.append(rv);
            service->objectCreated(engine, rv);
        }
    }

    return rv;
}

void QQmlComponentPrivate::beginDeferred(QQmlEnginePrivate *enginePriv,
                                                 QObject *object, ConstructionState *state)
{
    enginePriv->inProgressCreations++;
    state->errors.clear();
    state->completePending = true;

    QQmlData *ddata = QQmlData::get(object);
    Q_ASSERT(ddata->deferredData);
    QQmlData::DeferredData *deferredData = ddata->deferredData;
    QQmlContextData *creationContext = 0;
    state->creator.reset(new QQmlObjectCreator(deferredData->context->parent, deferredData->compilationUnit, creationContext));
    if (!state->creator->populateDeferredProperties(object))
        state->errors << state->creator->errors;
}

void QQmlComponentPrivate::complete(QQmlEnginePrivate *enginePriv, ConstructionState *state)
{
    if (state->completePending) {
        QQmlInstantiationInterrupt interrupt;
        state->creator->finalize(interrupt);

        state->completePending = false;

        enginePriv->inProgressCreations--;

        if (0 == enginePriv->inProgressCreations) {
            while (enginePriv->erroredBindings) {
                enginePriv->warning(enginePriv->erroredBindings);
                enginePriv->erroredBindings->removeError();
            }
        }
    }
}

/*!
    This method provides advanced control over component instance creation.
    In general, programmers should use QQmlComponent::create() to create a
    component.

    This function completes the component creation begun with QQmlComponent::beginCreate()
    and must be called afterwards.

    \sa beginCreate()
*/
void QQmlComponent::completeCreate()
{
    Q_D(QQmlComponent);

    d->completeCreate();
}

void QQmlComponentPrivate::completeCreate()
{
    if (state.completePending) {
        QQmlEnginePrivate *ep = QQmlEnginePrivate::get(engine);
        complete(ep, &state);
    }

    if (depthIncreased) {
        Q_ASSERT(creationDepth.localData() >= 1);
        --creationDepth.localData();
        depthIncreased = false;
    }
}

QQmlComponentAttached::QQmlComponentAttached(QObject *parent)
: QObject(parent), prev(0), next(0)
{
}

QQmlComponentAttached::~QQmlComponentAttached()
{
    if (prev) *prev = next;
    if (next) next->prev = prev;
    prev = 0;
    next = 0;
}

/*!
    \internal
*/
QQmlComponentAttached *QQmlComponent::qmlAttachedProperties(QObject *obj)
{
    QQmlComponentAttached *a = new QQmlComponentAttached(obj);

    QQmlEngine *engine = qmlEngine(obj);
    if (!engine)
        return a;

    QQmlEnginePrivate *p = QQmlEnginePrivate::get(engine);
    if (p->activeObjectCreator) { // XXX should only be allowed during begin
        a->add(p->activeObjectCreator->componentAttachment());
    } else {
        QQmlData *d = QQmlData::get(obj);
        Q_ASSERT(d);
        Q_ASSERT(d->context);
        a->add(&d->context->componentAttached);
    }

    return a;
}

/*!
    Create an object instance from this component using the provided
    \a incubator. \a context specifies the context within which to create the object
    instance.

    If \a context is 0 (the default), it will create the instance in the
    engine's \l {QQmlEngine::rootContext()}{root context}.

    \a forContext specifies a context that this object creation depends upon.
    If the \a forContext is being created asynchronously, and the
    \l QQmlIncubator::IncubationMode is \l QQmlIncubator::AsynchronousIfNested,
    this object will also be created asynchronously. If \a forContext is 0
    (the default), the \a context will be used for this decision.

    The created object and its creation status are available via the
    \a incubator.

    \sa QQmlIncubator
*/

void QQmlComponent::create(QQmlIncubator &incubator, QQmlContext *context,
                                   QQmlContext *forContext)
{
    Q_D(QQmlComponent);

    if (!context)
        context = d->engine->rootContext();

    QQmlContextData *contextData = QQmlContextData::get(context);
    QQmlContextData *forContextData = contextData;
    if (forContext) forContextData = QQmlContextData::get(forContext);

    if (!contextData->isValid()) {
        qWarning("QQmlComponent: Cannot create a component in an invalid context");
        return;
    }

    if (contextData->engine != d->engine) {
        qWarning("QQmlComponent: Must create component in context from the same QQmlEngine");
        return;
    }

    if (!isReady()) {
        qWarning("QQmlComponent: Component is not ready");
        return;
    }

    incubator.clear();
    QExplicitlySharedDataPointer<QQmlIncubatorPrivate> p(incubator.d);

    QQmlEnginePrivate *enginePriv = QQmlEnginePrivate::get(d->engine);

    p->compilationUnit = d->compilationUnit;
    p->enginePriv = enginePriv;
    p->creator.reset(new QQmlObjectCreator(contextData, d->compilationUnit, d->creationContext, p.data()));
    p->subComponentToCreate = d->start;

    enginePriv->incubate(incubator, forContextData);
}

class QQmlComponentIncubator;

namespace QV4 {

namespace Heap {

struct QmlIncubatorObject : Object {
    void init(QQmlIncubator::IncubationMode = QQmlIncubator::Asynchronous);
    inline void destroy();
    QQmlComponentIncubator *incubator;
    QQmlQPointer<QObject> parent;
    QV4::Value valuemap;
    QV4::Value statusChanged;
    Pointer<Heap::QmlContext> qmlContext;
};

}

struct QmlIncubatorObject : public QV4::Object
{
    V4_OBJECT2(QmlIncubatorObject, Object)
    V4_NEEDS_DESTROY

    static QV4::ReturnedValue method_get_statusChanged(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_set_statusChanged(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_get_status(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_get_object(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_forceCompletion(QV4::CallContext *ctx);

    static void markObjects(QV4::Heap::Base *that, QV4::ExecutionEngine *e);

    void statusChanged(QQmlIncubator::Status);
    void setInitialState(QObject *);
};

}

DEFINE_OBJECT_VTABLE(QV4::QmlIncubatorObject);

class QQmlComponentIncubator : public QQmlIncubator
{
public:
    QQmlComponentIncubator(QV4::Heap::QmlIncubatorObject *inc, IncubationMode mode)
        : QQmlIncubator(mode)
        , incubatorObject(inc)
    {}

    virtual void statusChanged(Status s) {
        QV4::Scope scope(incubatorObject->internalClass->engine);
        QV4::Scoped<QV4::QmlIncubatorObject> i(scope, incubatorObject);
        i->statusChanged(s);
    }

    virtual void setInitialState(QObject *o) {
        QV4::Scope scope(incubatorObject->internalClass->engine);
        QV4::Scoped<QV4::QmlIncubatorObject> i(scope, incubatorObject);
        i->setInitialState(o);
    }

    QV4::Heap::QmlIncubatorObject *incubatorObject;
};


static void QQmlComponent_setQmlParent(QObject *me, QObject *parent)
{
    if (parent) {
        me->setParent(parent);
        typedef QQmlPrivate::AutoParentFunction APF;
        QList<APF> functions = QQmlMetaType::parentFunctions();

        bool needParent = false;
        for (int ii = 0; ii < functions.count(); ++ii) {
            QQmlPrivate::AutoParentResult res = functions.at(ii)(me, parent);
            if (res == QQmlPrivate::Parented) {
                needParent = false;
                break;
            } else if (res == QQmlPrivate::IncompatibleParent) {
                needParent = true;
            }
        }
        if (needParent)
            qWarning("QQmlComponent: Created graphical object was not "
                     "placed in the graphics scene.");
    }
}

/*!
    \qmlmethod object Component::createObject(QtObject parent, object properties)

    Creates and returns an object instance of this component that will have
    the given \a parent and \a properties. The \a properties argument is optional.
    Returns null if object creation fails.

    The object will be created in the same context as the one in which the component
    was created. This function will always return null when called on components
    which were not created in QML.

    If you wish to create an object without setting a parent, specify \c null for
    the \a parent value. Note that if the returned object is to be displayed, you
    must provide a valid \a parent value or set the returned object's \l{Item::parent}{parent}
    property, otherwise the object will not be visible.

    If a \a parent is not provided to createObject(), a reference to the returned object must be held so that
    it is not destroyed by the garbage collector. This is true regardless of whether \l{Item::parent} is set afterwards,
    because setting the Item parent does not change object ownership. Only the graphical parent is changed.

    As of \c {QtQuick 1.1}, this method accepts an optional \a properties argument that specifies a
    map of initial property values for the created object. These values are applied before the object
    creation is finalized. This is more efficient than setting property values after object creation,
    particularly where large sets of property values are defined, and also allows property bindings
    to be set up (using \l{Qt::binding}{Qt.binding}) before the object is created.

    The \a properties argument is specified as a map of property-value items. For example, the code
    below creates an object with initial \c x and \c y values of 100 and 100, respectively:

    \js
        var component = Qt.createComponent("Button.qml");
        if (component.status == Component.Ready)
            component.createObject(parent, {"x": 100, "y": 100});
    \endjs

    Dynamically created instances can be deleted with the \c destroy() method.
    See \l {Dynamic QML Object Creation from JavaScript} for more information.

    \sa incubateObject()
*/


void QQmlComponentPrivate::setInitialProperties(QV4::ExecutionEngine *engine, QV4::QmlContext *qmlContext, const QV4::Value &o, const QV4::Value &v)
{
    QV4::Scope scope(engine);
    QV4::ScopedObject object(scope);
    QV4::ScopedObject valueMap(scope, v);
    QV4::ObjectIterator it(scope, valueMap, QV4::ObjectIterator::EnumerableOnly|QV4::ObjectIterator::WithProtoChain);
    QV4::ScopedString name(scope);
    QV4::ScopedValue val(scope);
    if (engine->hasException)
        return;

    QV4::ExecutionContextSaver saver(scope);
    engine->pushContext(qmlContext);

    while (1) {
        name = it.nextPropertyNameAsString(val);
        if (!name)
            break;
        object = o;
        const QStringList properties = name->toQString().split(QLatin1Char('.'));
        for (int i = 0; i < properties.length() - 1; ++i) {
            name = engine->newString(properties.at(i));
            object = object->get(name);
            if (engine->hasException || !object) {
                break;
            }
        }
        if (engine->hasException || !object) {
            engine->hasException = false;
            continue;
        }
        name = engine->newString(properties.last());
        object->put(name, val);
        if (engine->hasException) {
            engine->hasException = false;
            continue;
        }
    }

    engine->hasException = false;
}

/*!
    \internal
*/
void QQmlComponent::createObject(QQmlV4Function *args)
{
    Q_D(QQmlComponent);
    Q_ASSERT(d->engine);
    Q_ASSERT(args);

    QObject *parent = 0;
    QV4::ExecutionEngine *v4 = args->v4engine();
    QV4::Scope scope(v4);
    QV4::ScopedValue valuemap(scope, QV4::Primitive::undefinedValue());

    if (args->length() >= 1) {
        QV4::Scoped<QV4::QObjectWrapper> qobjectWrapper(scope, (*args)[0]);
        if (qobjectWrapper)
            parent = qobjectWrapper->object();
    }

    if (args->length() >= 2) {
        QV4::ScopedValue v(scope, (*args)[1]);
        if (!v->as<QV4::Object>() || v->as<QV4::ArrayObject>()) {
            qmlInfo(this) << tr("createObject: value is not an object");
            args->setReturnValue(QV4::Encode::null());
            return;
        }
        valuemap = v;
    }

    QQmlContext *ctxt = creationContext();
    if (!ctxt) ctxt = d->engine->rootContext();

    QObject *rv = beginCreate(ctxt);

    if (!rv) {
        args->setReturnValue(QV4::Encode::null());
        return;
    }

    QQmlComponent_setQmlParent(rv, parent);

    QV4::ScopedValue object(scope, QV4::QObjectWrapper::wrap(v4, rv));
    Q_ASSERT(object->isObject());

    if (!valuemap->isUndefined()) {
        QV4::Scoped<QV4::QmlContext> qmlContext(scope, v4->qmlContext());
        QQmlComponentPrivate::setInitialProperties(v4, qmlContext, object, valuemap);
    }

    d->completeCreate();

    Q_ASSERT(QQmlData::get(rv));
    QQmlData::get(rv)->explicitIndestructibleSet = false;
    QQmlData::get(rv)->indestructible = false;

    args->setReturnValue(object->asReturnedValue());
}

/*!
    \qmlmethod object Component::incubateObject(Item parent, object properties, enumeration mode)

    Creates an incubator for an instance of this component. Incubators allow new component
    instances to be instantiated asynchronously and do not cause freezes in the UI.

    The \a parent argument specifies the parent the created instance will have. Omitting the
    parameter or passing null will create an object with no parent. In this case, a reference
    to the created object must be held so that it is not destroyed by the garbage collector.

    The \a properties argument is specified as a map of property-value items which will be
    set on the created object during its construction. \a mode may be Qt.Synchronous or
    Qt.Asynchronous, and controls whether the instance is created synchronously or asynchronously.
    The default is asynchronous. In some circumstances, even if Qt.Synchronous is specified,
    the incubator may create the object asynchronously. This happens if the component calling
    incubateObject() is itself being created asynchronously.

    All three arguments are optional.

    If successful, the method returns an incubator, otherwise null. The incubator has the following
    properties:

    \list
    \li status The status of the incubator. Valid values are Component.Ready, Component.Loading and
       Component.Error.
    \li object The created object instance. Will only be available once the incubator is in the
       Ready status.
    \li onStatusChanged Specifies a callback function to be invoked when the status changes. The
       status is passed as a parameter to the callback.
    \li forceCompletion() Call to complete incubation synchronously.
    \endlist

    The following example demonstrates how to use an incubator:

    \js
        var component = Qt.createComponent("Button.qml");

        var incubator = component.incubateObject(parent, { x: 10, y: 10 });
        if (incubator.status != Component.Ready) {
            incubator.onStatusChanged = function(status) {
                if (status == Component.Ready) {
                    print ("Object", incubator.object, "is now ready!");
                }
            }
        } else {
            print ("Object", incubator.object, "is ready immediately!");
        }
    \endjs

    Dynamically created instances can be deleted with the \c destroy() method.
    See \l {Dynamic QML Object Creation from JavaScript} for more information.

    \sa createObject()
*/

/*!
    \internal
*/
void QQmlComponent::incubateObject(QQmlV4Function *args)
{
    Q_D(QQmlComponent);
    Q_ASSERT(d->engine);
    Q_UNUSED(d);
    Q_ASSERT(args);
    QV4::ExecutionEngine *v4 = args->v4engine();
    QV4::Scope scope(v4);

    QObject *parent = 0;
    QV4::ScopedValue valuemap(scope, QV4::Primitive::undefinedValue());
    QQmlIncubator::IncubationMode mode = QQmlIncubator::Asynchronous;

    if (args->length() >= 1) {
        QV4::Scoped<QV4::QObjectWrapper> qobjectWrapper(scope, (*args)[0]);
        if (qobjectWrapper)
            parent = qobjectWrapper->object();
    }

    if (args->length() >= 2) {
        QV4::ScopedValue v(scope, (*args)[1]);
        if (v->isNull()) {
        } else if (!v->as<QV4::Object>() || v->as<QV4::ArrayObject>()) {
            qmlInfo(this) << tr("createObject: value is not an object");
            args->setReturnValue(QV4::Encode::null());
            return;
        } else {
            valuemap = v;
        }
    }

    if (args->length() >= 3) {
        QV4::ScopedValue val(scope, (*args)[2]);
        quint32 v = val->toUInt32();
        if (v == 0)
            mode = QQmlIncubator::Asynchronous;
        else if (v == 1)
            mode = QQmlIncubator::AsynchronousIfNested;
    }

    QQmlComponentExtension *e = componentExtension(args->v4engine());

    QV4::Scoped<QV4::QmlIncubatorObject> r(scope, v4->memoryManager->allocObject<QV4::QmlIncubatorObject>(mode));
    QV4::ScopedObject p(scope, e->incubationProto.value());
    r->setPrototype(p);

    if (!valuemap->isUndefined())
        r->d()->valuemap = valuemap;
    r->d()->qmlContext = v4->qmlContext();
    r->d()->parent = parent;

    QQmlIncubator *incubator = r->d()->incubator;
    create(*incubator, creationContext());

    if (incubator->status() == QQmlIncubator::Null) {
        args->setReturnValue(QV4::Encode::null());
    } else {
        args->setReturnValue(r.asReturnedValue());
    }
}

// XXX used by QSGLoader
void QQmlComponentPrivate::initializeObjectWithInitialProperties(QV4::QmlContext *qmlContext, const QV4::Value &valuemap, QObject *toCreate)
{
    QQmlEnginePrivate *ep = QQmlEnginePrivate::get(engine);
    QV4::ExecutionEngine *v4engine = QV8Engine::getV4(ep->v8engine());
    QV4::Scope scope(v4engine);

    QV4::ScopedValue object(scope, QV4::QObjectWrapper::wrap(v4engine, toCreate));
    Q_ASSERT(object->as<QV4::Object>());

    if (!valuemap.isUndefined())
        setInitialProperties(v4engine, qmlContext, object, valuemap);
}

QQmlComponentExtension::QQmlComponentExtension(QV4::ExecutionEngine *v4)
{
    QV4::Scope scope(v4);
    QV4::ScopedObject proto(scope, v4->newObject());
    proto->defineAccessorProperty(QStringLiteral("onStatusChanged"),
                                  QV4::QmlIncubatorObject::method_get_statusChanged, QV4::QmlIncubatorObject::method_set_statusChanged);
    proto->defineAccessorProperty(QStringLiteral("status"), QV4::QmlIncubatorObject::method_get_status, 0);
    proto->defineAccessorProperty(QStringLiteral("object"), QV4::QmlIncubatorObject::method_get_object, 0);
    proto->defineDefaultProperty(QStringLiteral("forceCompletion"), QV4::QmlIncubatorObject::method_forceCompletion);

    incubationProto.set(v4, proto);
}

QV4::ReturnedValue QV4::QmlIncubatorObject::method_get_object(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QmlIncubatorObject> o(scope, ctx->thisObject().as<QmlIncubatorObject>());
    if (!o)
        return ctx->engine()->throwTypeError();

    return QV4::QObjectWrapper::wrap(ctx->d()->engine, o->d()->incubator->object());
}

QV4::ReturnedValue QV4::QmlIncubatorObject::method_forceCompletion(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QmlIncubatorObject> o(scope, ctx->thisObject().as<QmlIncubatorObject>());
    if (!o)
        return ctx->engine()->throwTypeError();

    o->d()->incubator->forceCompletion();

    return QV4::Encode::undefined();
}

QV4::ReturnedValue QV4::QmlIncubatorObject::method_get_status(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QmlIncubatorObject> o(scope, ctx->thisObject().as<QmlIncubatorObject>());
    if (!o)
        return ctx->engine()->throwTypeError();

    return QV4::Encode(o->d()->incubator->status());
}

QV4::ReturnedValue QV4::QmlIncubatorObject::method_get_statusChanged(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QmlIncubatorObject> o(scope, ctx->thisObject().as<QmlIncubatorObject>());
    if (!o)
        return ctx->engine()->throwTypeError();

    return o->d()->statusChanged.asReturnedValue();
}

QV4::ReturnedValue QV4::QmlIncubatorObject::method_set_statusChanged(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QmlIncubatorObject> o(scope, ctx->thisObject().as<QmlIncubatorObject>());
    if (!o || ctx->argc() < 1)
        return ctx->engine()->throwTypeError();


    o->d()->statusChanged = ctx->args()[0];
    return QV4::Encode::undefined();
}

QQmlComponentExtension::~QQmlComponentExtension()
{
}

void QV4::Heap::QmlIncubatorObject::init(QQmlIncubator::IncubationMode m)
{
    Object::init();
    valuemap = QV4::Primitive::undefinedValue();
    statusChanged = QV4::Primitive::undefinedValue();
    parent.init();
    qmlContext = nullptr;
    incubator = new QQmlComponentIncubator(this, m);
}

void QV4::Heap::QmlIncubatorObject::destroy() {
    delete incubator;
    parent.destroy();
    Object::destroy();
}

void QV4::QmlIncubatorObject::setInitialState(QObject *o)
{
    QQmlComponent_setQmlParent(o, d()->parent);

    if (!d()->valuemap.isUndefined()) {
        QV4::ExecutionEngine *v4 = engine();
        QV4::Scope scope(v4);
        QV4::ScopedObject obj(scope, QV4::QObjectWrapper::wrap(v4, o));
        QV4::Scoped<QV4::QmlContext> qmlCtxt(scope, d()->qmlContext);
        QQmlComponentPrivate::setInitialProperties(v4, qmlCtxt, obj, d()->valuemap);
    }
}

void QV4::QmlIncubatorObject::markObjects(QV4::Heap::Base *that, QV4::ExecutionEngine *e)
{
    QmlIncubatorObject::Data *o = static_cast<QmlIncubatorObject::Data *>(that);
    o->valuemap.mark(e);
    o->statusChanged.mark(e);
    if (o->qmlContext)
        o->qmlContext->mark(e);
    Object::markObjects(that, e);
}

void QV4::QmlIncubatorObject::statusChanged(QQmlIncubator::Status s)
{
    QV4::Scope scope(engine());
    // hold the incubated object in a scoped value to prevent it's destruction before this method returns
    QV4::ScopedObject incubatedObject(scope, QV4::QObjectWrapper::wrap(scope.engine, d()->incubator->object()));

    if (s == QQmlIncubator::Ready) {
        Q_ASSERT(QQmlData::get(d()->incubator->object()));
        QQmlData::get(d()->incubator->object())->explicitIndestructibleSet = false;
        QQmlData::get(d()->incubator->object())->indestructible = false;
    }

    QV4::ScopedFunctionObject f(scope, d()->statusChanged);
    if (f) {
        QV4::ScopedCallData callData(scope, 1);
        callData->thisObject = this;
        callData->args[0] = QV4::Primitive::fromUInt32(s);
        f->call(scope, callData);
        if (scope.hasException()) {
            QQmlError error = scope.engine->catchExceptionAsQmlError();
            QQmlEnginePrivate::warning(QQmlEnginePrivate::get(scope.engine->qmlEngine()), error);
        }
    }
}

#undef INITIALPROPERTIES_SOURCE

QT_END_NAMESPACE

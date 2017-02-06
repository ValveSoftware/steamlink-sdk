/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQuick module of the Qt Toolkit.
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

#include "qquickloader_p_p.h"

#include <QtQml/qqmlinfo.h>

#include <private/qqmlengine_p.h>
#include <private/qqmlglobal_p.h>

#include <private/qqmlcomponent_p.h>

QT_BEGIN_NAMESPACE

static const QQuickItemPrivate::ChangeTypes watchedChanges
    = QQuickItemPrivate::Geometry | QQuickItemPrivate::ImplicitWidth | QQuickItemPrivate::ImplicitHeight;

QQuickLoaderPrivate::QQuickLoaderPrivate()
    : item(0), object(0), component(0), itemContext(0), incubator(0), updatingSize(false),
      active(true), loadingFromSource(false), asynchronous(false)
{
}

QQuickLoaderPrivate::~QQuickLoaderPrivate()
{
    delete itemContext;
    itemContext = 0;
    delete incubator;
    disposeInitialPropertyValues();
}

void QQuickLoaderPrivate::itemGeometryChanged(QQuickItem *resizeItem, QQuickGeometryChange change,
                                              const QRectF &oldGeometry)
{
    if (resizeItem == item)
        _q_updateSize(false);
    QQuickItemChangeListener::itemGeometryChanged(resizeItem, change, oldGeometry);
}

void QQuickLoaderPrivate::itemImplicitWidthChanged(QQuickItem *)
{
    Q_Q(QQuickLoader);
    q->setImplicitWidth(getImplicitWidth());
}

void QQuickLoaderPrivate::itemImplicitHeightChanged(QQuickItem *)
{
    Q_Q(QQuickLoader);
    q->setImplicitHeight(getImplicitHeight());
}

void QQuickLoaderPrivate::clear()
{
    Q_Q(QQuickLoader);
    disposeInitialPropertyValues();

    if (incubator)
        incubator->clear();

    delete itemContext;
    itemContext = 0;

    if (loadingFromSource && component) {
        // disconnect since we deleteLater
        QObject::disconnect(component, SIGNAL(statusChanged(QQmlComponent::Status)),
                q, SLOT(_q_sourceLoaded()));
        QObject::disconnect(component, SIGNAL(progressChanged(qreal)),
                q, SIGNAL(progressChanged()));
        component->deleteLater();
        component = 0;
    }
    componentStrongReference.clear();
    source = QUrl();

    if (item) {
        QQuickItemPrivate *p = QQuickItemPrivate::get(item);
        p->removeItemChangeListener(this, watchedChanges);

        // We can't delete immediately because our item may have triggered
        // the Loader to load a different item.
        item->setParentItem(0);
        item->setVisible(false);
        item = 0;
    }
    if (object) {
        object->deleteLater();
        object = 0;
    }
}

void QQuickLoaderPrivate::initResize()
{
    if (!item)
        return;
    QQuickItemPrivate *p = QQuickItemPrivate::get(item);
    p->addItemChangeListener(this, watchedChanges);
    _q_updateSize();
}

qreal QQuickLoaderPrivate::getImplicitWidth() const
{
    Q_Q(const QQuickLoader);
    // If the Loader has a valid width then Loader has set an explicit width on the
    // item, and we want the item's implicitWidth.  If the Loader's width has
    // not been set then its implicitWidth is the width of the item.
    if (item)
        return q->widthValid() ? item->implicitWidth() : item->width();
    return QQuickImplicitSizeItemPrivate::getImplicitWidth();
}

qreal QQuickLoaderPrivate::getImplicitHeight() const
{
    Q_Q(const QQuickLoader);
    // If the Loader has a valid height then Loader has set an explicit height on the
    // item, and we want the item's implicitHeight.  If the Loader's height has
    // not been set then its implicitHeight is the height of the item.
    if (item)
        return q->heightValid() ? item->implicitHeight() : item->height();
    return QQuickImplicitSizeItemPrivate::getImplicitHeight();
}

/*!
    \qmltype Loader
    \instantiates QQuickLoader
    \inqmlmodule QtQuick
    \ingroup qtquick-dynamic
    \inherits Item

    \brief Allows dynamic loading of a subtree from a URL or Component

    Loader is used to dynamically load QML components.

    Loader can load a
    QML file (using the \l source property) or a \l Component object (using
    the \l sourceComponent property). It is useful for delaying the creation
    of a component until it is required: for example, when a component should
    be created on demand, or when a component should not be created
    unnecessarily for performance reasons.

    Here is a Loader that loads "Page1.qml" as a component when the
    \l MouseArea is clicked:

    \snippet qml/loader/simple.qml 0

    The loaded object can be accessed using the \l item property.

    If the \l source or \l sourceComponent changes, any previously instantiated
    items are destroyed. Setting \l source to an empty string or setting
    \l sourceComponent to \c undefined destroys the currently loaded object,
    freeing resources and leaving the Loader empty.

    \section2 Loader sizing behavior

    If the source component is not an Item type, Loader does not
    apply any special sizing rules.  When used to load visual types,
    Loader applies the following sizing rules:

    \list
    \li If an explicit size is not specified for the Loader, the Loader
    is automatically resized to the size of the loaded item once the
    component is loaded.
    \li If the size of the Loader is specified explicitly by setting
    the width, height or by anchoring, the loaded item will be resized
    to the size of the Loader.
    \endlist

    In both scenarios the size of the item and the Loader are identical.
    This ensures that anchoring to the Loader is equivalent to anchoring
    to the loaded item.

    \table
    \row
    \li sizeloader.qml
    \li sizeitem.qml
    \row
    \li \snippet qml/loader/sizeloader.qml 0
    \li \snippet qml/loader/sizeitem.qml 0
    \row
    \li The red rectangle will be sized to the size of the root item.
    \li The red rectangle will be 50x50, centered in the root item.
    \endtable


    \section2 Receiving signals from loaded objects

    Any signals emitted from the loaded object can be received using the
    \l Connections type. For example, the following \c application.qml
    loads \c MyItem.qml, and is able to receive the \c message signal from
    the loaded item through a \l Connections object:

    \table 70%
    \row
    \li application.qml
    \li MyItem.qml
    \row
    \li \snippet qml/loader/connections.qml 0
    \li \snippet qml/loader/MyItem.qml 0
    \endtable

    Alternatively, since \c MyItem.qml is loaded within the scope of the
    Loader, it could also directly call any function defined in the Loader or
    its parent \l Item.


    \section2 Focus and key events

    Loader is a focus scope. Its \l {Item::}{focus} property must be set to
    \c true for any of its children to get the \e {active focus}. (See
    \l{Keyboard Focus in Qt Quick}
    for more details.) Any key events received in the loaded item should likely
    also be \l {KeyEvent::}{accepted} so they are not propagated to the Loader.

    For example, the following \c application.qml loads \c KeyReader.qml when
    the \l MouseArea is clicked.  Notice the \l {Item::}{focus} property is
    set to \c true for the Loader as well as the \l Item in the dynamically
    loaded object:

    \table
    \row
    \li application.qml
    \li KeyReader.qml
    \row
    \li \snippet qml/loader/focus.qml 0
    \li \snippet qml/loader/KeyReader.qml 0
    \endtable

    Once \c KeyReader.qml is loaded, it accepts key events and sets
    \c event.accepted to \c true so that the event is not propagated to the
    parent \l Rectangle.

    Since \c {QtQuick 2.0}, Loader can also load non-visual components.

    \section2 Using a Loader within a view delegate

    In some cases you may wish to use a Loader within a view delegate to improve delegate
    loading performance. This works well in most cases, but there is one important issue to

    In the following example, the \c index context property inserted by the ListView into \c delegateComponent's
    context will be inaccessible to Text, as the Loader will use the creation context of \c myComponent as the parent
    context when instantiating it, and \c index does not refer to anything within that context chain.

    \snippet qml/loader/creationContext1.qml 0

    In this situation we can either move the component inline,

    \snippet qml/loader/creationContext2.qml 0

    into a separate file,

    \snippet qml/loader/creationContext3.qml 0

    or explicitly set the required information as a property of the Loader (this works because the
    Loader sets itself as the context object for the component it is loading).

    \snippet qml/loader/creationContext4.qml 0

    \sa {dynamic-object-creation}{Dynamic Object Creation}
*/

QQuickLoader::QQuickLoader(QQuickItem *parent)
  : QQuickImplicitSizeItem(*(new QQuickLoaderPrivate), parent)
{
    setFlag(ItemIsFocusScope);
}

QQuickLoader::~QQuickLoader()
{
    Q_D(QQuickLoader);
    if (d->item) {
        QQuickItemPrivate *p = QQuickItemPrivate::get(d->item);
        p->removeItemChangeListener(d, watchedChanges);
    }
}

/*!
    \qmlproperty bool QtQuick::Loader::active
    This property is \c true if the Loader is currently active.
    The default value for this property is \c true.

    If the Loader is inactive, changing the \l source or \l sourceComponent
    will not cause the item to be instantiated until the Loader is made active.

    Setting the value to inactive will cause any \l item loaded by the loader
    to be released, but will not affect the \l source or \l sourceComponent.

    The \l status of an inactive loader is always \c Null.

    \sa source, sourceComponent
 */
bool QQuickLoader::active() const
{
    Q_D(const QQuickLoader);
    return d->active;
}

void QQuickLoader::setActive(bool newVal)
{
    Q_D(QQuickLoader);
    if (d->active == newVal)
        return;

    d->active = newVal;
    if (newVal == true) {
        if (d->loadingFromSource) {
            loadFromSource();
        } else {
            loadFromSourceComponent();
        }
    } else {
        // cancel any current incubation
        if (d->incubator) {
            d->incubator->clear();
            delete d->itemContext;
            d->itemContext = 0;
        }

        if (d->item) {
            QQuickItemPrivate *p = QQuickItemPrivate::get(d->item);
            p->removeItemChangeListener(d, watchedChanges);

            // We can't delete immediately because our item may have triggered
            // the Loader to load a different item.
            d->item->setParentItem(0);
            d->item->setVisible(false);
            d->item = 0;
        }
        if (d->object) {
            d->object->deleteLater();
            d->object = 0;
            emit itemChanged();
        }
        emit statusChanged();
    }
    emit activeChanged();
}


/*!
    \qmlproperty url QtQuick::Loader::source
    This property holds the URL of the QML component to instantiate.

    Since \c {QtQuick 2.0}, Loader is able to load any type of object; it
    is not restricted to Item types.

    To unload the currently loaded object, set this property to an empty string,
    or set \l sourceComponent to \c undefined. Setting \c source to a
    new URL will also cause the item created by the previous URL to be unloaded.

    \sa sourceComponent, status, progress
*/
QUrl QQuickLoader::source() const
{
    Q_D(const QQuickLoader);
    return d->source;
}

void QQuickLoader::setSource(const QUrl &url)
{
    setSource(url, true); // clear previous values
}

void QQuickLoader::setSource(const QUrl &url, bool needsClear)
{
    Q_D(QQuickLoader);
    if (d->source == url)
        return;

    if (needsClear)
        d->clear();

    d->source = url;
    d->loadingFromSource = true;

    if (d->active)
        loadFromSource();
    else
        emit sourceChanged();
}

void QQuickLoader::loadFromSource()
{
    Q_D(QQuickLoader);
    if (d->source.isEmpty()) {
        emit sourceChanged();
        emit statusChanged();
        emit progressChanged();
        emit itemChanged();
        return;
    }

    if (isComponentComplete()) {
        QQmlComponent::CompilationMode mode = d->asynchronous ? QQmlComponent::Asynchronous : QQmlComponent::PreferSynchronous;
        d->component = new QQmlComponent(qmlEngine(this), d->source, mode, this);
        d->load();
    }
}

/*!
    \qmlproperty Component QtQuick::Loader::sourceComponent
    This property holds the \l{Component} to instantiate.

    \qml
    Item {
        Component {
            id: redSquare
            Rectangle { color: "red"; width: 10; height: 10 }
        }

        Loader { sourceComponent: redSquare }
        Loader { sourceComponent: redSquare; x: 10 }
    }
    \endqml

    To unload the currently loaded object, set this property to \c undefined.

    Since \c {QtQuick 2.0}, Loader is able to load any type of object; it
    is not restricted to Item types.

    \sa source, progress
*/

QQmlComponent *QQuickLoader::sourceComponent() const
{
    Q_D(const QQuickLoader);
    return d->component;
}

void QQuickLoader::setSourceComponent(QQmlComponent *comp)
{
    Q_D(QQuickLoader);
    if (comp == d->component)
        return;

    d->clear();

    d->component = comp;
    if (comp) {
        if (QQmlData *ddata = QQmlData::get(comp))
            d->componentStrongReference = ddata->jsWrapper;
    }
    d->loadingFromSource = false;

    if (d->active)
        loadFromSourceComponent();
    else
        emit sourceComponentChanged();
}

void QQuickLoader::resetSourceComponent()
{
    setSourceComponent(0);
}

void QQuickLoader::loadFromSourceComponent()
{
    Q_D(QQuickLoader);
    if (!d->component) {
        emit sourceComponentChanged();
        emit statusChanged();
        emit progressChanged();
        emit itemChanged();
        return;
    }

    if (isComponentComplete())
        d->load();
}

/*!
    \qmlmethod object QtQuick::Loader::setSource(url source, object properties)

    Creates an object instance of the given \a source component that will have
    the given \a properties. The \a properties argument is optional.  The instance
    will be accessible via the \l item property once loading and instantiation
    is complete.

    If the \l active property is \c false at the time when this function is called,
    the given \a source component will not be loaded but the \a source and initial
    \a properties will be cached.  When the loader is made \l active, an instance of
    the \a source component will be created with the initial \a properties set.

    Setting the initial property values of an instance of a component in this manner
    will \b{not} trigger any associated \l{Behavior}s.

    Note that the cached \a properties will be cleared if the \l source or \l sourceComponent
    is changed after calling this function but prior to setting the loader \l active.

    Example:
    \table 70%
    \row
    \li
    \qml
    // ExampleComponent.qml
    import QtQuick 2.0
    Rectangle {
        id: rect
        color: "red"
        width: 10
        height: 10

        Behavior on color {
            NumberAnimation {
                target: rect
                property: "width"
                to: (rect.width + 20)
                duration: 0
            }
        }
    }
    \endqml
    \li
    \qml
    // example.qml
    import QtQuick 2.0
    Item {
        Loader {
            id: squareLoader
            onLoaded: console.log(squareLoader.item.width);
            // prints [10], not [30]
        }

        Component.onCompleted: {
            squareLoader.setSource("ExampleComponent.qml",
                                 { "color": "blue" });
            // will trigger the onLoaded code when complete.
        }
    }
    \endqml
    \endtable

    \sa source, active
*/
void QQuickLoader::setSource(QQmlV4Function *args)
{
    Q_ASSERT(args);
    Q_D(QQuickLoader);

    bool ipvError = false;
    args->setReturnValue(QV4::Encode::undefined());
    QV4::Scope scope(args->v4engine());
    QV4::ScopedValue ipv(scope, d->extractInitialPropertyValues(args, this, &ipvError));
    if (ipvError)
        return;

    d->clear();
    QUrl sourceUrl = d->resolveSourceUrl(args);
    if (!ipv->isUndefined()) {
        d->disposeInitialPropertyValues();
        d->initialPropertyValues.set(args->v4engine(), ipv);
    }
    d->qmlCallingContext.set(scope.engine, scope.engine->qmlContext());

    setSource(sourceUrl, false); // already cleared and set ipv above.
}

void QQuickLoaderPrivate::disposeInitialPropertyValues()
{
}

void QQuickLoaderPrivate::load()
{
    Q_Q(QQuickLoader);

    if (!q->isComponentComplete() || !component)
        return;

    if (!component->isLoading()) {
        _q_sourceLoaded();
    } else {
        QObject::connect(component, SIGNAL(statusChanged(QQmlComponent::Status)),
                q, SLOT(_q_sourceLoaded()));
        QObject::connect(component, SIGNAL(progressChanged(qreal)),
                q, SIGNAL(progressChanged()));
        emit q->statusChanged();
        emit q->progressChanged();
        if (loadingFromSource)
            emit q->sourceChanged();
        else
            emit q->sourceComponentChanged();
        emit q->itemChanged();
    }
}

void QQuickLoaderIncubator::setInitialState(QObject *o)
{
    loader->setInitialState(o);
}

void QQuickLoaderPrivate::setInitialState(QObject *obj)
{
    Q_Q(QQuickLoader);

    QQuickItem *item = qmlobject_cast<QQuickItem*>(obj);
    if (item) {
        // If the item doesn't have an explicit size, but the Loader
        // does, then set the item's size now before bindings are
        // evaluated, otherwise we will end up resizing the item
        // later and triggering any affected bindings/anchors.
        if (widthValid && !QQuickItemPrivate::get(item)->widthValid)
            item->setWidth(q->width());
        if (heightValid && !QQuickItemPrivate::get(item)->heightValid)
            item->setHeight(q->height());
        item->setParentItem(q);
    }
    if (obj) {
        QQml_setParent_noEvent(itemContext, obj);
        QQml_setParent_noEvent(obj, q);
        itemContext = 0;
    }

    if (initialPropertyValues.isUndefined())
        return;

    QQmlComponentPrivate *d = QQmlComponentPrivate::get(component);
    Q_ASSERT(d && d->engine);
    QV4::ExecutionEngine *v4 = QV8Engine::getV4(d->engine);
    Q_ASSERT(v4);
    QV4::Scope scope(v4);
    QV4::ScopedValue ipv(scope, initialPropertyValues.value());
    QV4::Scoped<QV4::QmlContext> qmlContext(scope, qmlCallingContext.value());
    d->initializeObjectWithInitialProperties(qmlContext, ipv, obj);
}

void QQuickLoaderIncubator::statusChanged(Status status)
{
    loader->incubatorStateChanged(status);
}

void QQuickLoaderPrivate::incubatorStateChanged(QQmlIncubator::Status status)
{
    Q_Q(QQuickLoader);
    if (status == QQmlIncubator::Loading || status == QQmlIncubator::Null)
        return;

    if (status == QQmlIncubator::Ready) {
        object = incubator->object();
        item = qmlobject_cast<QQuickItem*>(object);
        emit q->itemChanged();
        initResize();
        incubator->clear();
    } else if (status == QQmlIncubator::Error) {
        if (!incubator->errors().isEmpty())
            QQmlEnginePrivate::warning(qmlEngine(q), incubator->errors());
        delete itemContext;
        itemContext = 0;
        delete incubator->object();
        source = QUrl();
        emit q->itemChanged();
    }
    if (loadingFromSource)
        emit q->sourceChanged();
    else
        emit q->sourceComponentChanged();
    emit q->statusChanged();
    emit q->progressChanged();
    if (status == QQmlIncubator::Ready)
        emit q->loaded();
    disposeInitialPropertyValues(); // cleanup
}

void QQuickLoaderPrivate::_q_sourceLoaded()
{
    Q_Q(QQuickLoader);
    if (!component || !component->errors().isEmpty()) {
        if (component)
            QQmlEnginePrivate::warning(qmlEngine(q), component->errors());
        if (loadingFromSource)
            emit q->sourceChanged();
        else
            emit q->sourceComponentChanged();
        emit q->statusChanged();
        emit q->progressChanged();
        emit q->itemChanged(); //Like clearing source, emit itemChanged even if previous item was also null
        disposeInitialPropertyValues(); // cleanup
        return;
    }

    QQmlContext *creationContext = component->creationContext();
    if (!creationContext) creationContext = qmlContext(q);
    itemContext = new QQmlContext(creationContext);
    itemContext->setContextObject(q);

    delete incubator;
    incubator = new QQuickLoaderIncubator(this, asynchronous ? QQmlIncubator::Asynchronous : QQmlIncubator::AsynchronousIfNested);

    component->create(*incubator, itemContext);

    if (incubator && incubator->status() == QQmlIncubator::Loading)
        emit q->statusChanged();
}

/*!
    \qmlproperty enumeration QtQuick::Loader::status

    This property holds the status of QML loading.  It can be one of:
    \list
    \li Loader.Null - the loader is inactive or no QML source has been set
    \li Loader.Ready - the QML source has been loaded
    \li Loader.Loading - the QML source is currently being loaded
    \li Loader.Error - an error occurred while loading the QML source
    \endlist

    Use this status to provide an update or respond to the status change in some way.
    For example, you could:

    \list
    \li Trigger a state change:
    \qml
        State { name: 'loaded'; when: loader.status == Loader.Ready }
    \endqml

    \li Implement an \c onStatusChanged signal handler:
    \qml
        Loader {
            id: loader
            onStatusChanged: if (loader.status == Loader.Ready) console.log('Loaded')
        }
    \endqml

    \li Bind to the status value:
    \qml
        Text { text: loader.status == Loader.Ready ? 'Loaded' : 'Not loaded' }
    \endqml
    \endlist

    Note that if the source is a local file, the status will initially be Ready (or Error). While
    there will be no onStatusChanged signal in that case, the onLoaded will still be invoked.

    \sa progress
*/

QQuickLoader::Status QQuickLoader::status() const
{
    Q_D(const QQuickLoader);

    if (!d->active)
        return Null;

    if (d->component) {
        switch (d->component->status()) {
        case QQmlComponent::Loading:
            return Loading;
        case QQmlComponent::Error:
            return Error;
        case QQmlComponent::Null:
            return Null;
        default:
            break;
        }
    }

    if (d->incubator) {
        switch (d->incubator->status()) {
        case QQmlIncubator::Loading:
            return Loading;
        case QQmlIncubator::Error:
            return Error;
        default:
            break;
        }
    }

    if (d->object)
        return Ready;

    return d->source.isEmpty() ? Null : Error;
}

void QQuickLoader::componentComplete()
{
    Q_D(QQuickLoader);
    QQuickItem::componentComplete();
    if (active()) {
        if (d->loadingFromSource) {
            QQmlComponent::CompilationMode mode = d->asynchronous ? QQmlComponent::Asynchronous : QQmlComponent::PreferSynchronous;
            d->component = new QQmlComponent(qmlEngine(this), d->source, mode, this);
        }
        d->load();
    }
}

/*!
    \qmlsignal QtQuick::Loader::loaded()

    This signal is emitted when the \l status becomes \c Loader.Ready, or on successful
    initial load.

    The corresponding handler is \c onLoaded.
*/


/*!
\qmlproperty real QtQuick::Loader::progress

This property holds the progress of loading QML data from the network, from
0.0 (nothing loaded) to 1.0 (finished).  Most QML files are quite small, so
this value will rapidly change from 0 to 1.

\sa status
*/
qreal QQuickLoader::progress() const
{
    Q_D(const QQuickLoader);

    if (d->object)
        return 1.0;

    if (d->component)
        return d->component->progress();

    return 0.0;
}

/*!
\qmlproperty bool QtQuick::Loader::asynchronous

This property holds whether the component will be instantiated asynchronously.

When used in conjunction with the \l source property, loading and compilation
will also be performed in a background thread.

Loading asynchronously creates the objects declared by the component
across multiple frames, and reduces the
likelihood of glitches in animation.  When loading asynchronously the status
will change to Loader.Loading.  Once the entire component has been created, the
\l item will be available and the status will change to Loader.Ready.

Changing the value of this property to \c false while an asynchronous load is in
progress will force immediate, synchronous completion.  This allows beginning an
asynchronous load and then forcing completion if the Loader content must be
accessed before the asynchronous load has completed.

To avoid seeing the items loading progressively set \c visible appropriately, e.g.

\code
Loader {
    source: "mycomponent.qml"
    asynchronous: true
    visible: status == Loader.Ready
}
\endcode

Note that this property affects object instantiation only; it is unrelated to
loading a component asynchronously via a network.
*/
bool QQuickLoader::asynchronous() const
{
    Q_D(const QQuickLoader);
    return d->asynchronous;
}

void QQuickLoader::setAsynchronous(bool a)
{
    Q_D(QQuickLoader);
    if (d->asynchronous == a)
        return;

    d->asynchronous = a;

    if (!d->asynchronous && isComponentComplete() && d->active) {
        if (d->loadingFromSource && d->component && d->component->isLoading()) {
            // Force a synchronous component load
            QUrl currentSource = d->source;
            d->clear();
            d->source = currentSource;
            loadFromSource();
        } else if (d->incubator && d->incubator->isLoading()) {
            d->incubator->forceCompletion();
        }
    }

    emit asynchronousChanged();
}

void QQuickLoaderPrivate::_q_updateSize(bool loaderGeometryChanged)
{
    Q_Q(QQuickLoader);
    if (!item)
        return;

    if (loaderGeometryChanged && q->widthValid())
        item->setWidth(q->width());
    if (loaderGeometryChanged && q->heightValid())
        item->setHeight(q->height());

    if (updatingSize)
        return;

    updatingSize = true;

    q->setImplicitSize(getImplicitWidth(), getImplicitHeight());

    updatingSize = false;
}

/*!
    \qmlproperty object QtQuick::Loader::item
    This property holds the top-level object that is currently loaded.

    Since \c {QtQuick 2.0}, Loader can load any object type.
*/
QObject *QQuickLoader::item() const
{
    Q_D(const QQuickLoader);
    return d->object;
}

void QQuickLoader::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    Q_D(QQuickLoader);
    if (newGeometry != oldGeometry) {
        d->_q_updateSize();
    }
    QQuickItem::geometryChanged(newGeometry, oldGeometry);
}

QUrl QQuickLoaderPrivate::resolveSourceUrl(QQmlV4Function *args)
{
    QV4::Scope scope(args->v4engine());
    QV4::ScopedValue v(scope, (*args)[0]);
    QString arg = v->toQString();
    if (arg.isEmpty())
        return QUrl();

    QQmlContextData *context = scope.engine->callingQmlContext();
    Q_ASSERT(context);
    return context->resolvedUrl(QUrl(arg));
}

QV4::ReturnedValue QQuickLoaderPrivate::extractInitialPropertyValues(QQmlV4Function *args, QObject *loader, bool *error)
{
    QV4::Scope scope(args->v4engine());
    QV4::ScopedValue valuemap(scope, QV4::Primitive::undefinedValue());
    if (args->length() >= 2) {
        QV4::ScopedValue v(scope, (*args)[1]);
        if (!v->isObject() || v->as<QV4::ArrayObject>()) {
            *error = true;
            qmlInfo(loader) << QQuickLoader::tr("setSource: value is not an object");
        } else {
            *error = false;
            valuemap = v;
        }
    }

    return valuemap->asReturnedValue();
}

#include <moc_qquickloader_p.cpp>

QT_END_NAMESPACE

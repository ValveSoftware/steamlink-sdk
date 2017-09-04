/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Quick Templates 2 module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qquickstackelement_p_p.h"
#include "qquickstackview_p_p.h"

#include <QtQml/qqmlinfo.h>
#include <QtQml/qqmlengine.h>
#include <QtQml/qqmlcomponent.h>
#include <QtQml/qqmlincubator.h>
#include <QtQml/private/qv4qobjectwrapper_p.h>
#include <QtQml/private/qqmlcomponent_p.h>
#include <QtQml/private/qqmlengine_p.h>

QT_BEGIN_NAMESPACE

static QQuickStackViewAttached *attachedStackObject(QQuickStackElement *element)
{
    QQuickStackViewAttached *attached = qobject_cast<QQuickStackViewAttached *>(qmlAttachedPropertiesObject<QQuickStackView>(element->item, false));
    if (attached)
        QQuickStackViewAttachedPrivate::get(attached)->element = element;
    return attached;
}

class QQuickStackIncubator : public QQmlIncubator
{
public:
    QQuickStackIncubator(QQuickStackElement *element)
        : QQmlIncubator(Synchronous),
          element(element)
    {
    }

protected:
    void setInitialState(QObject *object) override { element->incubate(object); }

private:
    QQuickStackElement *element;
};

QQuickStackElement::QQuickStackElement()
    : QQuickItemViewTransitionableItem(nullptr),
      index(-1),
      init(false),
      removal(false),
      ownItem(false),
      ownComponent(false),
      widthValid(false),
      heightValid(false),
      context(nullptr),
      component(nullptr),
      view(nullptr),
      status(QQuickStackView::Inactive)
{
}

QQuickStackElement::~QQuickStackElement()
{
    if (item)
        QQuickItemPrivate::get(item)->removeItemChangeListener(this, QQuickItemPrivate::Destroyed);

    if (ownComponent)
        delete component;

    QQuickStackViewAttached *attached = attachedStackObject(this);
    if (item) {
        if (ownItem) {
            item->setParentItem(nullptr);
            item->deleteLater();
            item = nullptr;
        } else {
            setVisible(false);
            if (!widthValid)
                item->resetWidth();
            if (!heightValid)
                item->resetHeight();
            if (item->parentItem() != originalParent) {
                item->setParentItem(originalParent);
            } else {
                if (attached)
                    QQuickStackViewAttachedPrivate::get(attached)->itemParentChanged(item, nullptr);
            }
        }
    }

    if (attached)
        emit attached->removed();

    delete context;
}

QQuickStackElement *QQuickStackElement::fromString(const QString &str, QQuickStackView *view, QString *error)
{
    QUrl url(str);
    if (!url.isValid()) {
        *error = QStringLiteral("invalid url: ") + str;
        return nullptr;
    }

    QQuickStackElement *element = new QQuickStackElement;
    element->component = new QQmlComponent(qmlEngine(view), url, view);
    element->ownComponent = true;
    return element;
}

QQuickStackElement *QQuickStackElement::fromObject(QObject *object, QQuickStackView *view, QString *error)
{
    Q_UNUSED(view);
    QQmlComponent *component = qobject_cast<QQmlComponent *>(object);
    QQuickItem *item = qobject_cast<QQuickItem *>(object);
    if (!component && !item) {
        *error = QQmlMetaType::prettyTypeName(object) + QStringLiteral(" is not supported. Must be Item or Component.");
        return nullptr;
    }

    QQuickStackElement *element = new QQuickStackElement;
    element->component = qobject_cast<QQmlComponent *>(object);
    element->item = qobject_cast<QQuickItem *>(object);
    if (element->item)
        element->originalParent = element->item->parentItem();
    return element;
}

bool QQuickStackElement::load(QQuickStackView *parent)
{
    setView(parent);
    if (!item) {
        ownItem = true;

        if (component->isLoading()) {
            QObject::connect(component, &QQmlComponent::statusChanged, [this](QQmlComponent::Status status) {
                if (status == QQmlComponent::Ready)
                    load(view);
                else if (status == QQmlComponent::Error)
                    QQuickStackViewPrivate::get(view)->warn(component->errorString().trimmed());
            });
            return true;
        }

        QQmlContext *creationContext = component->creationContext();
        if (!creationContext)
            creationContext = qmlContext(parent);
        context = new QQmlContext(creationContext, parent);
        context->setContextObject(parent);

        QQuickStackIncubator incubator(this);
        component->create(incubator, context);
        if (component->isError())
            QQuickStackViewPrivate::get(parent)->warn(component->errorString().trimmed());
    } else {
        initialize();
    }
    return item;
}

void QQuickStackElement::incubate(QObject *object)
{
    item = qmlobject_cast<QQuickItem *>(object);
    if (item) {
        QQmlEngine::setObjectOwnership(item, QQmlEngine::CppOwnership);
        item->setParent(view);
        initialize();
    }
}

void QQuickStackElement::initialize()
{
    if (!item || init)
        return;

    QQuickItemPrivate *p = QQuickItemPrivate::get(item);
    if (!(widthValid = p->widthValid))
        item->setWidth(view->width());
    if (!(heightValid = p->heightValid))
        item->setHeight(view->height());
    item->setParentItem(view);
    p->addItemChangeListener(this, QQuickItemPrivate::Destroyed);

    if (!properties.isUndefined()) {
        QQmlEngine *engine = qmlEngine(view);
        Q_ASSERT(engine);
        QV4::ExecutionEngine *v4 = QQmlEnginePrivate::getV4Engine(engine);
        Q_ASSERT(v4);
        QV4::Scope scope(v4);
        QV4::ScopedValue ipv(scope, properties.value());
        QV4::Scoped<QV4::QmlContext> qmlContext(scope, qmlCallingContext.value());
        QV4::ScopedValue qmlObject(scope, QV4::QObjectWrapper::wrap(v4, item));
        QQmlComponentPrivate::setInitialProperties(v4, qmlContext, qmlObject, ipv);
        properties.clear();
    }

    init = true;
}

void QQuickStackElement::setIndex(int value)
{
    if (index == value)
        return;

    index = value;
    QQuickStackViewAttached *attached = attachedStackObject(this);
    if (attached)
        emit attached->indexChanged();
}

void QQuickStackElement::setView(QQuickStackView *value)
{
    if (view == value)
        return;

    view = value;
    QQuickStackViewAttached *attached = attachedStackObject(this);
    if (attached)
        emit attached->viewChanged();
}

void QQuickStackElement::setStatus(QQuickStackView::Status value)
{
    if (status == value)
        return;

    status = value;
    QQuickStackViewAttached *attached = attachedStackObject(this);
    if (!attached)
        return;

    switch (value) {
    case QQuickStackView::Inactive:
        emit attached->deactivated();
        break;
    case QQuickStackView::Deactivating:
        emit attached->deactivating();
        break;
    case QQuickStackView::Activating:
        emit attached->activating();
        break;
    case QQuickStackView::Active:
        emit attached->activated();
        break;
    default:
        Q_UNREACHABLE();
        break;
    }

    emit attached->statusChanged();
}

void QQuickStackElement::setVisible(bool visible)
{
    QQuickStackViewAttached *attached = attachedStackObject(this);
    if (!item || (attached && QQuickStackViewAttachedPrivate::get(attached)->explicitVisible))
        return;

    item->setVisible(visible);
}

void QQuickStackElement::transitionNextReposition(QQuickItemViewTransitioner *transitioner, QQuickItemViewTransitioner::TransitionType type, bool asTarget)
{
    if (transitioner)
        transitioner->transitionNextReposition(this, type, asTarget);
}

bool QQuickStackElement::prepareTransition(QQuickItemViewTransitioner *transitioner, const QRectF &viewBounds)
{
    if (transitioner) {
        if (item) {
            QQuickAnchors *anchors = QQuickItemPrivate::get(item)->_anchors;
            // TODO: expose QQuickAnchorLine so we can test for other conflicting anchors
            if (anchors && (anchors->fill() || anchors->centerIn()))
                qmlWarning(item) << "StackView has detected conflicting anchors. Transitions may not execute properly.";
        }

        // TODO: add force argument to QQuickItemViewTransitionableItem::prepareTransition()?
        nextTransitionToSet = true;
        nextTransitionFromSet = true;
        nextTransitionFrom += QPointF(1, 1);
        return QQuickItemViewTransitionableItem::prepareTransition(transitioner, index, viewBounds);
    }
    return false;
}

void QQuickStackElement::startTransition(QQuickItemViewTransitioner *transitioner, QQuickStackView::Status status)
{
    setStatus(status);
    if (transitioner)
        QQuickItemViewTransitionableItem::startTransition(transitioner, index);
}

void QQuickStackElement::itemDestroyed(QQuickItem *)
{
    item = nullptr;
}

QT_END_NAMESPACE

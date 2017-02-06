/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#include "qquickstackview_p_p.h"

#include <QtQml/qqmlinfo.h>
#include <QtQml/qqmllist.h>
#include <QtQml/qqmlengine.h>
#include <QtQml/qqmlcomponent.h>
#include <QtQml/qqmlincubator.h>
#include <QtQml/private/qv4qobjectwrapper_p.h>
#include <QtQml/private/qqmlcomponent_p.h>
#include <QtQml/private/qqmlengine_p.h>
#include <QtQuick/private/qquickanimation_p.h>
#include <QtQuick/private/qquicktransition_p.h>
#include <QtQuick/private/qquickitemviewtransition_p.h>

QT_BEGIN_NAMESPACE

static QQuickStackAttached *attachedStackObject(QQuickStackElement *element)
{
    QQuickStackAttached *attached = qobject_cast<QQuickStackAttached *>(qmlAttachedPropertiesObject<QQuickStackView>(element->item, false));
    if (attached)
        QQuickStackAttachedPrivate::get(attached)->element = element;
    return attached;
}

class QQuickStackIncubator : public QQmlIncubator
{
public:
    QQuickStackIncubator(QQuickStackElement *element) : QQmlIncubator(Synchronous), element(element) { }

protected:
    void setInitialState(QObject *object) override { element->incubate(object); }

private:
    QQuickStackElement *element;
};

QQuickStackElement::QQuickStackElement() : QQuickItemViewTransitionableItem(nullptr),
    index(-1), init(false), removal(false), ownItem(false), ownComponent(false), widthValid(false), heightValid(false),
    context(nullptr), component(nullptr), view(nullptr),
    status(QQuickStackView::Inactive)
{
}

QQuickStackElement::~QQuickStackElement()
{
    if (item)
        QQuickItemPrivate::get(item)->removeItemChangeListener(this, QQuickItemPrivate::Destroyed);

    if (ownComponent)
        delete component;

    QQuickStackAttached *attached = attachedStackObject(this);
    if (item) {
        if (ownItem) {
            item->setParentItem(nullptr);
            item->deleteLater();
            item = nullptr;
        } else {
            item->setVisible(false);
            if (!widthValid)
                item->resetWidth();
            if (!heightValid)
                item->resetHeight();
            if (item->parentItem() != originalParent) {
                item->setParentItem(originalParent);
            } else {
                if (attached)
                    QQuickStackAttachedPrivate::get(attached)->itemParentChanged(item, nullptr);
            }
        }
    }

    if (attached)
        emit attached->removed();

    delete context;
}

QQuickStackElement *QQuickStackElement::fromString(const QString &str, QQuickStackView *view)
{
    QQuickStackElement *element = new QQuickStackElement;
    element->component = new QQmlComponent(qmlEngine(view), QUrl(str), view);
    element->ownComponent = true;
    return element;
}

QQuickStackElement *QQuickStackElement::fromObject(QObject *object, QQuickStackView *view)
{
    Q_UNUSED(view);
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
                    qWarning() << qPrintable(component->errorString().trimmed());
            });
            return true;
        }

        QQmlContext *creationContext = component->creationContext();
        if (!creationContext)
            creationContext = qmlContext(parent);
        context = new QQmlContext(creationContext);
        context->setContextObject(parent);

        QQuickStackIncubator incubator(this);
        component->create(incubator, context);
        if (component->isError())
            qWarning() << qPrintable(component->errorString().trimmed());
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
    QQuickStackAttached *attached = attachedStackObject(this);
    if (attached)
        emit attached->indexChanged();
}

void QQuickStackElement::setView(QQuickStackView *value)
{
    if (view == value)
        return;

    view = value;
    QQuickStackAttached *attached = attachedStackObject(this);
    if (attached)
        emit attached->viewChanged();
}

void QQuickStackElement::setStatus(QQuickStackView::Status value)
{
    if (status == value)
        return;

    status = value;
    QQuickStackAttached *attached = attachedStackObject(this);
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
                qmlInfo(item) << "StackView has detected conflicting anchors. Transitions may not execute properly.";
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

QQuickStackViewPrivate::QQuickStackViewPrivate() : busy(false), currentItem(nullptr), transitioner(nullptr)
{
}

void QQuickStackViewPrivate::setCurrentItem(QQuickItem *item)
{
    Q_Q(QQuickStackView);
    if (currentItem == item)
        return;

    currentItem = item;
    if (item)
        item->setVisible(true);
    emit q->currentItemChanged();
}

static bool initProperties(QQuickStackElement *element, const QV4::Value &props, QQmlV4Function *args)
{
    if (props.isObject()) {
        const QV4::QObjectWrapper *wrapper = props.as<QV4::QObjectWrapper>();
        if (!wrapper) {
            QV4::ExecutionEngine *v4 = args->v4engine();
            element->properties.set(v4, props);
            element->qmlCallingContext.set(v4, v4->qmlContext());
            return true;
        }
    }
    return false;
}

QList<QQuickStackElement *> QQuickStackViewPrivate::parseElements(QQmlV4Function *args, int from)
{
    QV4::ExecutionEngine *v4 = args->v4engine();
    QV4::Scope scope(v4);

    QList<QQuickStackElement *> elements;

    int argc = args->length();
    for (int i = from; i < argc; ++i) {
        QV4::ScopedValue arg(scope, (*args)[i]);
        if (QV4::ArrayObject *array = arg->as<QV4::ArrayObject>()) {
            int len = array->getLength();
            for (int j = 0; j < len; ++j) {
                QV4::ScopedValue value(scope, array->getIndexed(j));
                QQuickStackElement *element = createElement(value);
                if (element) {
                    if (j < len - 1) {
                        QV4::ScopedValue props(scope, array->getIndexed(j + 1));
                        if (initProperties(element, props, args))
                            ++j;
                    }
                    elements += element;
                }
            }
        } else {
            QQuickStackElement *element = createElement(arg);
            if (element) {
                if (i < argc - 1) {
                    QV4::ScopedValue props(scope, (*args)[i + 1]);
                    if (initProperties(element, props, args))
                        ++i;
                }
                elements += element;
            }
        }
    }
    return elements;
}

QQuickStackElement *QQuickStackViewPrivate::findElement(QQuickItem *item) const
{
    if (item) {
        for (QQuickStackElement *e : qAsConst(elements)) {
            if (e->item == item)
                return e;
        }
    }
    return nullptr;
}

QQuickStackElement *QQuickStackViewPrivate::findElement(const QV4::Value &value) const
{
    if (const QV4::QObjectWrapper *o = value.as<QV4::QObjectWrapper>())
        return findElement(qobject_cast<QQuickItem *>(o->object()));
    return nullptr;
}

QQuickStackElement *QQuickStackViewPrivate::createElement(const QV4::Value &value)
{
    Q_Q(QQuickStackView);
    if (const QV4::String *s = value.as<QV4::String>())
        return QQuickStackElement::fromString(s->toQString(), q);
    if (const QV4::QObjectWrapper *o = value.as<QV4::QObjectWrapper>())
        return QQuickStackElement::fromObject(o->object(), q);
    return nullptr;
}

bool QQuickStackViewPrivate::pushElements(const QList<QQuickStackElement *> &elems)
{
    Q_Q(QQuickStackView);
    if (!elems.isEmpty()) {
        for (QQuickStackElement *e : elems) {
            e->setIndex(elements.count());
            elements += e;
        }
        return elements.top()->load(q);
    }
    return false;
}

bool QQuickStackViewPrivate::pushElement(QQuickStackElement *element)
{
    if (element)
        return pushElements(QList<QQuickStackElement *>() << element);
    return false;
}

bool QQuickStackViewPrivate::popElements(QQuickStackElement *element)
{
    Q_Q(QQuickStackView);
    while (elements.count() > 1 && elements.top() != element) {
        delete elements.pop();
        if (!element)
            break;
    }
    return elements.top()->load(q);
}

bool QQuickStackViewPrivate::replaceElements(QQuickStackElement *target, const QList<QQuickStackElement *> &elems)
{
    if (target) {
        while (!elements.isEmpty()) {
            QQuickStackElement* top = elements.pop();
            delete top;
            if (top == target)
                break;
        }
    }
    return pushElements(elems);
}

void QQuickStackViewPrivate::ensureTransitioner()
{
    if (!transitioner) {
        transitioner = new QQuickItemViewTransitioner;
        transitioner->setChangeListener(this);
    }
}

void QQuickStackViewPrivate::startTransition(const QQuickStackTransition &first, const QQuickStackTransition &second, bool immediate)
{
    if (first.element)
        first.element->transitionNextReposition(transitioner, first.type, first.target);
    if (second.element)
        second.element->transitionNextReposition(transitioner, second.type, second.target);

    if (first.element) {
        if (immediate || !first.element->item || !first.element->prepareTransition(transitioner, first.viewBounds))
            completeTransition(first.element, transitioner->removeTransition, first.status);
        else
            first.element->startTransition(transitioner, first.status);
    }
    if (second.element) {
        if (immediate || !second.element->item || !second.element->prepareTransition(transitioner, second.viewBounds))
            completeTransition(second.element, transitioner->removeDisplacedTransition, second.status);
        else
            second.element->startTransition(transitioner, second.status);
    }

    if (transitioner) {
        setBusy(!transitioner->runningJobs.isEmpty());
        transitioner->resetTargetLists();
    }
}

void QQuickStackViewPrivate::completeTransition(QQuickStackElement *element, QQuickTransition *transition, QQuickStackView::Status status)
{
    element->setStatus(status);
    if (transition) {
        // TODO: add a proper way to complete a transition
        QQmlListProperty<QQuickAbstractAnimation> animations = transition->animations();
        int count = animations.count(&animations);
        for (int i = 0; i < count; ++i) {
            QQuickAbstractAnimation *anim = animations.at(&animations, i);
            anim->complete();
        }
    }
    viewItemTransitionFinished(element);
}

void QQuickStackViewPrivate::viewItemTransitionFinished(QQuickItemViewTransitionableItem *transitionable)
{
    QQuickStackElement *element = static_cast<QQuickStackElement *>(transitionable);
    if (element->status == QQuickStackView::Activating) {
        element->setStatus(QQuickStackView::Active);
    } else if (element->status == QQuickStackView::Deactivating) {
        element->setStatus(QQuickStackView::Inactive);
        if (element->item)
            element->item->setVisible(false);
        if (element->removal || element->isPendingRemoval())
            removals += element;
    }

    if (transitioner->runningJobs.isEmpty()) {
        qDeleteAll(removals);
        removals.clear();
        setBusy(false);
    }
}

void QQuickStackViewPrivate::setBusy(bool b)
{
    Q_Q(QQuickStackView);
    if (busy == b)
        return;

    busy = b;
    q->setFiltersChildMouseEvents(busy);
    emit q->busyChanged();
}

static QQuickStackTransition exitTransition(QQuickStackView::Operation operation, QQuickStackElement *element, QQuickStackView *view)
{
    QQuickStackTransition st;
    st.status = QQuickStackView::Deactivating;
    st.transition = nullptr;
    st.element = element;

    const QQuickItemViewTransitioner *transitioner = QQuickStackViewPrivate::get(view)->transitioner;

    switch (operation) {
    case QQuickStackView::PushTransition:
        st.target = false;
        st.type = QQuickItemViewTransitioner::AddTransition;
        st.viewBounds = QRectF();
        if (transitioner)
            st.transition = transitioner->addDisplacedTransition;
        break;
    case QQuickStackView::ReplaceTransition:
        st.target = false;
        st.type = QQuickItemViewTransitioner::MoveTransition;
        st.viewBounds = QRectF();
        if (transitioner)
            st.transition = transitioner->moveDisplacedTransition;
        break;
    case QQuickStackView::PopTransition:
        st.target = true;
        st.type = QQuickItemViewTransitioner::RemoveTransition;
        st.viewBounds = view->boundingRect();
        if (transitioner)
            st.transition = transitioner->removeTransition;
        break;
    default:
        Q_UNREACHABLE();
        break;
    }

    return st;
}

static QQuickStackTransition enterTransition(QQuickStackView::Operation operation, QQuickStackElement *element, QQuickStackView *view)
{
    QQuickStackTransition st;
    st.status = QQuickStackView::Activating;
    st.transition = nullptr;
    st.element = element;

    const QQuickItemViewTransitioner *transitioner = QQuickStackViewPrivate::get(view)->transitioner;

    switch (operation) {
    case QQuickStackView::PushTransition:
        st.target = true;
        st.type = QQuickItemViewTransitioner::AddTransition;
        st.viewBounds = view->boundingRect();
        if (transitioner)
            st.transition = transitioner->addTransition;
        break;
    case QQuickStackView::ReplaceTransition:
        st.target = true;
        st.type = QQuickItemViewTransitioner::MoveTransition;
        st.viewBounds = view->boundingRect();
        if (transitioner)
            st.transition = transitioner->moveTransition;
        break;
    case QQuickStackView::PopTransition:
        st.target = false;
        st.type = QQuickItemViewTransitioner::RemoveTransition;
        st.viewBounds = QRectF();
        if (transitioner)
            st.transition = transitioner->removeDisplacedTransition;
        break;
    default:
        Q_UNREACHABLE();
        break;
    }

    return st;
}

static QQuickStackView::Operation operationTransition(QQuickStackView::Operation operation, QQuickStackView::Operation transition)
{
    if (operation == QQuickStackView::Immediate || operation == QQuickStackView::Transition)
        return transition;
    return operation;
}

QQuickStackTransition QQuickStackTransition::popExit(QQuickStackView::Operation operation, QQuickStackElement *element, QQuickStackView *view)
{
    return exitTransition(operationTransition(operation, QQuickStackView::PopTransition), element, view);
}

QQuickStackTransition QQuickStackTransition::popEnter(QQuickStackView::Operation operation, QQuickStackElement *element, QQuickStackView *view)
{
    return enterTransition(operationTransition(operation, QQuickStackView::PopTransition), element, view);
}

QQuickStackTransition QQuickStackTransition::pushExit(QQuickStackView::Operation operation, QQuickStackElement *element, QQuickStackView *view)
{
    return exitTransition(operationTransition(operation, QQuickStackView::PushTransition), element, view);
}

QQuickStackTransition QQuickStackTransition::pushEnter(QQuickStackView::Operation operation, QQuickStackElement *element, QQuickStackView *view)
{
    return enterTransition(operationTransition(operation, QQuickStackView::PushTransition), element, view);
}

QQuickStackTransition QQuickStackTransition::replaceExit(QQuickStackView::Operation operation, QQuickStackElement *element, QQuickStackView *view)
{
    return exitTransition(operationTransition(operation, QQuickStackView::ReplaceTransition), element, view);
}

QQuickStackTransition QQuickStackTransition::replaceEnter(QQuickStackView::Operation operation, QQuickStackElement *element, QQuickStackView *view)
{
    return enterTransition(operationTransition(operation, QQuickStackView::ReplaceTransition), element, view);
}

QT_END_NAMESPACE

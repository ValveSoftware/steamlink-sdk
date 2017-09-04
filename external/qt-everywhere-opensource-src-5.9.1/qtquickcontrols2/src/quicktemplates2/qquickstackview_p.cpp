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

#include "qquickstackview_p_p.h"
#include "qquickstackelement_p_p.h"
#include "qquickstacktransition_p_p.h"

#include <QtQml/qqmlinfo.h>
#include <QtQml/qqmllist.h>
#include <QtQml/private/qv4qmlcontext_p.h>
#include <QtQml/private/qv4qobjectwrapper_p.h>
#include <QtQuick/private/qquickanimation_p.h>
#include <QtQuick/private/qquicktransition_p.h>

QT_BEGIN_NAMESPACE

QQuickStackViewPrivate::QQuickStackViewPrivate()
    : busy(false),
      currentItem(nullptr),
      transitioner(nullptr)
{
}

void QQuickStackViewPrivate::warn(const QString &error)
{
    Q_Q(QQuickStackView);
    if (operation.isEmpty())
        qmlWarning(q) << error;
    else
        qmlWarning(q) << operation << ": " << error;
}

void QQuickStackViewPrivate::setCurrentItem(QQuickStackElement *element)
{
    Q_Q(QQuickStackView);
    QQuickItem *item = element ? element->item : nullptr;
    if (currentItem == item)
        return;

    currentItem = item;
    if (element)
        element->setVisible(true);
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

QList<QQuickStackElement *> QQuickStackViewPrivate::parseElements(int from, QQmlV4Function *args, QStringList *errors)
{
    QV4::ExecutionEngine *v4 = args->v4engine();
    QQmlContextData *context = v4->callingQmlContext();
    QV4::Scope scope(v4);

    QList<QQuickStackElement *> elements;

    int argc = args->length();
    for (int i = from; i < argc; ++i) {
        QV4::ScopedValue arg(scope, (*args)[i]);
        if (QV4::ArrayObject *array = arg->as<QV4::ArrayObject>()) {
            int len = array->getLength();
            for (int j = 0; j < len; ++j) {
                QString error;
                QV4::ScopedValue value(scope, array->getIndexed(j));
                QQuickStackElement *element = createElement(value, context, &error);
                if (element) {
                    if (j < len - 1) {
                        QV4::ScopedValue props(scope, array->getIndexed(j + 1));
                        if (initProperties(element, props, args))
                            ++j;
                    }
                    elements += element;
                } else if (!error.isEmpty()) {
                    *errors += error;
                }
            }
        } else {
            QString error;
            QQuickStackElement *element = createElement(arg, context, &error);
            if (element) {
                if (i < argc - 1) {
                    QV4::ScopedValue props(scope, (*args)[i + 1]);
                    if (initProperties(element, props, args))
                        ++i;
                }
                elements += element;
            } else if (!error.isEmpty()) {
                *errors += error;
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

static QString resolvedUrl(const QString &str, QQmlContextData *context)
{
    QUrl url(str);
    if (url.isRelative())
        return context->resolvedUrl(url).toString();
    return str;
}

QQuickStackElement *QQuickStackViewPrivate::createElement(const QV4::Value &value, QQmlContextData *context, QString *error)
{
    Q_Q(QQuickStackView);
    if (const QV4::String *s = value.as<QV4::String>())
        return QQuickStackElement::fromString(resolvedUrl(s->toQString(), context), q, error);
    if (const QV4::QObjectWrapper *o = value.as<QV4::QObjectWrapper>())
        return QQuickStackElement::fromObject(o->object(), q, error);
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
            completeTransition(first.element, first.transition, first.status);
        else
            first.element->startTransition(transitioner, first.status);
    }
    if (second.element) {
        if (immediate || !second.element->item || !second.element->prepareTransition(transitioner, second.viewBounds))
            completeTransition(second.element, second.transition, second.status);
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
        element->setVisible(false);
        if (element->removal || element->isPendingRemoval())
            removed += element;
    }

    if (transitioner->runningJobs.isEmpty()) {
        qDeleteAll(removed);
        removed.clear();
        setBusy(false);
    }

    removing.remove(element);
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

QT_END_NAMESPACE

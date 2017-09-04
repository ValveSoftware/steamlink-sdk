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

#include "qquickstacktransition_p_p.h"
#include "qquickstackelement_p_p.h"
#include "qquickstackview_p_p.h"

QT_BEGIN_NAMESPACE

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

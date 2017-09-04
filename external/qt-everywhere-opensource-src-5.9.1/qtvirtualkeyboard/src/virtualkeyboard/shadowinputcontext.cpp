/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Virtual Keyboard module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
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
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "shadowinputcontext.h"
#include "inputcontext.h"
#include "virtualkeyboarddebug.h"

#include <QtCore/private/qobject_p.h>
#include <QGuiApplication>
#include <QQuickItem>

QT_BEGIN_NAMESPACE
bool operator==(const QInputMethodEvent::Attribute &attribute1, const QInputMethodEvent::Attribute &attribute2);
QT_END_NAMESPACE

namespace QtVirtualKeyboard {

class ShadowInputContextPrivate : public QObjectPrivate
{
public:
    ShadowInputContextPrivate() :
        QObjectPrivate(),
        inputContext(0),
        anchorRectIntersectsClipRect(false),
        cursorRectIntersectsClipRect(false),
        selectionControlVisible(false)
    {
    }

    InputContext *inputContext;
    QPointer<QObject> inputItem;
    QString preeditText;
    QList<QInputMethodEvent::Attribute> preeditTextAttributes;
    QRectF anchorRectangle;
    QRectF cursorRectangle;
    bool anchorRectIntersectsClipRect;
    bool cursorRectIntersectsClipRect;
    bool selectionControlVisible;
};

ShadowInputContext::ShadowInputContext(QObject *parent) :
    QObject(*new ShadowInputContextPrivate(), parent)
{
}

void ShadowInputContext::setInputContext(InputContext *inputContext)
{
    Q_D(ShadowInputContext);
    d->inputContext = inputContext;
}

QObject *ShadowInputContext::inputItem() const
{
    Q_D(const ShadowInputContext);
    return d->inputItem.data();
}

void ShadowInputContext::setInputItem(QObject *inputItem)
{
    Q_D(ShadowInputContext);
    if (d->inputItem != inputItem) {
        d->inputItem = inputItem;
        emit inputItemChanged();
        update(Qt::ImQueryAll);
    }
}

QRectF ShadowInputContext::anchorRectangle() const
{
    Q_D(const ShadowInputContext);
    return d->anchorRectangle;
}

QRectF ShadowInputContext::cursorRectangle() const
{
    Q_D(const ShadowInputContext);
    return d->cursorRectangle;
}

bool ShadowInputContext::anchorRectIntersectsClipRect() const
{
    Q_D(const ShadowInputContext);
    return d->anchorRectIntersectsClipRect;
}

bool ShadowInputContext::cursorRectIntersectsClipRect() const
{
    Q_D(const ShadowInputContext);
    return d->cursorRectIntersectsClipRect;
}

bool ShadowInputContext::selectionControlVisible() const
{
    Q_D(const ShadowInputContext);
    return d->selectionControlVisible;
}

void ShadowInputContext::setSelectionOnFocusObject(const QPointF &anchorPos, const QPointF &cursorPos)
{
    Q_D(ShadowInputContext);
    QObject *focus = d->inputItem;
    if (!focus)
        return;

    QQuickItem *quickItem = qobject_cast<QQuickItem *>(d->inputItem);
    bool success;
    int anchor = queryFocusObject(Qt::ImCursorPosition, quickItem ? quickItem->mapFromScene(anchorPos) : anchorPos).toInt(&success);
    if (success) {
        int cursor = queryFocusObject(Qt::ImCursorPosition, quickItem ? quickItem->mapFromScene(cursorPos) : cursorPos).toInt(&success);
        if (success) {
            QList<QInputMethodEvent::Attribute> imAttributes;
            imAttributes.append(QInputMethodEvent::Attribute(QInputMethodEvent::Selection, anchor, cursor - anchor, QVariant()));
            QInputMethodEvent event(QString(), imAttributes);
            QGuiApplication::sendEvent(QGuiApplication::focusObject(), &event);
        }
    }
}

void ShadowInputContext::updateSelectionProperties()
{
    Q_D(ShadowInputContext);
    if (!d->inputItem)
        return;

    QInputMethodQueryEvent imQueryEvent(Qt::ImAnchorRectangle |
                                        Qt::ImCursorRectangle |
                                        Qt::ImInputItemClipRectangle);
    QGuiApplication::sendEvent(d->inputItem, &imQueryEvent);
    QQuickItem *quickItem = qobject_cast<QQuickItem *>(d->inputItem);
    const QRectF anchorRect = imQueryEvent.value(Qt::ImAnchorRectangle).toRectF();
    const QRectF cursorRect = imQueryEvent.value(Qt::ImCursorRectangle).toRectF();
    const QRectF anchorRectangle = quickItem ? quickItem->mapRectToScene(anchorRect) : anchorRect;
    const QRectF cursorRectangle = quickItem ? quickItem->mapRectToScene(cursorRect) : cursorRect;
    const QRectF inputItemClipRect = imQueryEvent.value(Qt::ImInputItemClipRectangle).toRectF();
    const bool anchorRectIntersectsClipRect = inputItemClipRect.intersects(anchorRect);
    const bool cursorRectIntersectsClipRect = inputItemClipRect.intersects(cursorRect);
    const bool selectionControlVisible = d->inputContext->selectionControlVisible();

    const bool newAnchorRectangle = anchorRectangle != d->anchorRectangle;
    const bool newCursorRectangle = cursorRectangle != d->cursorRectangle;
    const bool newAnchorRectIntersectsClipRect = anchorRectIntersectsClipRect != d->anchorRectIntersectsClipRect;
    const bool newCursorRectIntersectsClipRect = cursorRectIntersectsClipRect != d->cursorRectIntersectsClipRect;
    const bool newSelectionControlVisible = selectionControlVisible != d->selectionControlVisible;

    d->anchorRectangle = anchorRectangle;
    d->cursorRectangle = cursorRectangle;
    d->anchorRectIntersectsClipRect = anchorRectIntersectsClipRect;
    d->cursorRectIntersectsClipRect = cursorRectIntersectsClipRect;
    d->selectionControlVisible = selectionControlVisible;

    if (newAnchorRectangle)
        emit anchorRectangleChanged();
    if (newCursorRectangle)
        emit cursorRectangleChanged();
    if (newAnchorRectIntersectsClipRect)
        emit anchorRectIntersectsClipRectChanged();
    if (newCursorRectIntersectsClipRect)
        emit cursorRectIntersectsClipRectChanged();
    if (newSelectionControlVisible)
        emit selectionControlVisibleChanged();
}

void ShadowInputContext::update(Qt::InputMethodQueries queries)
{
    Q_UNUSED(queries)
    Q_D(ShadowInputContext);
    if (!d->inputItem)
        return;

    QInputMethodQueryEvent imQueryEvent(Qt::ImQueryInput);
    QGuiApplication::sendEvent(d->inputItem, &imQueryEvent);

    const QString surroundingText = imQueryEvent.value(Qt::ImSurroundingText).toString();
    const int cursorPosition = imQueryEvent.value(Qt::ImCursorPosition).toInt();
    const int anchorPosition = imQueryEvent.value(Qt::ImAnchorPosition).toInt();

    const QString newSurroundingText = d->inputContext->surroundingText();
    const int newCursorPosition = d->inputContext->cursorPosition();
    const int newAnchorPosition = d->inputContext->anchorPosition();

    bool updateSurroundingText = newSurroundingText != surroundingText;
    bool updateSelection = newCursorPosition != cursorPosition || newAnchorPosition != anchorPosition;
    if (updateSurroundingText || updateSelection) {
        QList<QInputMethodEvent::Attribute> attributes;
        attributes.append(QInputMethodEvent::Attribute(QInputMethodEvent::Selection,
                                                       newAnchorPosition,
                                                       newCursorPosition - newAnchorPosition, QVariant()));
        QInputMethodEvent inputEvent(QString(), attributes);
        if (updateSurroundingText)
            inputEvent.setCommitString(newSurroundingText, -cursorPosition, surroundingText.length());
        QGuiApplication::sendEvent(d->inputItem, &inputEvent);
    }

    const QString newPreeditText = d->inputContext->preeditText();
    const QList<QInputMethodEvent::Attribute> newPreeditAttributes = d->inputContext->preeditTextAttributes();
    if (d->preeditText != newPreeditText || d->preeditTextAttributes != newPreeditAttributes) {
        d->preeditText = newPreeditText;
        d->preeditTextAttributes = newPreeditAttributes;
        QInputMethodEvent inputEvent(d->preeditText, d->preeditTextAttributes);
        QGuiApplication::sendEvent(d->inputItem, &inputEvent);
    }

    updateSelectionProperties();
}

QVariant ShadowInputContext::queryFocusObject(Qt::InputMethodQuery query, QVariant argument)
{
    Q_D(ShadowInputContext);
    QVariant retval;
    QObject *focusObject = d->inputItem;
    if (!focusObject)
        return retval;

    bool newMethodWorks = QMetaObject::invokeMethod(focusObject, "inputMethodQuery",
                                                    Qt::DirectConnection,
                                                    Q_RETURN_ARG(QVariant, retval),
                                                    Q_ARG(Qt::InputMethodQuery, query),
                                                    Q_ARG(QVariant, argument));
    if (newMethodWorks)
        return retval;

    QInputMethodQueryEvent queryEvent(query);
    QCoreApplication::sendEvent(focusObject, &queryEvent);
    return queryEvent.value(query);
}

} // namespace QtVirtualKeyboard

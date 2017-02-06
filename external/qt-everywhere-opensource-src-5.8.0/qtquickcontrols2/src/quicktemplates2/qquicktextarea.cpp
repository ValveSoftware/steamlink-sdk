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

#include "qquicktextarea_p.h"
#include "qquicktextarea_p_p.h"
#include "qquickcontrol_p.h"
#include "qquickcontrol_p_p.h"

#include <QtGui/qguiapplication.h>
#include <QtQml/qqmlinfo.h>
#include <QtQuick/private/qquickitem_p.h>
#include <QtQuick/private/qquicktext_p.h>
#include <QtQuick/private/qquickclipnode_p.h>
#include <QtQuick/private/qquickflickable_p.h>

#ifndef QT_NO_ACCESSIBILITY
#include <QtQuick/private/qquickaccessibleattached_p.h>
#endif

QT_BEGIN_NAMESPACE

/*!
    \qmltype TextArea
    \inherits TextEdit
    \instantiates QQuickTextArea
    \inqmlmodule QtQuick.Controls
    \since 5.7
    \ingroup qtquickcontrols2-input
    \brief Multi-line text input area.

    TextArea is a multi-line text editor. TextArea extends TextEdit with
    a \l {placeholderText}{placeholder text} functionality, and adds decoration.

    \image qtquickcontrols2-textarea.png

    \code
    TextArea {
        placeholderText: qsTr("Enter description")
    }
    \endcode

    TextArea is not scrollable by itself. Especially on screen-size constrained
    platforms, it is often preferable to make entire application pages scrollable.
    On such a scrollable page, a non-scrollable TextArea might behave better than
    nested scrollable controls. Notice, however, that in such a scenario, the background
    decoration of the TextArea scrolls together with the rest of the scrollable
    content.

    \section2 Scrollable TextArea

    If you want to make a TextArea scrollable, for example, when it covers
    an entire application page, attach it to a \l Flickable and combine with a
    \l ScrollBar or \l ScrollIndicator.

    \image qtquickcontrols2-textarea-flickable.png

    \snippet qtquickcontrols2-textarea-flickable.qml 1

    A TextArea that is attached to a \l Flickable does the following:

    \list
    \li Sets the content size automatically
    \li Ensures that the background decoration stays in place
    \li Clips the content
    \endlist

    \sa TextField, {Customizing TextArea}, {Input Controls}
*/

/*!
    \qmlsignal QtQuick.Controls::TextArea::pressAndHold(MouseEvent event)

    This signal is emitted when there is a long press (the delay depends on the platform plugin).
    The \l {MouseEvent}{event} parameter provides information about the press, including the x and y
    position of the press, and which button is pressed.

    \sa pressed, released
*/

/*!
    \qmlsignal QtQuick.Controls::TextArea::pressed(MouseEvent event)
    \since QtQuick.Controls 2.1

    This signal is emitted when the text area is pressed by the user.
    The \l {MouseEvent}{event} parameter provides information about the press,
    including the x and y position of the press, and which button is pressed.

    \sa released, pressAndHold
*/

/*!
    \qmlsignal QtQuick.Controls::TextArea::released(MouseEvent event)
    \since QtQuick.Controls 2.1

    This signal is emitted when the text area is released by the user.
    The \l {MouseEvent}{event} parameter provides information about the release,
    including the x and y position of the press, and which button is pressed.

    \sa pressed, pressAndHold
*/

QQuickTextAreaPrivate::QQuickTextAreaPrivate()
    : hovered(false), explicitHoverEnabled(false), background(nullptr),
      focusReason(Qt::OtherFocusReason), accessibleAttached(nullptr), flickable(nullptr)
{
#ifndef QT_NO_ACCESSIBILITY
    QAccessible::installActivationObserver(this);
#endif
}

QQuickTextAreaPrivate::~QQuickTextAreaPrivate()
{
#ifndef QT_NO_ACCESSIBILITY
    QAccessible::removeActivationObserver(this);
#endif
}

void QQuickTextAreaPrivate::resizeBackground()
{
    Q_Q(QQuickTextArea);
    if (background) {
        QQuickItemPrivate *p = QQuickItemPrivate::get(background);
        if (!p->widthValid && qFuzzyIsNull(background->x())) {
            if (flickable)
                background->setWidth(flickable->width());
            else
                background->setWidth(q->width());
            p->widthValid = false;
        }
        if (!p->heightValid && qFuzzyIsNull(background->y())) {
            if (flickable)
                background->setHeight(flickable->height());
            else
                background->setHeight(q->height());
            p->heightValid = false;
        }
    }
}

void QQuickTextAreaPrivate::attachFlickable(QQuickFlickable *item)
{
    Q_Q(QQuickTextArea);
    flickable = item;
    q->setParentItem(flickable->contentItem());

    if (background)
        background->setParentItem(flickable);

    QObjectPrivate::connect(q, &QQuickTextArea::contentSizeChanged, this, &QQuickTextAreaPrivate::resizeFlickableContent);
    QObjectPrivate::connect(q, &QQuickTextEdit::cursorRectangleChanged, this, &QQuickTextAreaPrivate::ensureCursorVisible);

    QObject::connect(flickable, &QQuickFlickable::contentXChanged, q, &QQuickItem::update);
    QObject::connect(flickable, &QQuickFlickable::contentYChanged, q, &QQuickItem::update);

    QQuickItemPrivate::get(flickable)->updateOrAddGeometryChangeListener(this, QQuickGeometryChange::Size);
    QObjectPrivate::connect(flickable, &QQuickFlickable::contentWidthChanged, this, &QQuickTextAreaPrivate::resizeFlickableControl);
    QObjectPrivate::connect(flickable, &QQuickFlickable::contentHeightChanged, this, &QQuickTextAreaPrivate::resizeFlickableControl);

    resizeFlickableControl();
}

void QQuickTextAreaPrivate::detachFlickable()
{
    Q_Q(QQuickTextArea);
    q->setParentItem(nullptr);
    if (background && background->parentItem() == flickable)
        background->setParentItem(q);

    QObjectPrivate::disconnect(q, &QQuickTextArea::contentSizeChanged, this, &QQuickTextAreaPrivate::resizeFlickableContent);
    QObjectPrivate::disconnect(q, &QQuickTextEdit::cursorRectangleChanged, this, &QQuickTextAreaPrivate::ensureCursorVisible);

    QObject::disconnect(flickable, &QQuickFlickable::contentXChanged, q, &QQuickItem::update);
    QObject::disconnect(flickable, &QQuickFlickable::contentYChanged, q, &QQuickItem::update);

    QQuickItemPrivate::get(flickable)->updateOrRemoveGeometryChangeListener(this, QQuickGeometryChange::Size);
    QObjectPrivate::disconnect(flickable, &QQuickFlickable::contentWidthChanged, this, &QQuickTextAreaPrivate::resizeFlickableControl);
    QObjectPrivate::disconnect(flickable, &QQuickFlickable::contentHeightChanged, this, &QQuickTextAreaPrivate::resizeFlickableControl);

    flickable = nullptr;
}

void QQuickTextAreaPrivate::ensureCursorVisible()
{
    Q_Q(QQuickTextArea);
    if (!flickable)
        return;

    const qreal cx = flickable->contentX();
    const qreal cy = flickable->contentY();
    const qreal w = flickable->width();
    const qreal h = flickable->height();

    const qreal tp = q->topPadding();
    const qreal lp = q->leftPadding();
    const QRectF cr = q->cursorRectangle();

    if (cr.left() <= cx + lp) {
        flickable->setContentX(cr.left() - lp);
    } else {
        // calculate the rectangle of the next character and ensure that
        // it's visible if it's on the same line with the cursor
        const qreal rp = q->rightPadding();
        const QRectF nr = q->cursorPosition() < q->length() ? q->positionToRectangle(q->cursorPosition() + 1) : QRectF();
        if (qFuzzyCompare(nr.y(), cr.y()) && nr.right() >= cx + lp + w - rp)
            flickable->setContentX(nr.right() - w + rp);
        else if (cr.right() >= cx + lp + w - rp)
            flickable->setContentX(cr.right() - w + rp);
    }

    if (cr.top() <= cy + tp) {
        flickable->setContentY(cr.top() - tp);
    } else {
        const qreal bp = q->bottomPadding();
        if (cr.bottom() >= cy + tp + h - bp)
            flickable->setContentY(cr.bottom() - h + bp);
    }
}

void QQuickTextAreaPrivate::resizeFlickableControl()
{
    Q_Q(QQuickTextArea);
    if (!flickable)
        return;

    const qreal w = wrapMode == QQuickTextArea::NoWrap ? qMax(flickable->width(), flickable->contentWidth()) : flickable->width();
    const qreal h = qMax(flickable->height(), flickable->contentHeight());
    q->setSize(QSizeF(w, h));

    resizeBackground();
}

void QQuickTextAreaPrivate::resizeFlickableContent()
{
    Q_Q(QQuickTextArea);
    if (!flickable)
        return;

    flickable->setContentWidth(q->contentWidth() + q->leftPadding() + q->rightPadding());
    flickable->setContentHeight(q->contentHeight() + q->topPadding() + q->bottomPadding());
}

void QQuickTextAreaPrivate::itemGeometryChanged(QQuickItem *item, QQuickGeometryChange change, const QRectF &diff)
{
    Q_UNUSED(item);
    Q_UNUSED(change);
    Q_UNUSED(diff);

    resizeFlickableControl();
}

qreal QQuickTextAreaPrivate::getImplicitWidth() const
{
    return QQuickItemPrivate::getImplicitWidth();
}

qreal QQuickTextAreaPrivate::getImplicitHeight() const
{
    return QQuickItemPrivate::getImplicitHeight();
}

void QQuickTextAreaPrivate::implicitWidthChanged()
{
    Q_Q(QQuickTextArea);
    QQuickItemPrivate::implicitWidthChanged();
    emit q->implicitWidthChanged3();
}

void QQuickTextAreaPrivate::implicitHeightChanged()
{
    Q_Q(QQuickTextArea);
    QQuickItemPrivate::implicitHeightChanged();
    emit q->implicitHeightChanged3();
}

QQuickTextArea::QQuickTextArea(QQuickItem *parent) :
    QQuickTextEdit(*(new QQuickTextAreaPrivate), parent)
{
    Q_D(QQuickTextArea);
    setActiveFocusOnTab(true);
    setAcceptedMouseButtons(Qt::AllButtons);
    d->setImplicitResizeEnabled(false);
    d->pressHandler.control = this;
#ifndef QT_NO_CURSOR
    setCursor(Qt::IBeamCursor);
#endif
    QObjectPrivate::connect(this, &QQuickTextEdit::readOnlyChanged,
                            d, &QQuickTextAreaPrivate::_q_readOnlyChanged);
}

QQuickTextArea::~QQuickTextArea()
{
}

QQuickTextAreaAttached *QQuickTextArea::qmlAttachedProperties(QObject *object)
{
    return new QQuickTextAreaAttached(object);
}

/*!
    \internal

    Determine which font is implicitly imposed on this control by its ancestors
    and QGuiApplication::font, resolve this against its own font (attributes from
    the implicit font are copied over). Then propagate this font to this
    control's children.
*/
void QQuickTextAreaPrivate::resolveFont()
{
    Q_Q(QQuickTextArea);
    inheritFont(QQuickControlPrivate::parentFont(q));
}

void QQuickTextAreaPrivate::inheritFont(const QFont &f)
{
    Q_Q(QQuickTextArea);
    QFont parentFont = font.resolve(f);
    parentFont.resolve(font.resolve() | f.resolve());

    const QFont defaultFont = QQuickControlPrivate::themeFont(QPlatformTheme::EditorFont);
    const QFont resolvedFont = parentFont.resolve(defaultFont);

    const bool changed = resolvedFont != sourceFont;
    q->QQuickTextEdit::setFont(resolvedFont);
    if (changed)
        emit q->fontChanged();
}

void QQuickTextAreaPrivate::updateHoverEnabled(bool enabled, bool xplicit)
{
    Q_Q(QQuickTextArea);
    if (!xplicit && explicitHoverEnabled)
        return;

    bool wasEnabled = q->isHoverEnabled();
    explicitHoverEnabled = xplicit;
    if (wasEnabled != enabled) {
        q->setAcceptHoverEvents(enabled);
        QQuickControlPrivate::updateHoverEnabledRecur(q, enabled);
        emit q->hoverEnabledChanged();
    }
}

void QQuickTextAreaPrivate::_q_readOnlyChanged(bool isReadOnly)
{
#ifndef QT_NO_ACCESSIBILITY
    if (accessibleAttached)
        accessibleAttached->set_readOnly(isReadOnly);
#else
    Q_UNUSED(isReadOnly)
#endif
}

#ifndef QT_NO_ACCESSIBILITY
void QQuickTextAreaPrivate::accessibilityActiveChanged(bool active)
{
    if (accessibleAttached || !active)
        return;

    Q_Q(QQuickTextArea);
    accessibleAttached = qobject_cast<QQuickAccessibleAttached *>(qmlAttachedPropertiesObject<QQuickAccessibleAttached>(q, true));
    if (accessibleAttached) {
        accessibleAttached->setRole(accessibleRole());
        accessibleAttached->set_readOnly(q->isReadOnly());
        accessibleAttached->setDescription(placeholder);
    } else {
        qWarning() << "QQuickTextArea: " << q << " QQuickAccessibleAttached object creation failed!";
    }
}

QAccessible::Role QQuickTextAreaPrivate::accessibleRole() const
{
    return QAccessible::EditableText;
}
#endif

void QQuickTextAreaPrivate::deleteDelegate(QObject *delegate)
{
    if (componentComplete)
        delete delegate;
    else
        pendingDeletions.append(delegate);
}

QFont QQuickTextArea::font() const
{
    return QQuickTextEdit::font();
}

void QQuickTextArea::setFont(const QFont &font)
{
    Q_D(QQuickTextArea);
    if (d->font.resolve() == font.resolve() && d->font == font)
        return;

    d->font = font;
    d->resolveFont();
}

/*!
    \qmlproperty Item QtQuick.Controls::TextArea::background

    This property holds the background item.

    \input qquickcontrol-background.qdocinc notes

    \sa {Customizing TextArea}
*/
QQuickItem *QQuickTextArea::background() const
{
    Q_D(const QQuickTextArea);
    return d->background;
}

void QQuickTextArea::setBackground(QQuickItem *background)
{
    Q_D(QQuickTextArea);
    if (d->background == background)
        return;

    d->deleteDelegate(d->background);
    d->background = background;
    if (background) {
        background->setParentItem(this);
        if (qFuzzyIsNull(background->z()))
            background->setZ(-1);
        if (isComponentComplete())
            d->resizeBackground();
    }
    emit backgroundChanged();
}

/*!
    \qmlproperty string QtQuick.Controls::TextArea::placeholderText

    This property holds the short hint that is displayed in the text area before
    the user enters a value.
*/
QString QQuickTextArea::placeholderText() const
{
    Q_D(const QQuickTextArea);
    return d->placeholder;
}

void QQuickTextArea::setPlaceholderText(const QString &text)
{
    Q_D(QQuickTextArea);
    if (d->placeholder == text)
        return;

    d->placeholder = text;
#ifndef QT_NO_ACCESSIBILITY
    if (d->accessibleAttached)
        d->accessibleAttached->setDescription(text);
#endif
    emit placeholderTextChanged();
}

/*!
    \qmlproperty enumeration QtQuick.Controls::TextArea::focusReason

    \include qquickcontrol-focusreason.qdocinc
*/
Qt::FocusReason QQuickTextArea::focusReason() const
{
    Q_D(const QQuickTextArea);
    return d->focusReason;
}

void QQuickTextArea::setFocusReason(Qt::FocusReason reason)
{
    Q_D(QQuickTextArea);
    if (d->focusReason == reason)
        return;

    d->focusReason = reason;
    emit focusReasonChanged();
}

/*!
    \since QtQuick.Controls 2.1
    \qmlproperty bool QtQuick.Controls::TextArea::hovered
    \readonly

    This property holds whether the text area is hovered.

    \sa hoverEnabled
*/
bool QQuickTextArea::isHovered() const
{
    Q_D(const QQuickTextArea);
    return d->hovered;
}

void QQuickTextArea::setHovered(bool hovered)
{
    Q_D(QQuickTextArea);
    if (hovered == d->hovered)
        return;

    d->hovered = hovered;
    emit hoveredChanged();
}

/*!
    \since QtQuick.Controls 2.1
    \qmlproperty bool QtQuick.Controls::TextArea::hoverEnabled

    This property determines whether the text area accepts hover events. The default value is \c true.

    \sa hovered
*/
bool QQuickTextArea::isHoverEnabled() const
{
    Q_D(const QQuickTextArea);
    return d->hoverEnabled;
}

void QQuickTextArea::setHoverEnabled(bool enabled)
{
    Q_D(QQuickTextArea);
    if (d->explicitHoverEnabled && enabled == d->hoverEnabled)
        return;

    d->updateHoverEnabled(enabled, true); // explicit=true
}

void QQuickTextArea::resetHoverEnabled()
{
    Q_D(QQuickTextArea);
    if (!d->explicitHoverEnabled)
        return;

    d->explicitHoverEnabled = false;
    d->updateHoverEnabled(QQuickControlPrivate::calcHoverEnabled(d->parentItem), false); // explicit=false
}

bool QQuickTextArea::contains(const QPointF &point) const
{
    Q_D(const QQuickTextArea);
    if (d->flickable && !d->flickable->contains(d->flickable->mapFromItem(this, point)))
        return false;
    return QQuickTextEdit::contains(point);
}

void QQuickTextArea::classBegin()
{
    Q_D(QQuickTextArea);
    QQuickTextEdit::classBegin();
    d->resolveFont();
}

void QQuickTextArea::componentComplete()
{
    Q_D(QQuickTextArea);
    QQuickTextEdit::componentComplete();
    if (!d->explicitHoverEnabled)
        setAcceptHoverEvents(QQuickControlPrivate::calcHoverEnabled(d->parentItem));
#ifndef QT_NO_ACCESSIBILITY
    if (!d->accessibleAttached && QAccessible::isActive())
        d->accessibilityActiveChanged(true);
#endif

    qDeleteAll(d->pendingDeletions);
    d->pendingDeletions.clear();
}

void QQuickTextArea::itemChange(QQuickItem::ItemChange change, const QQuickItem::ItemChangeData &value)
{
    Q_D(QQuickTextArea);
    QQuickTextEdit::itemChange(change, value);
    if (change == ItemParentHasChanged && value.item) {
        d->resolveFont();
        if (!d->explicitHoverEnabled)
            d->updateHoverEnabled(QQuickControlPrivate::calcHoverEnabled(d->parentItem), false); // explicit=false
    }
}

void QQuickTextArea::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    Q_D(QQuickTextArea);
    QQuickTextEdit::geometryChanged(newGeometry, oldGeometry);
    d->resizeBackground();
}

QSGNode *QQuickTextArea::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data)
{
    Q_D(QQuickTextArea);
    QQuickDefaultClipNode *clipNode = static_cast<QQuickDefaultClipNode *>(oldNode);
    if (!clipNode)
        clipNode = new QQuickDefaultClipNode(QRectF());

    QQuickItem *clipper = this;
    if (d->flickable)
        clipper = d->flickable;

    const QRectF cr = clipper->clipRect().adjusted(leftPadding(), topPadding(), -rightPadding(), -bottomPadding());
    clipNode->setRect(!d->flickable ? cr : cr.translated(d->flickable->contentX(), d->flickable->contentY()));
    clipNode->update();

    QSGNode *textNode = QQuickTextEdit::updatePaintNode(clipNode->firstChild(), data);
    if (!textNode->parent())
        clipNode->appendChildNode(textNode);

    if (d->cursorItem) {
        QQuickDefaultClipNode *cursorNode = QQuickItemPrivate::get(d->cursorItem)->clipNode();
        if (cursorNode)
            cursorNode->setClipRect(d->cursorItem->mapRectFromItem(clipper, cr));
    }

    return clipNode;
}

void QQuickTextArea::focusInEvent(QFocusEvent *event)
{
    QQuickTextEdit::focusInEvent(event);
    setFocusReason(event->reason());
}

void QQuickTextArea::focusOutEvent(QFocusEvent *event)
{
    QQuickTextEdit::focusOutEvent(event);
    setFocusReason(event->reason());
}

void QQuickTextArea::hoverEnterEvent(QHoverEvent *event)
{
    Q_D(QQuickTextArea);
    QQuickTextEdit::hoverEnterEvent(event);
    setHovered(d->hoverEnabled);
    event->setAccepted(d->hoverEnabled);
}

void QQuickTextArea::hoverLeaveEvent(QHoverEvent *event)
{
    Q_D(QQuickTextArea);
    QQuickTextEdit::hoverLeaveEvent(event);
    setHovered(false);
    event->setAccepted(d->hoverEnabled);
}

void QQuickTextArea::mousePressEvent(QMouseEvent *event)
{
    Q_D(QQuickTextArea);
    d->pressHandler.mousePressEvent(event);
    if (d->pressHandler.isActive()) {
        if (d->pressHandler.delayedMousePressEvent) {
            QQuickTextEdit::mousePressEvent(d->pressHandler.delayedMousePressEvent);
            d->pressHandler.clearDelayedMouseEvent();
        }
        // Calling the base class implementation will result in QQuickTextControl's
        // press handler being called, which ignores events that aren't Qt::LeftButton.
        const bool wasAccepted = event->isAccepted();
        QQuickTextEdit::mousePressEvent(event);
        if (wasAccepted)
            event->accept();
    }
}

void QQuickTextArea::mouseMoveEvent(QMouseEvent *event)
{
    Q_D(QQuickTextArea);
    d->pressHandler.mouseMoveEvent(event);
    if (d->pressHandler.isActive()) {
        if (d->pressHandler.delayedMousePressEvent) {
            QQuickTextEdit::mousePressEvent(d->pressHandler.delayedMousePressEvent);
            d->pressHandler.clearDelayedMouseEvent();
        }
        QQuickTextEdit::mouseMoveEvent(event);
    }
}

void QQuickTextArea::mouseReleaseEvent(QMouseEvent *event)
{
    Q_D(QQuickTextArea);
    d->pressHandler.mouseReleaseEvent(event);
    if (d->pressHandler.isActive()) {
        if (d->pressHandler.delayedMousePressEvent) {
            QQuickTextEdit::mousePressEvent(d->pressHandler.delayedMousePressEvent);
            d->pressHandler.clearDelayedMouseEvent();
        }
        QQuickTextEdit::mouseReleaseEvent(event);
    }
}

void QQuickTextArea::mouseDoubleClickEvent(QMouseEvent *event)
{
    Q_D(QQuickTextArea);
    if (d->pressHandler.delayedMousePressEvent) {
        QQuickTextEdit::mousePressEvent(d->pressHandler.delayedMousePressEvent);
        d->pressHandler.clearDelayedMouseEvent();
    }
    QQuickTextEdit::mouseDoubleClickEvent(event);
}

void QQuickTextArea::timerEvent(QTimerEvent *event)
{
    Q_D(QQuickTextArea);
    if (event->timerId() == d->pressHandler.timer.timerId()) {
        d->pressHandler.timerEvent(event);
    } else {
        QQuickTextEdit::timerEvent(event);
    }
}

class QQuickTextAreaAttachedPrivate : public QObjectPrivate
{
public:
    QQuickTextAreaAttachedPrivate() : control(nullptr) { }

    QQuickTextArea *control;
};

QQuickTextAreaAttached::QQuickTextAreaAttached(QObject *parent) :
    QObject(*(new QQuickTextAreaAttachedPrivate), parent)
{
}

QQuickTextAreaAttached::~QQuickTextAreaAttached()
{
}

/*!
    \qmlattachedproperty TextArea QtQuick.Controls::TextArea::flickable

    This property attaches a text area to a \l Flickable.

    \sa ScrollBar, ScrollIndicator, {Scrollable TextArea}
*/
QQuickTextArea *QQuickTextAreaAttached::flickable() const
{
    Q_D(const QQuickTextAreaAttached);
    return d->control;
}

void QQuickTextAreaAttached::setFlickable(QQuickTextArea *control)
{
    Q_D(QQuickTextAreaAttached);
    QQuickFlickable *flickable = qobject_cast<QQuickFlickable *>(parent());
    if (!flickable) {
        qmlInfo(parent()) << "TextArea must be attached to a Flickable";
        return;
    }

    if (d->control == control)
        return;

    if (d->control)
        QQuickTextAreaPrivate::get(d->control)->detachFlickable();

    d->control = control;

    if (control)
        QQuickTextAreaPrivate::get(control)->attachFlickable(flickable);

    emit flickableChanged();
}

QT_END_NAMESPACE

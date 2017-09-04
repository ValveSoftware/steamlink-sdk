/****************************************************************************
 **
 ** Copyright (C) 2016 Ivan Vizir <define-true-false@yandex.com>
 ** Copyright (C) 2016 The Qt Company Ltd.
 ** Contact: https://www.qt.io/licensing/
 **
 ** This file is part of the QtWinExtras module of the Qt Toolkit.
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

#include "qquickdwmfeatures_p.h"
#include "qquickdwmfeatures_p_p.h"

#include <QtWinExtras/private/qwineventfilter_p.h>
#include <QWinEvent>
#include <QQuickWindow>

QT_BEGIN_NAMESPACE

/*!
    \qmltype DwmFeatures
    \instantiates QQuickDwmFeatures
    \inqmlmodule QtWinExtras

    \brief Enables you to manage the Windows DWM features.

    \since QtWinExtras 1.0

    The DwmFeatures type enables you to extend a glass frame into the client
    area, as well as to control the behavior of Aero Peek and Flip3D.
 */

/*!
    \class QQuickDwmFeatures
    \internal
 */

QQuickDwmFeatures::QQuickDwmFeatures(QQuickItem *parent) :
    QQuickItem(parent), d_ptr(new QQuickDwmFeaturesPrivate(this))
{
    QWinEventFilter::setup();
}

QQuickDwmFeatures::~QQuickDwmFeatures()
{
}

void QQuickDwmFeatures::setCompositionEnabled(bool enabled)
{
    QtWin::setCompositionEnabled(enabled);
}

bool QQuickDwmFeatures::isCompositionEnabled() const
{
    return QtWin::isCompositionEnabled();
}

QColor QQuickDwmFeatures::colorizationColor() const
{
    return QtWin::colorizationColor();
}

QColor QQuickDwmFeatures::realColorizationColor() const
{
    return QtWin::realColorizationColor();
}

bool QQuickDwmFeatures::colorizationOpaqueBlend() const
{
    bool opaque;
    QtWin::colorizationColor(&opaque);
    return opaque;
}

/*!
    \qmlproperty int DwmFeatures::topGlassMargin

    The top glass frame margin. The default value is 0.
 */
void QQuickDwmFeatures::setTopGlassMargin(int margin)
{
    Q_D(QQuickDwmFeatures);
    if (d->topMargin == margin)
        return;

    d->topMargin = margin;
    if (window())
        QtWin::extendFrameIntoClientArea(window(), d->leftMargin, d->topMargin, d->rightMargin, d->bottomMargin);
    emit topGlassMarginChanged();
}

/*!
    \qmlproperty int DwmFeatures::rightGlassMargin

    The right glass frame margin. The default value is 0.
 */
void QQuickDwmFeatures::setRightGlassMargin(int margin)
{
    Q_D(QQuickDwmFeatures);
    if (d->rightMargin == margin)
        return;

    d->rightMargin = margin;
    if (window())
        QtWin::extendFrameIntoClientArea(window(), d->leftMargin, d->topMargin, d->rightMargin, d->bottomMargin);
    emit rightGlassMarginChanged();
}

/*!
    \qmlproperty int DwmFeatures::bottomGlassMargin

    The bottom glass frame margin. The default value is 0.
 */
void QQuickDwmFeatures::setBottomGlassMargin(int margin)
{
    Q_D(QQuickDwmFeatures);
    if (d->bottomMargin == margin)
        return;

    d->bottomMargin = margin;
    if (window())
        QtWin::extendFrameIntoClientArea(window(), d->leftMargin, d->topMargin, d->rightMargin, d->bottomMargin);
    emit bottomGlassMarginChanged();
}

/*!
    \qmlproperty int DwmFeatures::leftGlassMargin

    The left glass frame margin. The default value is 0.
 */
void QQuickDwmFeatures::setLeftGlassMargin(int margin)
{
    Q_D(QQuickDwmFeatures);
    if (d->leftMargin == margin)
        return;

    d->leftMargin = margin;
    if (window())
        QtWin::extendFrameIntoClientArea(window(), d->leftMargin, d->topMargin, d->rightMargin, d->bottomMargin);
    emit leftGlassMarginChanged();
}

int QQuickDwmFeatures::topGlassMargin() const
{
    Q_D(const QQuickDwmFeatures);
    return d->topMargin;
}

int QQuickDwmFeatures::rightGlassMargin() const
{
    Q_D(const QQuickDwmFeatures);
    return d->rightMargin;
}

int QQuickDwmFeatures::bottomGlassMargin() const
{
    Q_D(const QQuickDwmFeatures);
    return d->bottomMargin;
}

int QQuickDwmFeatures::leftGlassMargin() const
{
    Q_D(const QQuickDwmFeatures);
    return d->leftMargin;
}

/*!
    \qmlproperty bool DwmFeatures::blurBehindEnabled

    Specifies whether the blur behind the window client area is enabled.
 */
bool QQuickDwmFeatures::isBlurBehindEnabled() const
{
    Q_D(const QQuickDwmFeatures);
    return d->blurBehindEnabled;
}

void QQuickDwmFeatures::setBlurBehindEnabled(bool enabled)
{
    Q_D(QQuickDwmFeatures);
    if (d->blurBehindEnabled == enabled)
        return;

    d->blurBehindEnabled = enabled;
    if (window()) {
        if (d->blurBehindEnabled)
            QtWin::enableBlurBehindWindow(window());
        else
            QtWin::disableBlurBehindWindow(window());
    }
    emit blurBehindEnabledChanged();
}

/*!
    \qmlproperty bool DwmFeatures::excludedFromPeek

    Specifies whether the window is excluded from Aero Peek.
    The default value is false.
 */
bool QQuickDwmFeatures::isExcludedFromPeek() const
{
    Q_D(const QQuickDwmFeatures);
    if (window())
        return QtWin::isWindowExcludedFromPeek(window());
    else
        return d->peekExcluded;
}

void QQuickDwmFeatures::setExcludedFromPeek(bool exclude)
{
    Q_D(QQuickDwmFeatures);
    if (d->peekExcluded == exclude)
        return;

    d->peekExcluded = exclude;
    if (window())
        QtWin::setWindowExcludedFromPeek(window(), d->peekExcluded);
    emit excludedFromPeekChanged();
}

/*!
    \qmlproperty bool DwmFeatures::peekDisallowed

    Set this value to true if you want to forbid Aero Peek when the user hovers
    the mouse over the window thumbnail. The default value is false.
*/
bool QQuickDwmFeatures::isPeekDisallowed() const
{
    Q_D(const QQuickDwmFeatures);
    if (window())
        return QtWin::isWindowPeekDisallowed(window());
    else
        return d->peekDisallowed;
}

void QQuickDwmFeatures::setPeekDisallowed(bool disallow)
{
    Q_D(QQuickDwmFeatures);
    if (d->peekDisallowed == disallow)
        return;

    d->peekDisallowed = disallow;
    if (window())
        QtWin::setWindowDisallowPeek(window(), d->peekDisallowed);
    emit peekDisallowedChanged();
}

/*!
    \qmlproperty QtWin::WindowFlip3DPolicy DwmFeatures::flip3DPolicy

    The current Flip3D policy for the window.
 */
QQuickWin::WindowFlip3DPolicy QQuickDwmFeatures::flip3DPolicy() const
{
    Q_D(const QQuickDwmFeatures);
    if (window())
        return static_cast<QQuickWin::WindowFlip3DPolicy>(QtWin::windowFlip3DPolicy(window()));
    else
        return d->flipPolicy;
}

void QQuickDwmFeatures::setFlip3DPolicy(QQuickWin::WindowFlip3DPolicy policy)
{
    Q_D(QQuickDwmFeatures);
    if (d->flipPolicy == policy)
        return;

    d->flipPolicy = policy;
    if (window())
        QtWin::setWindowFlip3DPolicy(window(), static_cast<QtWin::WindowFlip3DPolicy>(d->flipPolicy));
    emit flip3DPolicyChanged();
}

bool QQuickDwmFeatures::eventFilter(QObject *object, QEvent *event)
{
    Q_D(QQuickDwmFeatures);
    if (object == window()) {
        if (event->type() == QWinEvent::CompositionChange) {
            d->updateSurfaceFormat();
            if (static_cast<QWinCompositionChangeEvent *>(event)->isCompositionEnabled())
                d->updateAll();
            emit compositionEnabledChanged();
        } else if (event->type() == QWinEvent::ColorizationChange) {
            emit colorizationColorChanged();
            emit realColorizationColorChanged();
            emit colorizationOpaqueBlendChanged();
        }
    }
    return QQuickItem::eventFilter(object, event);
}

QQuickDwmFeatures *QQuickDwmFeatures::qmlAttachedProperties(QObject *parentObject)
{
    QQuickDwmFeatures *featuresObj = new QQuickDwmFeatures();
    QQuickItem *parentItem = qobject_cast<QQuickItem *>(parentObject);
    if (parentItem)
        featuresObj->setParentItem(parentItem);
    else
        featuresObj->setParent(parentObject);
    return featuresObj;
}

void QQuickDwmFeatures::itemChange(QQuickItem::ItemChange change, const QQuickItem::ItemChangeData &data)
{
    Q_D(QQuickDwmFeatures);
    if (change == ItemSceneChange && data.window) {
        d->updateAll();
        data.window->installEventFilter(this);
        d->originalSurfaceColor = data.window->color();
    }
    QQuickItem::itemChange(change, data);
}



QQuickDwmFeaturesPrivate::QQuickDwmFeaturesPrivate(QQuickDwmFeatures *parent) :
    topMargin(0), rightMargin(0), bottomMargin(0), leftMargin(0),
    blurBehindEnabled(false),
    peekDisallowed(false), peekExcluded(false), flipPolicy(QQuickWin::FlipDefault),
    q_ptr(parent)
{
}

void QQuickDwmFeaturesPrivate::updateAll()
{
    Q_Q(QQuickDwmFeatures);
    QWindow *w = q->window();
    if (w) {
        updateSurfaceFormat();
        QtWin::setWindowExcludedFromPeek(w, peekExcluded);
        QtWin::setWindowDisallowPeek(w, peekDisallowed);
        QtWin::setWindowFlip3DPolicy(w, static_cast<QtWin::WindowFlip3DPolicy>(flipPolicy));
        if (blurBehindEnabled)
            QtWin::enableBlurBehindWindow(w);
        else
            QtWin::disableBlurBehindWindow(w);
        QtWin::extendFrameIntoClientArea(w, leftMargin, topMargin, rightMargin, bottomMargin);
    }
}

void QQuickDwmFeaturesPrivate::updateSurfaceFormat()
{
    Q_Q(QQuickDwmFeatures);
    if (q->window()) {
        const bool compositionEnabled = q->isCompositionEnabled();
        QSurfaceFormat format = q->window()->format();
        format.setAlphaBufferSize(compositionEnabled ? 8 : 0);
        q->window()->setFormat(format);
        q->window()->setColor(compositionEnabled ? QColor(Qt::transparent) : originalSurfaceColor);
    }
}

QT_END_NAMESPACE

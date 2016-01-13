/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQuick.Dialogs module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qquickabstractdialog_p.h"
#include "qquickitem.h"

#include <private/qguiapplication_p.h>
#include <private/qqmlglobal_p.h>
#include <QLoggingCategory>
#include <QWindow>
#include <QQmlComponent>
#include <QQuickWindow>
#include <qpa/qplatformintegration.h>

QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(lcWindow, "qt.quick.dialogs.window")

QQmlComponent *QQuickAbstractDialog::m_decorationComponent(0);

QQuickAbstractDialog::QQuickAbstractDialog(QObject *parent)
    : QObject(parent)
    , m_parentWindow(0)
    , m_visible(false)
    , m_modality(Qt::WindowModal)
    , m_contentItem(0)
    , m_dialogWindow(0)
    , m_windowDecoration(0)
    , m_hasNativeWindows(QGuiApplicationPrivate::platformIntegration()->
        hasCapability(QPlatformIntegration::MultipleWindows) &&
        QGuiApplicationPrivate::platformIntegration()->
        hasCapability(QPlatformIntegration::WindowManagement))
    , m_hasAspiredPosition(false)
    , m_visibleChangedConnected(false)
{
}

QQuickAbstractDialog::~QQuickAbstractDialog()
{
}

void QQuickAbstractDialog::setVisible(bool v)
{
    if (m_visible == v) return;
    m_visible = v;
    if (helper()) {
        qCDebug(lcWindow) << "via helper" << helper();
        if (v) {
            Qt::WindowFlags flags = Qt::Dialog;
            if (!title().isEmpty())
                flags |= Qt::WindowTitleHint;
            m_visible = helper()->show(flags, m_modality, parentWindow());
        } else {
            helper()->hide();
        }
    } else {
        // Pure QML implementation: wrap the contentItem in a window, or fake it
        if (!m_dialogWindow && m_contentItem) {
            if (m_hasNativeWindows)
                m_dialogWindow = m_contentItem->window();
            // An Item-based dialog implementation doesn't come with a window, so
            // we have to instantiate one iff the platform allows it.
            if (!m_dialogWindow && m_hasNativeWindows) {
                QQuickWindow *win = new QQuickWindow;
                ((QObject *)win)->setParent(this); // memory management only
                win->setFlags(Qt::Dialog);
                m_dialogWindow = win;
                m_contentItem->setParentItem(win->contentItem());
                QSize minSize = QSize(m_contentItem->implicitWidth(), m_contentItem->implicitHeight());
                QVariant minHeight = m_contentItem->property("minimumHeight");
                if (minHeight.isValid()) {
                    if (minHeight.toInt() > minSize.height())
                        minSize.setHeight(minHeight.toDouble());
                    connect(m_contentItem, SIGNAL(minimumHeightChanged()), this, SLOT(minimumHeightChanged()));
                }
                QVariant minWidth = m_contentItem->property("minimumWidth");
                if (minWidth.isValid()) {
                    if (minWidth.toInt() > minSize.width())
                        minSize.setWidth(minWidth.toInt());
                    connect(m_contentItem, SIGNAL(minimumWidthChanged()), this, SLOT(minimumWidthChanged()));
                }
                m_dialogWindow->setMinimumSize(minSize);
                connect(win, SIGNAL(widthChanged(int)), this, SLOT(windowGeometryChanged()));
                connect(win, SIGNAL(heightChanged(int)), this, SLOT(windowGeometryChanged()));
                qCDebug(lcWindow) << "created window" << win;
            }

            if (!m_dialogWindow) {
                if (Q_UNLIKELY(!parentWindow())) {
                    qWarning("cannot set dialog visible: no window");
                    return;
                }
                m_dialogWindow = parentWindow();

                // If the platform does not support multiple windows, but the dialog is
                // implemented as an Item, then try to decorate it as a fake window and make it visible.
                if (!m_windowDecoration) {
                    if (m_decorationComponent) {
                        if (m_decorationComponent->isLoading())
                            connect(m_decorationComponent, SIGNAL(statusChanged(QQmlComponent::Status)),
                                    this, SLOT(decorationLoaded()));
                        else
                            decorationLoaded(); // do the reparenting of contentItem on top of it
                    }
                    // Window decoration wasn't possible, so just reparent it into the scene
                    else {
                        qCDebug(lcWindow) << "no window and no decoration";
                        m_contentItem->setParentItem(parentWindow()->contentItem());
                        m_contentItem->setZ(10000);
                    }
                }
            }
        }
        if (m_dialogWindow) {
            // "grow up" to the size and position expected to achieve
            if (!m_sizeAspiration.isNull()) {
                if (m_hasAspiredPosition) {
                    qCDebug(lcWindow) << "geometry aspiration" << m_sizeAspiration;
                    m_dialogWindow->setGeometry(m_sizeAspiration);
                } else {
                    qCDebug(lcWindow) << "size aspiration" << m_sizeAspiration.size();
                    if (m_sizeAspiration.width() > 0)
                        m_dialogWindow->setWidth(m_sizeAspiration.width());
                    if (m_sizeAspiration.height() > 0)
                        m_dialogWindow->setHeight(m_sizeAspiration.height());
                }
                connect(m_dialogWindow, SIGNAL(xChanged(int)), this, SLOT(setX(int)));
                connect(m_dialogWindow, SIGNAL(yChanged(int)), this, SLOT(setY(int)));
                connect(m_dialogWindow, SIGNAL(widthChanged(int)), this, SLOT(setWidth(int)));
                connect(m_dialogWindow, SIGNAL(heightChanged(int)), this, SLOT(setHeight(int)));
                connect(m_contentItem, SIGNAL(implicitHeightChanged()), this, SLOT(implicitHeightChanged()));
            }
            if (!m_visibleChangedConnected) {
                connect(m_dialogWindow, SIGNAL(visibleChanged(bool)), this, SLOT(visibleChanged(bool)));
                m_visibleChangedConnected = true;
            }
        }
        if (m_windowDecoration) {
            m_windowDecoration->setProperty("dismissOnOuterClick", (m_modality == Qt::NonModal));
            m_windowDecoration->setVisible(v);
        } else if (m_dialogWindow) {
            if (v) {
                m_dialogWindow->setTransientParent(parentWindow());
                m_dialogWindow->setTitle(title());
                m_dialogWindow->setModality(m_modality);
            }
            m_dialogWindow->setVisible(v);
        }
    }
    emit visibilityChanged();
}

void QQuickAbstractDialog::decorationLoaded()
{
    bool ok = false;
    Q_ASSERT(parentWindow());
    QQuickItem *parentItem = parentWindow()->contentItem();
    Q_ASSERT(parentItem);
    if (m_decorationComponent->isError()) {
        qWarning() << m_decorationComponent->errors();
    } else {
        QObject *decoration = m_decorationComponent->create();
        m_windowDecoration = qobject_cast<QQuickItem *>(decoration);
        if (m_windowDecoration) {
            m_windowDecoration->setParentItem(parentItem);
            // Give the window decoration its content to manage
            QVariant contentVariant;
            contentVariant.setValue<QQuickItem*>(m_contentItem);
            m_windowDecoration->setProperty("content", contentVariant);
            connect(m_windowDecoration, SIGNAL(dismissed()), this, SLOT(reject()));
            ok = true;
            qCDebug(lcWindow) << "using synthetic window decoration" << m_windowDecoration;
        } else {
            qWarning() << m_decorationComponent->url() <<
                "cannot be used as a window decoration because it's not an Item";
            delete m_windowDecoration;
            delete m_decorationComponent;
            m_decorationComponent = 0;
        }
    }
    // Window decoration wasn't possible, so just reparent it into the scene
    if (!ok) {
        m_contentItem->setParentItem(parentItem);
        m_contentItem->setZ(10000);
        qCDebug(lcWindow) << "no decoration";
    }
}

void QQuickAbstractDialog::setModality(Qt::WindowModality m)
{
    if (m_modality == m) return;
    qCDebug(lcWindow) << "modality" << m;
    m_modality = m;
    emit modalityChanged();
}

void QQuickAbstractDialog::accept()
{
    setVisible(false);
    emit accepted();
}

void QQuickAbstractDialog::reject()
{
    setVisible(false);
    emit rejected();
}

void QQuickAbstractDialog::visibleChanged(bool v)
{
    m_visible = v;
    qCDebug(lcWindow) << "visible" << v;
    emit visibilityChanged();
}

void QQuickAbstractDialog::windowGeometryChanged()
{
    if (m_dialogWindow && m_contentItem) {
        qCDebug(lcWindow) << m_dialogWindow->geometry();
        m_contentItem->setWidth(m_dialogWindow->width());
        m_contentItem->setHeight(m_dialogWindow->height());
    }
}

void QQuickAbstractDialog::minimumWidthChanged()
{
    qreal min = m_contentItem->property("minimumWidth").toReal();
    qCDebug(lcWindow) << "content implicitWidth" << m_contentItem->implicitWidth() << "minimumWidth" << min;
    m_dialogWindow->setMinimumWidth(qMax(m_contentItem->implicitWidth(), min));
}

void QQuickAbstractDialog::minimumHeightChanged()
{
    qreal min = m_contentItem->property("minimumHeight").toReal();
    qCDebug(lcWindow) << "content implicitHeight" << m_contentItem->implicitHeight() << "minimumHeight" << min;
    m_dialogWindow->setMinimumHeight(qMax(m_contentItem->implicitHeight(),
        m_contentItem->property("minimumHeight").toReal()));
}

void QQuickAbstractDialog::implicitHeightChanged()
{
    qCDebug(lcWindow) << "content implicitHeight" << m_contentItem->implicitHeight()
                      << "window minimumHeight" << m_dialogWindow->minimumHeight();
    if (m_contentItem->implicitHeight() < m_dialogWindow->minimumHeight())
        m_dialogWindow->setMinimumHeight(m_contentItem->implicitHeight());
}

QQuickWindow *QQuickAbstractDialog::parentWindow()
{
    if (!m_parentWindow) {
        // Usually a dialog is declared inside an Item; but if its QObject parent
        // is a Window, that's the window we are interested in. (QTBUG-38578)
        QQuickItem *parentItem = qobject_cast<QQuickItem *>(parent());
        m_parentWindow = (parentItem ? parentItem->window() : qmlobject_cast<QQuickWindow *>(parent()));
    }
    return m_parentWindow;
}

void QQuickAbstractDialog::setContentItem(QQuickItem *obj)
{
    m_contentItem = obj;
    qCDebug(lcWindow) << obj;
    if (m_dialogWindow) {
        disconnect(this, SLOT(visibleChanged(bool)));
        // Can't necessarily delete because m_dialogWindow might have been provided by the QML.
        m_dialogWindow = 0;
    }
}

int QQuickAbstractDialog::x() const
{
    if (m_dialogWindow)
        return m_dialogWindow->x();
    return m_sizeAspiration.x();
}

int QQuickAbstractDialog::y() const
{
    if (m_dialogWindow)
        return m_dialogWindow->y();
    return m_sizeAspiration.y();
}

int QQuickAbstractDialog::width() const
{
    if (m_dialogWindow)
        return m_dialogWindow->width();
    return m_sizeAspiration.width();
}

int QQuickAbstractDialog::height() const
{
    if (m_dialogWindow)
        return m_dialogWindow->height();
    return m_sizeAspiration.height();
}

/*
    A non-fullscreen dialog is not allowed to be too large
    to fit on the screen in either orientation (portrait or landscape).
    That way on platforms which can do rotation, the dialog does not
    change its size when the screen is rotated.  So the value returned
    here is the maximum for both width and height.  We need to know
    at init time, not wait until the dialog's content item is shown in
    a window so that the desktopAvailableWidth and Height will be valid
    in the Screen attached property.  And to allow space for window borders,
    the max is further reduced by 10%.
*/
int QQuickAbstractDialog::__maximumDimension() const
{
    QScreen *screen = QGuiApplication::primaryScreen();
    return (screen ?
                qMin(screen->availableVirtualGeometry().width(), screen->availableVirtualGeometry().height()) :
                480) * 9 / 10;
}

void QQuickAbstractDialog::setX(int arg)
{
    m_hasAspiredPosition = true;
    m_sizeAspiration.moveLeft(arg);
    if (helper()) {
        // TODO
    } else if (m_dialogWindow) {
        if (sender() != m_dialogWindow)
            m_dialogWindow->setX(arg);
    } else if (m_contentItem) {
        m_contentItem->setX(arg);
    }
    qCDebug(lcWindow) << arg;
    emit geometryChanged();
}

void QQuickAbstractDialog::setY(int arg)
{
    m_hasAspiredPosition = true;
    m_sizeAspiration.moveTop(arg);
    if (helper()) {
        // TODO
    } else if (m_dialogWindow) {
        if (sender() != m_dialogWindow)
            m_dialogWindow->setY(arg);
    } else if (m_contentItem) {
        m_contentItem->setY(arg);
    }
    qCDebug(lcWindow) << arg;
    emit geometryChanged();
}

void QQuickAbstractDialog::setWidth(int arg)
{
    m_sizeAspiration.setWidth(arg);
    if (helper()) {
        // TODO
    } else if (m_dialogWindow) {
        if (sender() != m_dialogWindow)
            m_dialogWindow->setWidth(arg);
    } else if (m_contentItem) {
        m_contentItem->setWidth(arg);
    }
    qCDebug(lcWindow) << arg;
    emit geometryChanged();
}

void QQuickAbstractDialog::setHeight(int arg)
{
    m_sizeAspiration.setHeight(arg);
    if (helper()) {
        // TODO
    } else if (m_dialogWindow) {
        if (sender() != m_dialogWindow)
            m_dialogWindow->setHeight(arg);
    } else if (m_contentItem) {
        m_contentItem->setHeight(arg);
    }
    qCDebug(lcWindow) << arg;
    emit geometryChanged();
}

QT_END_NAMESPACE

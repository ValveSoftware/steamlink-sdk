/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Designer of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
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
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef DEFAULT_ACTIONPROVIDER_H
#define DEFAULT_ACTIONPROVIDER_H

#include "formeditor_global.h"
#include "actionprovider_p.h"
#include <extensionfactory_p.h>

#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QToolBar>

QT_BEGIN_NAMESPACE

namespace qdesigner_internal {

class FormWindow;

class QT_FORMEDITOR_EXPORT ActionProviderBase: public QDesignerActionProviderExtension
{
protected:
    explicit ActionProviderBase(QWidget *widget);

public:
    void adjustIndicator(const QPoint &pos) Q_DECL_OVERRIDE;
    virtual Qt::Orientation orientation() const = 0;

protected:
    virtual QRect indicatorGeometry(const QPoint &pos, Qt::LayoutDirection layoutDirection) const;

private:
    QWidget *m_indicator;
};

class QT_FORMEDITOR_EXPORT QToolBarActionProvider: public QObject, public ActionProviderBase
{
    Q_OBJECT
    Q_INTERFACES(QDesignerActionProviderExtension)
public:
    explicit QToolBarActionProvider(QToolBar *widget, QObject *parent = 0);

    QRect actionGeometry(QAction *action) const Q_DECL_OVERRIDE;
    QAction *actionAt(const QPoint &pos) const Q_DECL_OVERRIDE;
    Qt::Orientation orientation() const Q_DECL_OVERRIDE;

protected:
    QRect indicatorGeometry(const QPoint &pos, Qt::LayoutDirection layoutDirection) const Q_DECL_OVERRIDE;

private:
    QToolBar *m_widget;
};

class QT_FORMEDITOR_EXPORT QMenuBarActionProvider: public QObject, public ActionProviderBase
{
    Q_OBJECT
    Q_INTERFACES(QDesignerActionProviderExtension)
public:
    explicit QMenuBarActionProvider(QMenuBar *widget, QObject *parent = 0);

    QRect actionGeometry(QAction *action) const Q_DECL_OVERRIDE;
    QAction *actionAt(const QPoint &pos) const Q_DECL_OVERRIDE;
    Qt::Orientation orientation() const Q_DECL_OVERRIDE;

private:
    QMenuBar *m_widget;
};

class QT_FORMEDITOR_EXPORT QMenuActionProvider: public QObject, public ActionProviderBase
{
    Q_OBJECT
    Q_INTERFACES(QDesignerActionProviderExtension)
public:
    explicit QMenuActionProvider(QMenu *widget, QObject *parent = 0);

    QRect actionGeometry(QAction *action) const Q_DECL_OVERRIDE;
    QAction *actionAt(const QPoint &pos) const Q_DECL_OVERRIDE;
    Qt::Orientation orientation() const Q_DECL_OVERRIDE;

private:
    QMenu *m_widget;
};

typedef ExtensionFactory<QDesignerActionProviderExtension, QToolBar, QToolBarActionProvider> QToolBarActionProviderFactory;
typedef ExtensionFactory<QDesignerActionProviderExtension, QMenuBar, QMenuBarActionProvider> QMenuBarActionProviderFactory;
typedef ExtensionFactory<QDesignerActionProviderExtension, QMenu, QMenuActionProvider> QMenuActionProviderFactory;

} // namespace qdesigner_internal

QT_END_NAMESPACE

#endif // DEFAULT_ACTIONPROVIDER_H

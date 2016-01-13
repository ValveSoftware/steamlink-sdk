/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Designer of the Qt Toolkit.
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
    virtual void adjustIndicator(const QPoint &pos);
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

    virtual QRect actionGeometry(QAction *action) const;
    virtual QAction *actionAt(const QPoint &pos) const;
    Qt::Orientation orientation() const;

protected:
    virtual QRect indicatorGeometry(const QPoint &pos, Qt::LayoutDirection layoutDirection) const;

private:
    QToolBar *m_widget;
};

class QT_FORMEDITOR_EXPORT QMenuBarActionProvider: public QObject, public ActionProviderBase
{
    Q_OBJECT
    Q_INTERFACES(QDesignerActionProviderExtension)
public:
    explicit QMenuBarActionProvider(QMenuBar *widget, QObject *parent = 0);

    virtual QRect actionGeometry(QAction *action) const;
    virtual QAction *actionAt(const QPoint &pos) const;
    Qt::Orientation orientation() const;

private:
    QMenuBar *m_widget;
};

class QT_FORMEDITOR_EXPORT QMenuActionProvider: public QObject, public ActionProviderBase
{
    Q_OBJECT
    Q_INTERFACES(QDesignerActionProviderExtension)
public:
    explicit QMenuActionProvider(QMenu *widget, QObject *parent = 0);

    virtual QRect actionGeometry(QAction *action) const;
    virtual QAction *actionAt(const QPoint &pos) const;
    Qt::Orientation orientation() const;

private:
    QMenu *m_widget;
};

typedef ExtensionFactory<QDesignerActionProviderExtension, QToolBar, QToolBarActionProvider> QToolBarActionProviderFactory;
typedef ExtensionFactory<QDesignerActionProviderExtension, QMenuBar, QMenuBarActionProvider> QMenuBarActionProviderFactory;
typedef ExtensionFactory<QDesignerActionProviderExtension, QMenu, QMenuActionProvider> QMenuActionProviderFactory;

} // namespace qdesigner_internal

QT_END_NAMESPACE

#endif // DEFAULT_ACTIONPROVIDER_H

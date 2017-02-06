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

#ifndef DEFAULT_CONTAINER_H
#define DEFAULT_CONTAINER_H

#include <QtDesigner/QDesignerContainerExtension>
#include <QtDesigner/extension.h>
#include <extensionfactory_p.h>

#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QToolBox>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QDockWidget>

QT_BEGIN_NAMESPACE

namespace qdesigner_internal {

// ------------ QStackedWidgetContainer
class QStackedWidgetContainer: public QObject, public QDesignerContainerExtension
{
    Q_OBJECT
    Q_INTERFACES(QDesignerContainerExtension)
public:
    explicit QStackedWidgetContainer(QStackedWidget *widget, QObject *parent = 0);

    int count() const Q_DECL_OVERRIDE { return m_widget->count(); }
    QWidget *widget(int index) const Q_DECL_OVERRIDE { return m_widget->widget(index); }

    int currentIndex() const Q_DECL_OVERRIDE { return m_widget->currentIndex(); }
    void setCurrentIndex(int index) Q_DECL_OVERRIDE;

    void addWidget(QWidget *widget) Q_DECL_OVERRIDE;
    void insertWidget(int index, QWidget *widget) Q_DECL_OVERRIDE;
    void remove(int index) Q_DECL_OVERRIDE;

private:
    QStackedWidget *m_widget;
};

// ------------ QTabWidgetContainer
class QTabWidgetContainer: public QObject, public QDesignerContainerExtension
{
    Q_OBJECT
    Q_INTERFACES(QDesignerContainerExtension)
public:
    explicit QTabWidgetContainer(QTabWidget *widget, QObject *parent = 0);

    int count() const Q_DECL_OVERRIDE { return m_widget->count(); }
    QWidget *widget(int index) const Q_DECL_OVERRIDE { return m_widget->widget(index); }

    int currentIndex() const Q_DECL_OVERRIDE { return m_widget->currentIndex(); }
    void setCurrentIndex(int index) Q_DECL_OVERRIDE;

    void addWidget(QWidget *widget) Q_DECL_OVERRIDE;
    void insertWidget(int index, QWidget *widget) Q_DECL_OVERRIDE;
    void remove(int index) Q_DECL_OVERRIDE;

private:
    QTabWidget *m_widget;
};

// ------------  QToolBoxContainer
class QToolBoxContainer: public QObject, public QDesignerContainerExtension
{
    Q_OBJECT
    Q_INTERFACES(QDesignerContainerExtension)
public:
    explicit QToolBoxContainer(QToolBox *widget, QObject *parent = 0);

    int count() const Q_DECL_OVERRIDE { return m_widget->count(); }
    QWidget *widget(int index) const Q_DECL_OVERRIDE { return m_widget->widget(index); }

    int currentIndex() const Q_DECL_OVERRIDE { return m_widget->currentIndex(); }
    void setCurrentIndex(int index) Q_DECL_OVERRIDE;

    void addWidget(QWidget *widget) Q_DECL_OVERRIDE;
    void insertWidget(int index, QWidget *widget) Q_DECL_OVERRIDE;
    void remove(int index) Q_DECL_OVERRIDE;

private:
    QToolBox *m_widget;
};

// ------------ SingleChildContainer:
//  Template for containers that have a single child widget using widget()/setWidget().

template <class Container>
class SingleChildContainer: public QDesignerContainerExtension
{
protected:
    explicit SingleChildContainer(Container *widget, bool active = true);
public:
    int count() const Q_DECL_OVERRIDE;
    QWidget *widget(int index) const Q_DECL_OVERRIDE;
    int currentIndex() const Q_DECL_OVERRIDE;
    void setCurrentIndex(int /*index*/) Q_DECL_OVERRIDE {}
    void addWidget(QWidget *widget) Q_DECL_OVERRIDE;
    void insertWidget(int index, QWidget *widget) Q_DECL_OVERRIDE;
    void remove(int /*index*/) Q_DECL_OVERRIDE {}

    bool canAddWidget() const Q_DECL_OVERRIDE { return false; }
    bool canRemove(int) const Q_DECL_OVERRIDE { return false; }

private:
    const bool m_active;
    Container *m_container;
};

template <class Container>
SingleChildContainer<Container>::SingleChildContainer(Container *widget, bool active) :
    m_active(active),
    m_container(widget)
{
}

template <class Container>
int SingleChildContainer<Container>::count() const
{
    return  m_active && m_container->widget() ? 1 : 0;
}

template <class Container>
QWidget *SingleChildContainer<Container>::widget(int /* index */) const
{
    return m_container->widget();
}

template <class Container>
int SingleChildContainer<Container>::currentIndex() const
{
    return m_active && m_container->widget() ? 0 : -1;
}

template <class Container>
void SingleChildContainer<Container>::addWidget(QWidget *widget)
{
    Q_ASSERT(m_container->widget() == 0);
    widget->setParent(m_container);
    m_container->setWidget(widget);
}

template <class Container>
void SingleChildContainer<Container>::insertWidget(int /* index */, QWidget *widget)
{
    addWidget(widget);
}

// ------------  QScrollAreaContainer
class QScrollAreaContainer: public QObject, public SingleChildContainer<QScrollArea>
{
    Q_OBJECT
    Q_INTERFACES(QDesignerContainerExtension)
public:
    explicit QScrollAreaContainer(QScrollArea *widget, QObject *parent = 0);
};

// --------------- QDockWidgetContainer
class QDockWidgetContainer: public QObject, public SingleChildContainer<QDockWidget>
{
    Q_OBJECT
    Q_INTERFACES(QDesignerContainerExtension)
public:
    explicit QDockWidgetContainer(QDockWidget *widget, QObject *parent = 0);
};

typedef ExtensionFactory<QDesignerContainerExtension, QStackedWidget, QStackedWidgetContainer> QDesignerStackedWidgetContainerFactory;
typedef ExtensionFactory<QDesignerContainerExtension, QTabWidget, QTabWidgetContainer> QDesignerTabWidgetContainerFactory;
typedef ExtensionFactory<QDesignerContainerExtension, QToolBox, QToolBoxContainer> QDesignerToolBoxContainerFactory;
typedef ExtensionFactory<QDesignerContainerExtension, QScrollArea, QScrollAreaContainer> QScrollAreaContainerFactory;
typedef ExtensionFactory<QDesignerContainerExtension,  QDockWidget, QDockWidgetContainer> QDockWidgetContainerFactory;
}  // namespace qdesigner_internal

QT_END_NAMESPACE

#endif // DEFAULT_CONTAINER_H

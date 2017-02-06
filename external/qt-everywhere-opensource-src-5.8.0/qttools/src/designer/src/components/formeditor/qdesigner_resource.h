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

#ifndef QDESIGNER_RESOURCE_H
#define QDESIGNER_RESOURCE_H

#include "formeditor_global.h"
#include "qsimpleresource_p.h"

#include <QtCore/QHash>
#include <QtCore/QStack>
#include <QtCore/QList>

QT_BEGIN_NAMESPACE

class DomCustomWidget;
class DomCustomWidgets;
class DomResource;

class QDesignerContainerExtension;
class QDesignerFormEditorInterface;
class QDesignerCustomWidgetInterface;
class QDesignerWidgetDataBaseItemInterface;

class QTabWidget;
class QStackedWidget;
class QToolBox;
class QToolBar;
class QDesignerDockWidget;
class QLayoutWidget;
class QWizardPage;

namespace qdesigner_internal {

class FormWindow;

class QT_FORMEDITOR_EXPORT QDesignerResource : public QEditorFormBuilder
{
public:
    explicit QDesignerResource(FormWindow *fw);
    virtual ~QDesignerResource();

    void save(QIODevice *dev, QWidget *widget) Q_DECL_OVERRIDE;

    bool copy(QIODevice *dev, const FormBuilderClipboard &selection) Q_DECL_OVERRIDE;
    DomUI *copy(const FormBuilderClipboard &selection) Q_DECL_OVERRIDE;

    FormBuilderClipboard paste(DomUI *ui, QWidget *widgetParent, QObject *actionParent = 0) Q_DECL_OVERRIDE;
    FormBuilderClipboard paste(QIODevice *dev,  QWidget *widgetParent, QObject *actionParent = 0) Q_DECL_OVERRIDE;

    bool saveRelative() const;
    void setSaveRelative(bool relative);

    QWidget *load(QIODevice *dev, QWidget *parentWidget) Q_DECL_OVERRIDE;

protected:
    using QEditorFormBuilder::create;
    using QEditorFormBuilder::createDom;

    void saveDom(DomUI *ui, QWidget *widget) Q_DECL_OVERRIDE;
    QWidget *create(DomUI *ui, QWidget *parentWidget) Q_DECL_OVERRIDE;
    QWidget *create(DomWidget *ui_widget, QWidget *parentWidget) Q_DECL_OVERRIDE;
    QLayout *create(DomLayout *ui_layout, QLayout *layout, QWidget *parentWidget) Q_DECL_OVERRIDE;
    QLayoutItem *create(DomLayoutItem *ui_layoutItem, QLayout *layout, QWidget *parentWidget) Q_DECL_OVERRIDE;
    void applyProperties(QObject *o, const QList<DomProperty*> &properties) Q_DECL_OVERRIDE;
    QList<DomProperty*> computeProperties(QObject *obj) Q_DECL_OVERRIDE;
    DomProperty *createProperty(QObject *object, const QString &propertyName, const QVariant &value) Q_DECL_OVERRIDE;

    QWidget *createWidget(const QString &widgetName, QWidget *parentWidget, const QString &name) Q_DECL_OVERRIDE;
    QLayout *createLayout(const QString &layoutName, QObject *parent, const QString &name) Q_DECL_OVERRIDE;
    void createCustomWidgets(DomCustomWidgets *) Q_DECL_OVERRIDE;
    void createResources(DomResources*) Q_DECL_OVERRIDE;
    void applyTabStops(QWidget *widget, DomTabStops *tabStops) Q_DECL_OVERRIDE;

    bool addItem(DomLayoutItem *ui_item, QLayoutItem *item, QLayout *layout) Q_DECL_OVERRIDE;
    bool addItem(DomWidget *ui_widget, QWidget *widget, QWidget *parentWidget) Q_DECL_OVERRIDE;

    DomWidget *createDom(QWidget *widget, DomWidget *ui_parentWidget, bool recursive = true) Q_DECL_OVERRIDE;
    DomLayout *createDom(QLayout *layout, DomLayout *ui_layout, DomWidget *ui_parentWidget) Q_DECL_OVERRIDE;
    DomLayoutItem *createDom(QLayoutItem *item, DomLayout *ui_layout, DomWidget *ui_parentWidget) Q_DECL_OVERRIDE;

    QAction *create(DomAction *ui_action, QObject *parent) Q_DECL_OVERRIDE;
    QActionGroup *create(DomActionGroup *ui_action_group, QObject *parent) Q_DECL_OVERRIDE;
    void addMenuAction(QAction *action) Q_DECL_OVERRIDE;

    DomAction *createDom(QAction *action) Q_DECL_OVERRIDE;
    DomActionGroup *createDom(QActionGroup *actionGroup) Q_DECL_OVERRIDE;
    DomActionRef *createActionRefDom(QAction *action) Q_DECL_OVERRIDE;

    QAction *createAction(QObject *parent, const QString &name) Q_DECL_OVERRIDE;
    QActionGroup *createActionGroup(QObject *parent, const QString &name) Q_DECL_OVERRIDE;

    bool checkProperty(QObject *obj, const QString &prop) const Q_DECL_OVERRIDE;

    DomWidget *saveWidget(QTabWidget *widget, DomWidget *ui_parentWidget);
    DomWidget *saveWidget(QStackedWidget *widget, DomWidget *ui_parentWidget);
    DomWidget *saveWidget(QToolBox *widget, DomWidget *ui_parentWidget);
    DomWidget *saveWidget(QWidget *widget, QDesignerContainerExtension *container, DomWidget *ui_parentWidget);
    DomWidget *saveWidget(QToolBar *toolBar, DomWidget *ui_parentWidget);
    DomWidget *saveWidget(QDesignerDockWidget *dockWidget, DomWidget *ui_parentWidget);
    DomWidget *saveWidget(QWizardPage *wizardPage, DomWidget *ui_parentWidget);

    DomCustomWidgets *saveCustomWidgets() Q_DECL_OVERRIDE;
    DomTabStops *saveTabStops() Q_DECL_OVERRIDE;
    DomResources *saveResources() Q_DECL_OVERRIDE;

    void layoutInfo(DomLayout *layout, QObject *parent, int *margin, int *spacing) Q_DECL_OVERRIDE;

    void loadExtraInfo(DomWidget *ui_widget, QWidget *widget, QWidget *parentWidget) Q_DECL_OVERRIDE;

    void changeObjectName(QObject *o, QString name);
    DomProperty *applyProperStdSetAttribute(QObject *object, const QString &propertyName, DomProperty *property);

private:
    DomResources *saveResources(const QStringList &qrcPaths);
    bool canCompressSpacings(QObject *object) const;
    QStringList mergeWithLoadedPaths(const QStringList &paths) const;
    void applyAttributesToPropertySheet(const DomWidget *ui_widget, QWidget *widget);

    typedef QList<DomCustomWidget*> DomCustomWidgetList;
    void addCustomWidgetsToWidgetDatabase(DomCustomWidgetList& list);
    FormWindow *m_formWindow;
    bool m_isMainWidget;
    QHash<QString, QString> m_internal_to_qt;
    QHash<QString, QString> m_qt_to_internal;
    QStack<QLayout*> m_chain;
    QHash<QDesignerWidgetDataBaseItemInterface*, bool> m_usedCustomWidgets;
    bool m_copyWidget;
    QWidget *m_selected;
    class QDesignerResourceBuilder *m_resourceBuilder;
};

}  // namespace qdesigner_internal

QT_END_NAMESPACE

#endif // QDESIGNER_RESOURCE_H

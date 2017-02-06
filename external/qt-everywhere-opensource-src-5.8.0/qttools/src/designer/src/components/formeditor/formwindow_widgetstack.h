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

#ifndef FORMWINDOW_WIDGETSTACK_H
#define FORMWINDOW_WIDGETSTACK_H

#include "formeditor_global.h"

#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class QDesignerFormWindowToolInterface;

class QStackedLayout;
class QWidget;

namespace qdesigner_internal {

class QT_FORMEDITOR_EXPORT FormWindowWidgetStack: public QObject
{
    Q_OBJECT
public:
    FormWindowWidgetStack(QObject *parent = 0);
    virtual ~FormWindowWidgetStack();

    QLayout *layout() const;

    int count() const;
    QDesignerFormWindowToolInterface *tool(int index) const;
    QDesignerFormWindowToolInterface *currentTool() const;
    int currentIndex() const;
    int indexOf(QDesignerFormWindowToolInterface *tool) const;

    void setMainContainer(QWidget *w = 0);

    // Return the widget containing the form which can be used to apply embedded design settings to.
    // These settings should not affect the other editing tools.
    QWidget *formContainer() const { return m_formContainer; }

signals:
    void currentToolChanged(int index);

public slots:
    void addTool(QDesignerFormWindowToolInterface *tool);
    void setCurrentTool(QDesignerFormWindowToolInterface *tool);
    void setCurrentTool(int index);
    void setSenderAsCurrentTool();

protected:
    QWidget *defaultEditor() const;

private:
    QList<QDesignerFormWindowToolInterface*> m_tools;
    QWidget *m_formContainer;
    QStackedLayout *m_formContainerLayout;
    QStackedLayout *m_layout;
};

}  // namespace qdesigner_internal

QT_END_NAMESPACE

#endif // FORMWINDOW_WIDGETSTACK_H

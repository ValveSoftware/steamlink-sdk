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

#include "gridpanel_p.h"
#include "ui_gridpanel.h"
#include "grid_p.h"

QT_BEGIN_NAMESPACE

namespace qdesigner_internal {

GridPanel::GridPanel(QWidget *parentWidget) :
    QWidget(parentWidget)
{
    m_ui = new Ui::GridPanel;
    m_ui->setupUi(this);

    connect(m_ui->m_resetButton, SIGNAL(clicked()), this, SLOT(reset()));
}

GridPanel::~GridPanel()
{
    delete m_ui;
}

void GridPanel::setGrid(const Grid &g)
{
    m_ui->m_deltaXSpinBox->setValue(g.deltaX());
    m_ui->m_deltaYSpinBox->setValue(g.deltaY());
    m_ui->m_visibleCheckBox->setCheckState(g.visible() ? Qt::Checked : Qt::Unchecked);
    m_ui->m_snapXCheckBox->setCheckState(g.snapX()  ? Qt::Checked : Qt::Unchecked);
    m_ui->m_snapYCheckBox->setCheckState(g.snapY()  ? Qt::Checked : Qt::Unchecked);
}

void GridPanel::setTitle(const QString &title)
{
    m_ui->m_gridGroupBox->setTitle(title);
}

Grid GridPanel::grid() const
{
    Grid rc;
    rc.setDeltaX(m_ui->m_deltaXSpinBox->value());
    rc.setDeltaY(m_ui->m_deltaYSpinBox->value());
    rc.setSnapX(m_ui->m_snapXCheckBox->checkState() == Qt::Checked);
    rc.setSnapY(m_ui->m_snapYCheckBox->checkState() == Qt::Checked);
    rc.setVisible(m_ui->m_visibleCheckBox->checkState() == Qt::Checked);
    return rc;
}

void GridPanel::reset()
{
    setGrid(Grid());
}

void GridPanel::setCheckable (bool c)
{
    m_ui->m_gridGroupBox->setCheckable(c);
}

bool GridPanel::isCheckable () const
{
    return m_ui->m_gridGroupBox->isCheckable ();
}

bool GridPanel::isChecked () const
{
    return m_ui->m_gridGroupBox->isChecked ();
}

void GridPanel::setChecked(bool c)
{
    m_ui->m_gridGroupBox->setChecked(c);
}

void GridPanel::setResetButtonVisible(bool v)
{
    m_ui->m_resetButton->setVisible(v);
}

}

QT_END_NAMESPACE

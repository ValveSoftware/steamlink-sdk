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

#include "formeditor_optionspage.h"

// shared
#include "formwindowbase_p.h"
#include "gridpanel_p.h"
#include "grid_p.h"
#include "previewconfigurationwidget_p.h"
#include "shared_settings_p.h"
#include "zoomwidget_p.h"

// SDK
#include <QtDesigner/QDesignerFormEditorInterface>
#include <QtDesigner/QDesignerFormWindowManagerInterface>

#include <QtCore/QString>
#include <QtCore/QCoreApplication>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QComboBox>

QT_BEGIN_NAMESPACE

typedef QList<int> IntList;

namespace qdesigner_internal {

// Zoom, currently for preview only
class ZoomSettingsWidget : public QGroupBox {
    Q_DISABLE_COPY(ZoomSettingsWidget)
public:
    explicit ZoomSettingsWidget(QWidget *parent = 0);

    void fromSettings(const QDesignerSharedSettings &s);
    void toSettings(QDesignerSharedSettings &s) const;

private:
    QComboBox *m_zoomCombo;
};

ZoomSettingsWidget::ZoomSettingsWidget(QWidget *parent) :
    QGroupBox(parent),
    m_zoomCombo(new QComboBox)
{
    m_zoomCombo->setEditable(false);
    const IntList zoomValues = ZoomMenu::zoomValues();
    const IntList::const_iterator cend = zoomValues.constEnd();

    for (IntList::const_iterator it = zoomValues.constBegin(); it != cend; ++it) {
        //: Zoom percentage
        m_zoomCombo->addItem(QCoreApplication::translate("FormEditorOptionsPage", "%1 %").arg(*it), QVariant(*it));
    }

    // Layout
    setCheckable(true);
    setTitle(QCoreApplication::translate("FormEditorOptionsPage", "Preview Zoom"));
    QFormLayout *lt = new QFormLayout;
    lt->addRow(QCoreApplication::translate("FormEditorOptionsPage", "Default Zoom"), m_zoomCombo);
    setLayout(lt);
}

void ZoomSettingsWidget::fromSettings(const QDesignerSharedSettings &s)
{
    setChecked(s.zoomEnabled());
    const int idx = m_zoomCombo->findData(QVariant(s.zoom()));
    m_zoomCombo->setCurrentIndex(qMax(0, idx));
}

void ZoomSettingsWidget::toSettings(QDesignerSharedSettings &s) const
{
    s.setZoomEnabled(isChecked());
    const int zoom = m_zoomCombo->itemData(m_zoomCombo->currentIndex()).toInt();
    s.setZoom(zoom);
}



// FormEditorOptionsPage:
FormEditorOptionsPage::FormEditorOptionsPage(QDesignerFormEditorInterface *core)
        : m_core(core)
{
}

QString FormEditorOptionsPage::name() const
{
    //: Tab in preferences dialog
    return QCoreApplication::translate("FormEditorOptionsPage", "Forms");
}

QWidget *FormEditorOptionsPage::createPage(QWidget *parent)
{
    QWidget *optionsWidget = new QWidget(parent);

    const QDesignerSharedSettings settings(m_core);
    m_previewConf = new PreviewConfigurationWidget(m_core);
    m_zoomSettingsWidget = new ZoomSettingsWidget;
    m_zoomSettingsWidget->fromSettings(settings);

    m_defaultGridConf = new GridPanel();
    m_defaultGridConf->setTitle(QCoreApplication::translate("FormEditorOptionsPage", "Default Grid"));
    m_defaultGridConf->setGrid(settings.defaultGrid());

    QVBoxLayout *optionsVLayout = new QVBoxLayout();
    optionsVLayout->addWidget(m_defaultGridConf);
    optionsVLayout->addWidget(m_previewConf);
    optionsVLayout->addWidget(m_zoomSettingsWidget);
    optionsVLayout->addStretch(1);

    // Outer layout to give it horizontal stretch
    QHBoxLayout *optionsHLayout = new QHBoxLayout();
    optionsHLayout->addLayout(optionsVLayout);
    optionsHLayout->addStretch(1);
    optionsWidget->setLayout(optionsHLayout);

    return optionsWidget;
}

void FormEditorOptionsPage::apply()
{
    QDesignerSharedSettings settings(m_core);
    if (m_defaultGridConf) {
        const Grid defaultGrid = m_defaultGridConf->grid();
        settings.setDefaultGrid(defaultGrid);

        FormWindowBase::setDefaultDesignerGrid(defaultGrid);
        // Update grid settings in all existing form windows
        QDesignerFormWindowManagerInterface *fwm = m_core->formWindowManager();
        if (const int numWindows = fwm->formWindowCount()) {
            for (int i = 0; i < numWindows; i++)
                if (qdesigner_internal::FormWindowBase *fwb
                        = qobject_cast<qdesigner_internal::FormWindowBase *>( fwm->formWindow(i)))
                    if (!fwb->hasFormGrid())
                        fwb->setDesignerGrid(defaultGrid);
        }
    }
    if (m_previewConf) {
        m_previewConf->saveState();
    }

    if (m_zoomSettingsWidget)
        m_zoomSettingsWidget->toSettings(settings);
}

void FormEditorOptionsPage::finish()
{
}

}

QT_END_NAMESPACE

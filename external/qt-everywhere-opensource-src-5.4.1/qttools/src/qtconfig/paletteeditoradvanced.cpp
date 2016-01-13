/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the tools applications of the Qt Toolkit.
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
#include "ui_paletteeditoradvanced.h"
#include "paletteeditoradvanced.h"
#include "colorbutton.h"

QT_BEGIN_NAMESPACE

PaletteEditorAdvanced::PaletteEditorAdvanced(QWidget *parent)
    : QDialog(parent), ui(new Ui::PaletteEditorAdvanced), selectedPalette(0)
{
    ui->setupUi(this);

    // create a ColorButton's
    buttonCentral = new ColorButton(ui->groupCentral);
    buttonCentral->setToolTip(tr("Choose a color"));
    buttonCentral->setWhatsThis(tr("Choose a color for the selected central color role."));
    ui->layoutCentral->addWidget(buttonCentral);
    ui->labelCentral->setBuddy(buttonCentral);

    buttonEffect = new ColorButton(ui->groupEffect);
    buttonEffect->setToolTip(tr("Choose a color"));
    buttonEffect->setWhatsThis(tr("Choose a color for the selected effect color role."));
    buttonEffect->setEnabled(false);
    ui->layoutEffect->addWidget(buttonEffect);
    ui->labelEffect->setBuddy(buttonEffect);

    // signals and slots connections
    connect(ui->paletteCombo, SIGNAL(activated(int)), SLOT(paletteSelected(int)));
    connect(ui->comboCentral, SIGNAL(activated(int)), SLOT(onCentral(int)));
    connect(buttonCentral, SIGNAL(clicked()), SLOT(onChooseCentralColor()));
    connect(buttonEffect, SIGNAL(clicked()), SLOT(onChooseEffectColor()));
    connect(ui->comboEffect, SIGNAL(activated(int)), SLOT(onEffect(int)));
    connect(ui->checkBuildEffect, SIGNAL(toggled(bool)), SLOT(onToggleBuildEffects(bool)));
    connect(ui->checkBuildEffect, SIGNAL(toggled(bool)), buttonEffect, SLOT(setDisabled(bool)));
    connect(ui->checkBuildInactive, SIGNAL(toggled(bool)), SLOT(onToggleBuildInactive(bool)));
    connect(ui->checkBuildDisabled, SIGNAL(toggled(bool)), SLOT(onToggleBuildDisabled(bool)));

    onToggleBuildEffects(true);

    editPalette = QApplication::palette();
}

PaletteEditorAdvanced::~PaletteEditorAdvanced()
{
    delete ui;
}

void PaletteEditorAdvanced::onToggleBuildInactive(bool v)
{
    if (selectedPalette == 1) {
        ui->groupCentral->setDisabled(v);
        ui->groupEffect->setDisabled(v);
    }

    if (v) {
        build(QPalette::Inactive);
        updateColorButtons();
    }
}

void PaletteEditorAdvanced::onToggleBuildDisabled(bool v)
{
    if (selectedPalette == 2) {
        ui->groupCentral->setDisabled(v);
        ui->groupEffect->setDisabled(v);
    }

    if (v) {
        build(QPalette::Disabled);
        updateColorButtons();
    }
}

void PaletteEditorAdvanced::paletteSelected(int p)
{
    selectedPalette = p;

    if(p == 1) { // inactive
        ui->groupCentral->setDisabled(ui->checkBuildInactive->isChecked());
        ui->groupEffect->setDisabled(ui->checkBuildInactive->isChecked());
    } else if (p == 2) { // disabled
        ui->groupCentral->setDisabled(ui->checkBuildDisabled->isChecked());
        ui->groupEffect->setDisabled(ui->checkBuildDisabled->isChecked());
    } else {
        ui->groupCentral->setEnabled(true);
        ui->groupEffect->setEnabled(true);
    }
    updateColorButtons();
}

void PaletteEditorAdvanced::onChooseCentralColor()
{
    QPalette::ColorGroup group = groupFromIndex(selectedPalette);
    editPalette.setColor(group, centralFromIndex(ui->comboCentral->currentIndex()),
                         buttonCentral->color());

    buildEffect(group);
    if (group == QPalette::Active) {
        if(ui->checkBuildInactive->isChecked())
            build(QPalette::Inactive);
        if(ui->checkBuildDisabled->isChecked())
            build(QPalette::Disabled);
    }

    updateColorButtons();
}

void PaletteEditorAdvanced::onChooseEffectColor()
{
    QPalette::ColorGroup group = groupFromIndex(selectedPalette);
    editPalette.setColor(group, effectFromIndex(ui->comboEffect->currentIndex()),
                         buttonEffect->color());

    if (group == QPalette::Active) {
        if(ui->checkBuildInactive->isChecked())
            build(QPalette::Inactive);
        if(ui->checkBuildDisabled->isChecked())
            build(QPalette::Disabled);
    }

    updateColorButtons();
}

void PaletteEditorAdvanced::onToggleBuildEffects(bool on)
{
    if (on) {
        for (int i = 0; i < QPalette::NColorGroups; i++)
            buildEffect(QPalette::ColorGroup(i));
    }
}

QPalette::ColorGroup PaletteEditorAdvanced::groupFromIndex(int item)
{
    switch (item) {
    case 0:
    default:
        return QPalette::Active;
    case 1:
        return QPalette::Inactive;
    case 2:
        return QPalette::Disabled;
    }
}

QPalette::ColorRole PaletteEditorAdvanced::centralFromIndex(int item)
{
    switch (item) {
    case 0:
        return QPalette::Window;
    case 1:
        return QPalette::WindowText;
    case 2:
        return QPalette::Base;
    case 3:
        return QPalette::AlternateBase;
    case 4:
        return QPalette::ToolTipBase;
    case 5:
        return QPalette::ToolTipText;
    case 6:
        return QPalette::Text;
    case 7:
        return QPalette::Button;
    case 8:
        return QPalette::ButtonText;
    case 9:
        return QPalette::BrightText;
    case 10:
        return QPalette::Highlight;
    case 11:
        return QPalette::HighlightedText;
    case 12:
        return QPalette::Link;
    case 13:
        return QPalette::LinkVisited;
    default:
        return QPalette::NoRole;
    }
}

QPalette::ColorRole PaletteEditorAdvanced::effectFromIndex(int item)
{
    switch (item) {
    case 0:
        return QPalette::Light;
    case 1:
        return QPalette::Midlight;
    case 2:
        return QPalette::Mid;
    case 3:
        return QPalette::Dark;
    case 4:
        return QPalette::Shadow;
    default:
        return QPalette::NoRole;
    }
}

void PaletteEditorAdvanced::onCentral(int item)
{
    QColor c = editPalette.color(groupFromIndex(selectedPalette), centralFromIndex(item));
    buttonCentral->setColor(c);
}

void PaletteEditorAdvanced::onEffect(int item)
{
    QColor c = editPalette.color(groupFromIndex(selectedPalette), effectFromIndex(item));
    buttonEffect->setColor(c);
}

QPalette PaletteEditorAdvanced::buildEffect(QPalette::ColorGroup colorGroup,
                                            const QPalette &basePalette)
{
    QPalette result(basePalette);

    if (colorGroup == QPalette::Active) {
        QPalette calculatedPalette(basePalette.color(colorGroup, QPalette::Button),
                                   basePalette.color(colorGroup, QPalette::Window));

        for (int i = 0; i < 5; i++) {
            QPalette::ColorRole effectRole = effectFromIndex(i);
            result.setColor(colorGroup, effectRole,
                            calculatedPalette.color(colorGroup, effectRole));
        }
    } else {
        QColor btn = basePalette.color(colorGroup, QPalette::Button);

        result.setColor(colorGroup, QPalette::Light, btn.lighter());
        result.setColor(colorGroup, QPalette::Midlight, btn.lighter(115));
        result.setColor(colorGroup, QPalette::Mid, btn.darker(150));
        result.setColor(colorGroup, QPalette::Dark, btn.darker());
        result.setColor(colorGroup, QPalette::Shadow, Qt::black);
    }

    return result;
}

void PaletteEditorAdvanced::buildEffect(QPalette::ColorGroup colorGroup)
{
    editPalette = buildEffect(colorGroup, editPalette);
    updateColorButtons();
}

void PaletteEditorAdvanced::build(QPalette::ColorGroup colorGroup)
{
    if (colorGroup != QPalette::Active) {
        for (int i = 0; i < QPalette::NColorRoles; i++)
            editPalette.setColor(colorGroup, QPalette::ColorRole(i),
                                 editPalette.color(QPalette::Active, QPalette::ColorRole(i)));

        if (colorGroup == QPalette::Disabled) {
            editPalette.setColor(colorGroup, QPalette::ButtonText, Qt::darkGray);
            editPalette.setColor(colorGroup, QPalette::WindowText, Qt::darkGray);
            editPalette.setColor(colorGroup, QPalette::Text, Qt::darkGray);
            editPalette.setColor(colorGroup, QPalette::HighlightedText, Qt::darkGray);
        }

        if (ui->checkBuildEffect->isChecked())
            buildEffect(colorGroup);
        else
            updateColorButtons();
    }
}

void PaletteEditorAdvanced::updateColorButtons()
{
    QPalette::ColorGroup colorGroup = groupFromIndex(selectedPalette);
    buttonCentral->setColor(editPalette.color(colorGroup,
                                              centralFromIndex(ui->comboCentral->currentIndex())));
    buttonEffect->setColor(editPalette.color(colorGroup,
                                             effectFromIndex(ui->comboEffect->currentIndex())));
}

void PaletteEditorAdvanced::setPal(const QPalette &pal)
{
    editPalette = pal;
    updateColorButtons();
}

QPalette PaletteEditorAdvanced::pal() const
{
    return editPalette;
}

void PaletteEditorAdvanced::setupBackgroundRole(QPalette::ColorRole role)
{
    int initRole = 0;

    switch (role) {
    case QPalette::Window:
        initRole = 0;
        break;
    case QPalette::WindowText:
        initRole = 1;
        break;
    case QPalette::Base:
        initRole = 2;
        break;
    case QPalette::AlternateBase:
        initRole = 3;
        break;
    case QPalette::ToolTipBase:
        initRole = 4;
        break;
    case QPalette::ToolTipText:
        initRole = 5;
        break;
    case QPalette::Text:
        initRole = 6;
        break;
    case QPalette::Button:
        initRole = 7;
        break;
    case QPalette::ButtonText:
        initRole = 8;
        break;
    case QPalette::BrightText:
        initRole = 9;
        break;
    case QPalette::Highlight:
        initRole = 10;
        break;
    case QPalette::HighlightedText:
        initRole = 11;
        break;
    case QPalette::Link:
        initRole = 12;
        break;
    case QPalette::LinkVisited:
        initRole = 13;
        break;
    default:
        initRole = -1;
        break;
    }

    if (initRole != -1)
        ui->comboCentral->setCurrentIndex(initRole);
}

QPalette PaletteEditorAdvanced::getPalette(bool *ok, const QPalette &init,
                                           QPalette::ColorRole backgroundRole, QWidget *parent)
{
    PaletteEditorAdvanced *dlg = new PaletteEditorAdvanced(parent);
    dlg->setupBackgroundRole(backgroundRole);

    if (init != QPalette())
        dlg->setPal(init);
    int resultCode = dlg->exec();

    QPalette result = init;
    if (resultCode == QDialog::Accepted)
        result = dlg->pal();

    if (ok)
        *ok = resultCode;

    delete dlg;
    return result;
}

QT_END_NAMESPACE

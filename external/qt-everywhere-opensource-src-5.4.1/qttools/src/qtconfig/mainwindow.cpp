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

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "colorbutton.h"
#include "previewframe.h"
#include "paletteeditoradvanced.h"

#include <QLabel>
#include <QApplication>
#include <QComboBox>
#include <QStyleFactory>
#include <QFontDatabase>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QFileDialog>
#include <QAction>
#include <QStatusBar>
#include <QSettings>
#include <QMessageBox>
#include <QStyle>
#include <QtEvents>
#include <QInputContext>
#include <QtDebug>
#include <QPixmap>

#include <stdlib.h>

QT_BEGIN_NAMESPACE

// from qapplication.cpp and qapplication_x11.cpp - These are NOT for
// external use ignore them
// extern bool Q_CORE_EXPORT qt_resolve_symlinks;

static const char *appearance_text = QT_TRANSLATE_NOOP("MainWindow",
"<p><b><font size+=2>Appearance</font></b></p>"
"<hr>"
"<p>Use this tab to customize the appearance of your Qt applications.</p>"
"<p>You can select the default GUI Style from the drop down list and "
"customize the colors.</p>"
"<p>Any GUI Style plugins in your plugin path will automatically be added "
"to the list of built-in Qt styles. (See the Library Paths tab for "
"information on adding new plugin paths.)</p>"
"<p>When you choose 3-D Effects and Window Background colors, the Qt "
"Configuration program will automatically generate a palette for you. "
"To customize colors further, press the Tune Palette button to open "
"the advanced palette editor."
"<p>The Preview Window shows what the selected Style and colors look "
"like.");

static const char *font_text = QT_TRANSLATE_NOOP("MainWindow",
"<p><b><font size+=2>Fonts</font></b></p>"
"<hr>"
"<p>Use this tab to select the default font for your Qt applications. "
"The selected font is shown (initially as 'Sample Text') in the line "
"edit below the Family, "
"Style and Point Size drop down lists.</p>"
"<p>Qt has a powerful font substitution feature that allows you to "
"specify a list of substitute fonts.  Substitute fonts are used "
"when a font cannot be loaded, or if the specified font doesn't have "
"a particular character."
"<p>For example, if you select the font Lucida, which doesn't have Korean "
"characters, but need to show some Korean text using the Mincho font family "
"you can do so by adding Mincho to the list. Once Mincho is added, any "
"Korean characters that are not found in the Lucida font will be taken "
"from the Mincho font.  Because the font substitutions are "
"lists, you can also select multiple families, such as Song Ti (for "
"use with Chinese text).");

static const char *interface_text = QT_TRANSLATE_NOOP("MainWindow",
"<p><b><font size+=2>Interface</font></b></p>"
"<hr>"
"<p>Use this tab to customize the feel of your Qt applications.</p>"
"<p>If the Resolve Symlinks checkbox is checked Qt will follow symlinks "
"when handling URLs. For example, in the file dialog, if this setting is turned "
"on and /usr/tmp is a symlink to /var/tmp, entering the /usr/tmp directory "
"will cause the file dialog to change to /var/tmp.  With this setting turned "
"off, symlinks are not resolved or followed.</p>"
"<p>The Global Strut setting is useful for people who require a "
"minimum size for all widgets (e.g. when using a touch panel or for users "
"who are visually impaired).  Leaving the Global Strut width and height "
"at 0 will disable the Global Strut feature</p>"
"<p>XIM (Extended Input Methods) are used for entering characters in "
"languages that have large character sets, for example, Chinese and "
"Japanese.");
// ### What does the 'Enhanced support for languages written R2L do?

static const char *printer_text = QT_TRANSLATE_NOOP("MainWindow",
"<p><b><font size+=2>Printer</font></b></p>"
"<hr>"
"<p>Use this tab to configure the way Qt generates output for the printer."
"You can specify if Qt should try to embed fonts into its generated output."
"If you enable font embedding, the resulting postscript will be more "
"portable and will more accurately reflect the "
"visual output on the screen; however the resulting postscript file "
"size will be bigger."
"<p>When using font embedding you can select additional directories where "
"Qt should search for embeddable font files.  By default, the X "
"server font path is used.");

QPalette::ColorGroup MainWindow::groupFromIndex(int item)
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

static void setStyleHelper(QWidget *w, QStyle *s)
{
    const QObjectList children = w->children();
    for (int i = 0; i < children.size(); ++i) {
        QObject *child = children.at(i);
        if (child->isWidgetType())
            setStyleHelper((QWidget *) child, s);
    }
    w->setStyle(s);
}

MainWindow::MainWindow()
    : ui(new Ui::MainWindow),
      editPalette(palette()),
      previewPalette(palette()),
      previewstyle(0)
{
    ui->setupUi(this);
    statusBar();

    // signals and slots connections
    connect(ui->fontPathLineEdit, SIGNAL(returnPressed()), SLOT(addFontpath()));
    connect(ui->addFontPathButton, SIGNAL(clicked()), SLOT(addFontpath()));
    connect(ui->addSubstitutionButton, SIGNAL(clicked()), SLOT(addSubstitute()));
    connect(ui->browseFontPathButton, SIGNAL(clicked()), SLOT(browseFontpath()));
    connect(ui->fontStyleCombo, SIGNAL(activated(int)), SLOT(buildFont()));
    connect(ui->pointSizeCombo, SIGNAL(activated(int)), SLOT(buildFont()));
    connect(ui->downFontpathButton, SIGNAL(clicked()), SLOT(downFontpath()));
    connect(ui->downSubstitutionButton, SIGNAL(clicked()), SLOT(downSubstitute()));
    connect(ui->fontFamilyCombo, SIGNAL(activated(QString)), SLOT(familySelected(QString)));
    connect(ui->fileExitAction, SIGNAL(triggered()), SLOT(fileExit()));
    connect(ui->fileSaveAction, SIGNAL(triggered()), SLOT(fileSave()));
    connect(ui->helpAboutAction, SIGNAL(triggered()), SLOT(helpAbout()));
    connect(ui->helpAboutQtAction, SIGNAL(triggered()), SLOT(helpAboutQt()));
    connect(ui->mainTabWidget, SIGNAL(currentChanged(int)), SLOT(pageChanged(int)));
    connect(ui->paletteCombo, SIGNAL(activated(int)), SLOT(paletteSelected(int)));
    connect(ui->removeFontpathButton, SIGNAL(clicked()), SLOT(removeFontpath()));
    connect(ui->removeSubstitutionButton, SIGNAL(clicked()), SLOT(removeSubstitute()));
    connect(ui->toolBoxEffectCombo, SIGNAL(activated(int)), SLOT(somethingModified()));
    connect(ui->doubleClickIntervalSpinBox, SIGNAL(valueChanged(int)), SLOT(somethingModified()));
    connect(ui->cursorFlashTimeSpinBox, SIGNAL(valueChanged(int)), SLOT(somethingModified()));
    connect(ui->wheelScrollLinesSpinBox, SIGNAL(valueChanged(int)), SLOT(somethingModified()));
    connect(ui->menuEffectCombo, SIGNAL(activated(int)), SLOT(somethingModified()));
    connect(ui->comboEffectCombo, SIGNAL(activated(int)), SLOT(somethingModified()));
    connect(ui->toolTipEffectCombo, SIGNAL(activated(int)), SLOT(somethingModified()));
    connect(ui->strutWidthSpinBox, SIGNAL(valueChanged(int)), SLOT(somethingModified()));
    connect(ui->strutHeightSpinBox, SIGNAL(valueChanged(int)), SLOT(somethingModified()));
    connect(ui->effectsCheckBox, SIGNAL(toggled(bool)), SLOT(somethingModified()));
    connect(ui->resolveLinksCheckBox, SIGNAL(toggled(bool)), SLOT(somethingModified()));
    connect(ui->fontEmbeddingCheckBox, SIGNAL(clicked()), SLOT(somethingModified()));
    connect(ui->rtlExtensionsCheckBox, SIGNAL(toggled(bool)), SLOT(somethingModified()));
    connect(ui->inputStyleCombo, SIGNAL(activated(int)), SLOT(somethingModified()));
    connect(ui->inputMethodCombo, SIGNAL(activated(int)), SLOT(somethingModified()));
    connect(ui->guiStyleCombo, SIGNAL(activated(QString)), SLOT(styleSelected(QString)));
    connect(ui->familySubstitutionCombo, SIGNAL(activated(QString)), SLOT(substituteSelected(QString)));
    connect(ui->tunePaletteButton, SIGNAL(clicked()), SLOT(tunePalette()));
    connect(ui->upFontpathButton, SIGNAL(clicked()), SLOT(upFontpath()));
    connect(ui->upSubstitutionButton, SIGNAL(clicked()), SLOT(upSubstitute()));

    modified = true;
    desktopThemeName = tr("Desktop Settings (Default)");
    setWindowIcon(QPixmap(":/qt-project.org/qtconfig/images/appicon.png"));
    QStringList gstyles = QStyleFactory::keys();
    gstyles.sort();
    ui->guiStyleCombo->addItem(desktopThemeName);
    ui->guiStyleCombo->setItemData(ui->guiStyleCombo->findText(desktopThemeName),
                                   tr("Choose style and palette based on your desktop settings."),
                                   Qt::ToolTipRole);
    ui->guiStyleCombo->addItems(gstyles);

    QSettings settings(QLatin1String("QtProject"));
    settings.beginGroup(QLatin1String("Qt"));

    QString currentstyle = settings.value(QLatin1String("style")).toString();
    if (currentstyle.isEmpty()) {
        ui->guiStyleCombo->setCurrentIndex(ui->guiStyleCombo->findText(desktopThemeName));
        currentstyle = QApplication::style()->objectName();
    } else {
        int index = ui->guiStyleCombo->findText(currentstyle, Qt::MatchFixedString);
        if (index != -1) {
            ui->guiStyleCombo->setCurrentIndex(index);
        } else { // we give up
            ui->guiStyleCombo->addItem(tr("Unknown"));
            ui->guiStyleCombo->setCurrentIndex(ui->guiStyleCombo->count() - 1);
        }
    }
    ui->buttonMainColor->setColor(palette().color(QPalette::Active, QPalette::Button));
    ui->buttonWindowColor->setColor(palette().color(QPalette::Active, QPalette::Window));
    connect(ui->buttonMainColor, SIGNAL(colorChanged(QColor)), SLOT(buildPalette()));
    connect(ui->buttonWindowColor, SIGNAL(colorChanged(QColor)), SLOT(buildPalette()));

#ifdef Q_WS_X11
    if (X11->desktopEnvironment == DE_KDE)
        ui->colorConfig->hide();
    else
        ui->kdeNoteLabel->hide();
#else
    ui->colorConfig->hide();
    ui->kdeNoteLabel->hide();
#endif

    QFontDatabase db;
    QStringList families = db.families();
    ui->fontFamilyCombo->addItems(families);

    QStringList fs = families;
    QStringList fs2 = QFont::substitutions();
    QStringList::Iterator fsit = fs2.begin();
    while (fsit != fs2.end()) {
        if (!fs.contains(*fsit))
            fs += *fsit;
        fsit++;
    }
    fs.sort();
    ui->familySubstitutionCombo->addItems(fs);

    ui->chooseSubstitutionCombo->addItems(families);
    QList<int> sizes = db.standardSizes();
    foreach(int i, sizes)
        ui->pointSizeCombo->addItem(QString::number(i));

    ui->doubleClickIntervalSpinBox->setValue(QApplication::doubleClickInterval());
    ui->cursorFlashTimeSpinBox->setValue(QApplication::cursorFlashTime());
    ui->wheelScrollLinesSpinBox->setValue(QApplication::wheelScrollLines());
    // #############
    // resolveLinksCheckBox->setChecked(qt_resolve_symlinks);

    ui->effectsCheckBox->setChecked(QApplication::isEffectEnabled(Qt::UI_General));
    ui->effectsFrame->setEnabled(ui->effectsCheckBox->isChecked());

    if (QApplication::isEffectEnabled(Qt::UI_FadeMenu))
        ui->menuEffectCombo->setCurrentIndex(2);
    else if (QApplication::isEffectEnabled(Qt::UI_AnimateMenu))
        ui->menuEffectCombo->setCurrentIndex(1);

    if (QApplication::isEffectEnabled(Qt::UI_AnimateCombo))
        ui->comboEffectCombo->setCurrentIndex(1);

    if (QApplication::isEffectEnabled(Qt::UI_FadeTooltip))
        ui->toolTipEffectCombo->setCurrentIndex(2);
    else if (QApplication::isEffectEnabled(Qt::UI_AnimateTooltip))
        ui->toolTipEffectCombo->setCurrentIndex(1);

    if (QApplication::isEffectEnabled(Qt::UI_AnimateToolBox))
        ui->toolBoxEffectCombo->setCurrentIndex(1);

    QSize globalStrut = QApplication::globalStrut();
    ui->strutWidthSpinBox->setValue(globalStrut.width());
    ui->strutHeightSpinBox->setValue(globalStrut.height());

    // find the default family
    QStringList::Iterator sit = families.begin();
    int i = 0, possible = -1;
    while (sit != families.end()) {
        if (*sit == QApplication::font().family())
            break;
        if ((*sit).contains(QApplication::font().family()))
            possible = i;

        i++;
        sit++;
    }
    if (sit == families.end())
        i = possible;
    if (i == -1) // no clue about the current font
        i = 0;

    ui->fontFamilyCombo->setCurrentIndex(i);

    QStringList styles = db.styles(ui->fontFamilyCombo->currentText());
    ui->fontStyleCombo->addItems(styles);

    QString stylestring = db.styleString(QApplication::font());
    sit = styles.begin();
    i = 0;
    possible = -1;
    while (sit != styles.end()) {
        if (*sit == stylestring)
            break;
        if ((*sit).contains(stylestring))
            possible = i;

        i++;
        sit++;
    }
    if (sit == styles.end())
        i = possible;
    if (i == -1) // no clue about the current font
        i = 0;
    ui->fontStyleCombo->setCurrentIndex(i);

    i = 0;
    for (int psize = QApplication::font().pointSize(); i < ui->pointSizeCombo->count(); ++i) {
        const int sz = ui->pointSizeCombo->itemText(i).toInt();
        if (sz == psize) {
            ui->pointSizeCombo->setCurrentIndex(i);
            break;
        } else if(sz > psize) {
            ui->pointSizeCombo->insertItem(i, QString::number(psize));
            ui->pointSizeCombo->setCurrentIndex(i);
            break;
        }
    }

    QStringList subs = QFont::substitutes(ui->familySubstitutionCombo->currentText());
    ui->substitutionsListBox->clear();
    ui->substitutionsListBox->insertItems(0, subs);

    ui->rtlExtensionsCheckBox->setChecked(settings.value(QLatin1String("useRtlExtensions"), false)
                                          .toBool());

#ifdef Q_WS_X11
    QString settingsInputStyle = settings.value(QLatin1String("XIMInputStyle")).toString();
    if (!settingsInputStyle.isEmpty())
      ui->inputStyleCombo->setCurrentIndex(ui->inputStyleCombo->findText(settingsInputStyle));
#else
    ui->inputStyleCombo->hide();
    ui->inputStyleLabel->hide();
#endif

#if defined(Q_WS_X11) && !defined(QT_NO_XIM)
    QStringList inputMethodCombo = QInputContextFactory::keys();
    int inputMethodComboIndex = -1;
    QString defaultInputMethod = settings.value(QLatin1String("DefaultInputMethod"), QLatin1String("xim")).toString();
    for (int i = inputMethodCombo.size()-1; i >= 0; --i) {
        const QString &im = inputMethodCombo.at(i);
        if (im.contains(QLatin1String("imsw"))) {
            inputMethodCombo.removeAt(i);
            if (inputMethodComboIndex > i)
                --inputMethodComboIndex;
        } else if (im == defaultInputMethod) {
            inputMethodComboIndex = i;
        }
    }
    if (inputMethodComboIndex == -1 && !inputMethodCombo.isEmpty())
        inputMethodComboIndex = 0;
    ui->inputMethodCombo->addItems(inputMethodCombo);
    ui->inputMethodCombo->setCurrentIndex(inputMethodComboIndex);
#else
    ui->inputMethodCombo->hide();
    ui->inputMethodLabel->hide();
#endif

    ui->fontEmbeddingCheckBox->setChecked(settings.value(QLatin1String("embedFonts"), true)
                                          .toBool());
    fontpaths = settings.value(QLatin1String("fontPath")).toStringList();
    ui->fontpathListBox->insertItems(0, fontpaths);

    settings.endGroup(); // Qt

    ui->helpView->setText(tr(appearance_text));

    setModified(false);
    updateStyleLayout();
}

MainWindow::~MainWindow()
{
    delete ui;
}

#ifdef Q_WS_X11
extern void qt_x11_apply_settings_in_all_apps();
#endif

void MainWindow::fileSave()
{
    if (! modified) {
        statusBar()->showMessage(tr("No changes to be saved."), 2000);
        return;
    }

    statusBar()->showMessage(tr("Saving changes..."));

    {
        QSettings settings(QLatin1String("QtProject"));
        settings.beginGroup(QLatin1String("Qt"));
        QFontDatabase db;
        QFont font = db.font(ui->fontFamilyCombo->currentText(),
                             ui->fontStyleCombo->currentText(),
                             ui->pointSizeCombo->currentText().toInt());

        QStringList actcg, inactcg, discg;
        bool overrideDesktopSettings = (ui->guiStyleCombo->currentText() != desktopThemeName);
        if (overrideDesktopSettings) {
            int i;
            for (i = 0; i < QPalette::NColorRoles; i++)
                actcg << editPalette.color(QPalette::Active,
                                           QPalette::ColorRole(i)).name();
            for (i = 0; i < QPalette::NColorRoles; i++)
                inactcg << editPalette.color(QPalette::Inactive,
                                             QPalette::ColorRole(i)).name();
            for (i = 0; i < QPalette::NColorRoles; i++)
                discg << editPalette.color(QPalette::Disabled,
                                           QPalette::ColorRole(i)).name();
        }

        settings.setValue(QLatin1String("font"), font.toString());
        settings.setValue(QLatin1String("Palette/active"), actcg);
        settings.setValue(QLatin1String("Palette/inactive"), inactcg);
        settings.setValue(QLatin1String("Palette/disabled"), discg);

        settings.setValue(QLatin1String("fontPath"), fontpaths);
        settings.setValue(QLatin1String("embedFonts"), ui->fontEmbeddingCheckBox->isChecked());
        settings.setValue(QLatin1String("style"),
                          overrideDesktopSettings ? ui->guiStyleCombo->currentText() : QString());

        settings.setValue(QLatin1String("doubleClickInterval"), ui->doubleClickIntervalSpinBox->value());
        settings.setValue(QLatin1String("cursorFlashTime"),
                          ui->cursorFlashTimeSpinBox->value() == 9 ? 0 : ui->cursorFlashTimeSpinBox->value());
        settings.setValue(QLatin1String("wheelScrollLines"), ui->wheelScrollLinesSpinBox->value());
        settings.setValue(QLatin1String("resolveSymlinks"), ui->resolveLinksCheckBox->isChecked());

        QSize strut(ui->strutWidthSpinBox->value(), ui->strutHeightSpinBox->value());
        settings.setValue(QLatin1String("globalStrut/width"), strut.width());
        settings.setValue(QLatin1String("globalStrut/height"), strut.height());

        settings.setValue(QLatin1String("useRtlExtensions"), ui->rtlExtensionsCheckBox->isChecked());

#ifdef Q_WS_X11
        QString style = ui->inputStyleCombo->currentText();
        QString str = QLatin1String("On The Spot");
        if (style == tr("Over The Spot"))
            str = QLatin1String("Over The Spot");
        else if (style == tr("Off The Spot"))
            str = QLatin1String("Off The Spot");
        else if (style == tr("Root"))
            str = QLatin1String("Root");
        settings.setValue(QLatin1String("XIMInputStyle"), str);
#endif
#if defined(Q_WS_X11) && !defined(QT_NO_XIM)
        settings.setValue(QLatin1String("DefaultInputMethod"), ui->inputMethodCombo->currentText());
#endif

        QStringList effects;
        if (ui->effectsCheckBox->isChecked()) {
            effects << QLatin1String("general");

            switch (ui->menuEffectCombo->currentIndex()) {
            case 1: effects << QLatin1String("animatemenu"); break;
            case 2: effects << QLatin1String("fademenu"); break;
            }

            switch (ui->comboEffectCombo->currentIndex()) {
            case 1: effects << QLatin1String("animatecombo"); break;
            }

            switch (ui->toolTipEffectCombo->currentIndex()) {
            case 1: effects << QLatin1String("animatetooltip"); break;
            case 2: effects << QLatin1String("fadetooltip"); break;
            }

            switch (ui->toolBoxEffectCombo->currentIndex()) {
            case 1: effects << QLatin1String("animatetoolbox"); break;
            }
        } else
            effects << QLatin1String("none");
        settings.setValue(QLatin1String("GUIEffects"), effects);

        QStringList familysubs = QFont::substitutions();
        QStringList::Iterator fit = familysubs.begin();
        settings.beginGroup(QLatin1String("Font Substitutions"));
        while (fit != familysubs.end()) {
            QStringList subs = QFont::substitutes(*fit);
            settings.setValue(*fit, subs);
            fit++;
        }
        settings.endGroup(); // Font Substitutions
        settings.endGroup(); // Qt
    }

#if defined(Q_WS_X11)
    qt_x11_apply_settings_in_all_apps();
#endif // Q_WS_X11

    setModified(false);
    statusBar()->showMessage(tr("Saved changes."));
}

void MainWindow::fileExit()
{
    qApp->closeAllWindows();
}

void MainWindow::setModified(bool m)
{
    if (modified == m)
        return;

    modified = m;
    ui->fileSaveAction->setEnabled(m);
}

void MainWindow::buildPalette()
{
    QPalette temp(ui->buttonMainColor->color(), ui->buttonWindowColor->color());
    for (int i = 0; i < QPalette::NColorGroups; i++)
        temp = PaletteEditorAdvanced::buildEffect(QPalette::ColorGroup(i), temp);

    editPalette = temp;
    setPreviewPalette(editPalette);
    updateColorButtons();

    setModified(true);
}

void MainWindow::setPreviewPalette(const QPalette &pal)
{
    QPalette::ColorGroup colorGroup = groupFromIndex(ui->paletteCombo->currentIndex());

    for (int i = 0; i < QPalette::NColorGroups; i++) {
        for (int j = 0; j < QPalette::NColorRoles; j++) {
            QPalette::ColorGroup targetGroup = QPalette::ColorGroup(i);
            QPalette::ColorRole targetRole = QPalette::ColorRole(j);
            previewPalette.setColor(targetGroup, targetRole, pal.color(colorGroup, targetRole));
        }
    }

    ui->previewFrame->setPreviewPalette(previewPalette);
}

void MainWindow::updateColorButtons()
{
    ui->buttonMainColor->setColor(editPalette.color(QPalette::Active, QPalette::Button));
    ui->buttonWindowColor->setColor(editPalette.color(QPalette::Active, QPalette::Window));
}

void MainWindow::tunePalette()
{
    bool ok;
    QPalette pal = PaletteEditorAdvanced::getPalette(&ok, editPalette,
                                                     backgroundRole(), this);
    if (!ok)
        return;

    editPalette = pal;
    setPreviewPalette(editPalette);
    setModified(true);
}

void MainWindow::paletteSelected(int)
{
    setPreviewPalette(editPalette);
}

void MainWindow::updateStyleLayout()
{
    QString currentStyle = ui->guiStyleCombo->currentText();
    bool autoStyle = (currentStyle == desktopThemeName);
    ui->previewFrame->setPreviewVisible(!autoStyle);
    ui->buildPaletteGroup->setEnabled(currentStyle.toLower() != QLatin1String("gtk") && !autoStyle);
}

void MainWindow::styleSelected(const QString &stylename)
{
    QStyle *style = 0;
    if (stylename == desktopThemeName) {
        setModified(true);
    } else {
        style = QStyleFactory::create(stylename);
        if (!style)
            return;
        setStyleHelper(ui->previewFrame, style);
        delete previewstyle;
        previewstyle = style;
        setModified(true);
    }
    updateStyleLayout();
}

void MainWindow::familySelected(const QString &family)
{
    QFontDatabase db;
    QStringList styles = db.styles(family);
    ui->fontStyleCombo->clear();
    ui->fontStyleCombo->addItems(styles);
    ui->familySubstitutionCombo->addItem(family);
    buildFont();
}

void MainWindow::buildFont()
{
    QFontDatabase db;
    QFont font = db.font(ui->fontFamilyCombo->currentText(),
                         ui->fontStyleCombo->currentText(),
                         ui->pointSizeCombo->currentText().toInt());
    ui->sampleLineEdit->setFont(font);
    setModified(true);
}

void MainWindow::substituteSelected(const QString &family)
{
    QStringList subs = QFont::substitutes(family);
    ui->substitutionsListBox->clear();
    ui->substitutionsListBox->insertItems(0, subs);
}

void MainWindow::removeSubstitute()
{
    if (!ui->substitutionsListBox->currentItem())
        return;

    int row = ui->substitutionsListBox->currentRow();
    QStringList subs = QFont::substitutes(ui->familySubstitutionCombo->currentText());
    subs.removeAt(ui->substitutionsListBox->currentRow());
    ui->substitutionsListBox->clear();
    ui->substitutionsListBox->insertItems(0, subs);
    if (row > ui->substitutionsListBox->count())
        row = ui->substitutionsListBox->count() - 1;
    ui->substitutionsListBox->setCurrentRow(row);
    QFont::removeSubstitutions(ui->familySubstitutionCombo->currentText());
    QFont::insertSubstitutions(ui->familySubstitutionCombo->currentText(), subs);
    setModified(true);
}

void MainWindow::addSubstitute()
{
    if (!ui->substitutionsListBox->currentItem()) {
        QFont::insertSubstitution(ui->familySubstitutionCombo->currentText(),
                                  ui->chooseSubstitutionCombo->currentText());
        QStringList subs = QFont::substitutes(ui->familySubstitutionCombo->currentText());
        ui->substitutionsListBox->clear();
        ui->substitutionsListBox->insertItems(0, subs);
        setModified(true);
        return;
    }

    int row = ui->substitutionsListBox->currentRow();
    QFont::insertSubstitution(ui->familySubstitutionCombo->currentText(), ui->chooseSubstitutionCombo->currentText());
    QStringList subs = QFont::substitutes(ui->familySubstitutionCombo->currentText());
    ui->substitutionsListBox->clear();
    ui->substitutionsListBox->insertItems(0, subs);
    ui->substitutionsListBox->setCurrentRow(row);
    setModified(true);
}

void MainWindow::downSubstitute()
{
    if (!ui->substitutionsListBox->currentItem() || ui->substitutionsListBox->currentRow() >= ui->substitutionsListBox->count())
        return;

    int row = ui->substitutionsListBox->currentRow();
    QStringList subs = QFont::substitutes(ui->familySubstitutionCombo->currentText());
    QString fam = subs.at(row);
    subs.removeAt(row);
    subs.insert(row + 1, fam);
    ui->substitutionsListBox->clear();
    ui->substitutionsListBox->insertItems(0, subs);
    ui->substitutionsListBox->setCurrentRow(row + 1);
    QFont::removeSubstitutions(ui->familySubstitutionCombo->currentText());
    QFont::insertSubstitutions(ui->familySubstitutionCombo->currentText(), subs);
    setModified(true);
}

void MainWindow::upSubstitute()
{
    if (!ui->substitutionsListBox->currentItem() || ui->substitutionsListBox->currentRow() < 1)
        return;

    int row = ui->substitutionsListBox->currentRow();
    QStringList subs = QFont::substitutes(ui->familySubstitutionCombo->currentText());
    QString fam = subs.at(row);
    subs.removeAt(row);
    subs.insert(row-1, fam);
    ui->substitutionsListBox->clear();
    ui->substitutionsListBox->insertItems(0, subs);
    ui->substitutionsListBox->setCurrentRow(row - 1);
    QFont::removeSubstitutions(ui->familySubstitutionCombo->currentText());
    QFont::insertSubstitutions(ui->familySubstitutionCombo->currentText(), subs);
    setModified(true);
}

void MainWindow::removeFontpath()
{
    if (!ui->fontpathListBox->currentItem())
        return;

    int row = ui->fontpathListBox->currentRow();
    fontpaths.removeAt(row);
    ui->fontpathListBox->clear();
    ui->fontpathListBox->insertItems(0, fontpaths);
    if (row > ui->fontpathListBox->count())
        row = ui->fontpathListBox->count() - 1;
    ui->fontpathListBox->setCurrentRow(row);
    setModified(true);
}

void MainWindow::addFontpath()
{
    if (ui->fontPathLineEdit->text().isEmpty())
        return;

    if (!ui->fontpathListBox->currentItem()) {
        fontpaths.append(ui->fontPathLineEdit->text());
        ui->fontpathListBox->clear();
        ui->fontpathListBox->insertItems(0, fontpaths);
        setModified(true);

        return;
    }

    int row = ui->fontpathListBox->currentRow();
    fontpaths.insert(row + 1, ui->fontPathLineEdit->text());
    ui->fontpathListBox->clear();
    ui->fontpathListBox->insertItems(0, fontpaths);
    ui->fontpathListBox->setCurrentRow(row);
    setModified(true);
}

void MainWindow::downFontpath()
{
    if (!ui->fontpathListBox->currentItem()
        || ui->fontpathListBox->currentRow() >= (ui->fontpathListBox->count() - 1)) {
        return;
    }

    int row = ui->fontpathListBox->currentRow();
    QString fam = fontpaths.at(row);
    fontpaths.removeAt(row);
    fontpaths.insert(row + 1, fam);
    ui->fontpathListBox->clear();
    ui->fontpathListBox->insertItems(0, fontpaths);
    ui->fontpathListBox->setCurrentRow(row + 1);
    setModified(true);
}

void MainWindow::upFontpath()
{
    if (!ui->fontpathListBox->currentItem() || ui->fontpathListBox->currentRow() < 1)
        return;

    int row = ui->fontpathListBox->currentRow();
    QString fam = fontpaths.at(row);
    fontpaths.removeAt(row);
    fontpaths.insert(row - 1, fam);
    ui->fontpathListBox->clear();
    ui->fontpathListBox->insertItems(0, fontpaths);
    ui->fontpathListBox->setCurrentRow(row - 1);
    setModified(true);
}

void MainWindow::browseFontpath()
{
    QString dirname = QFileDialog::getExistingDirectory(this, tr("Select a Directory"));
    if (dirname.isNull())
        return;

   ui->fontPathLineEdit->setText(dirname);
}

void MainWindow::somethingModified()
{
    setModified(true);
}

void MainWindow::helpAbout()
{
    QMessageBox box(this);
    box.setText(tr("<h3>%1</h3>"
                   "<br/>Version %2"
                   "<br/><br/>Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).")
                   .arg(tr("Qt Configuration")).arg(QLatin1String(QT_VERSION_STR)));
    box.setWindowTitle(tr("Qt Configuration"));
    box.setIcon(QMessageBox::NoIcon);
    box.exec();
}

void MainWindow::helpAboutQt()
{
    QMessageBox::aboutQt(this, tr("Qt Configuration"));
}

void MainWindow::pageChanged(int pageNumber)
{
    QWidget *page = ui->mainTabWidget->widget(pageNumber);
    if (page == ui->interfaceTab)
        ui->helpView->setText(tr(interface_text));
    else if (page == ui->appearanceTab)
        ui->helpView->setText(tr(appearance_text));
    else if (page == ui->fontsTab)
        ui->helpView->setText(tr(font_text));
    else if (page == ui->printerTab)
        ui->helpView->setText(tr(printer_text));
}

void MainWindow::closeEvent(QCloseEvent *e)
{
    if (modified) {
        switch (QMessageBox::warning(this, tr("Save Changes"),
                                     tr("Save changes to settings?"),
                                     (QMessageBox::Yes | QMessageBox::No
                                     | QMessageBox::Cancel))) {
        case QMessageBox::Yes: // save
            qApp->processEvents();
            fileSave();

            // fall through intended
        case QMessageBox::No: // don't save
            e->accept();
            break;

        case QMessageBox::Cancel: // cancel
            e->ignore();
            break;

        default: break;
        }
    } else
        e->accept();
}

QT_END_NAMESPACE

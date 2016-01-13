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

#include "stylesheeteditor_p.h"
#include "csshighlighter_p.h"
#include "iconselector_p.h"
#include "qtgradientmanager.h"
#include "qtgradientviewdialog.h"
#include "qtgradientutils.h"
#include "qdesigner_utils_p.h"

#include <QtDesigner/QDesignerFormWindowInterface>
#include <QtDesigner/QDesignerFormWindowCursorInterface>
#include <QtDesigner/QDesignerFormEditorInterface>
#include <QtDesigner/QDesignerPropertySheetExtension>
#include <QtDesigner/QDesignerIntegrationInterface>
#include <QtDesigner/QDesignerSettingsInterface>
#include <QtDesigner/QExtensionManager>

#include <QtCore/QSignalMapper>
#include <QtWidgets/QAction>
#include <QtWidgets/QColorDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFontDialog>
#include <QtWidgets/QMenu>
#include <QtWidgets/QPushButton>
#include <QtGui/QTextDocument>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QVBoxLayout>
#include <private/qcssparser_p.h>

QT_BEGIN_NAMESPACE

static const char *styleSheetProperty = "styleSheet";
static const char *StyleSheetDialogC = "StyleSheetDialog";
static const char *Geometry = "Geometry";

namespace qdesigner_internal {

StyleSheetEditor::StyleSheetEditor(QWidget *parent)
    : QTextEdit(parent)
{
    setTabStopWidth(fontMetrics().width(QLatin1Char(' '))*4);
    setAcceptRichText(false);
    new CssHighlighter(document());
}

// --- StyleSheetEditorDialog
StyleSheetEditorDialog::StyleSheetEditorDialog(QDesignerFormEditorInterface *core, QWidget *parent, Mode mode):
    QDialog(parent),
    m_buttonBox(new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel|QDialogButtonBox::Help)),
    m_editor(new StyleSheetEditor),
    m_validityLabel(new QLabel(tr("Valid Style Sheet"))),
    m_core(core),
    m_addResourceAction(new QAction(tr("Add Resource..."), this)),
    m_addGradientAction(new QAction(tr("Add Gradient..."), this)),
    m_addColorAction(new QAction(tr("Add Color..."), this)),
    m_addFontAction(new QAction(tr("Add Font..."), this))
{
    setWindowTitle(tr("Edit Style Sheet"));
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    connect(m_buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(m_buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
    connect(m_buttonBox, SIGNAL(helpRequested()), this, SLOT(slotRequestHelp()));
    m_buttonBox->button(QDialogButtonBox::Help)->setShortcut(QKeySequence::HelpContents);

    connect(m_editor, SIGNAL(textChanged()), this, SLOT(validateStyleSheet()));

    QToolBar *toolBar = new QToolBar;

    QGridLayout *layout = new QGridLayout;
    layout->addWidget(toolBar, 0, 0, 1, 2);
    layout->addWidget(m_editor, 1, 0, 1, 2);
    layout->addWidget(m_validityLabel, 2, 0, 1, 1);
    layout->addWidget(m_buttonBox, 2, 1, 1, 1);
    setLayout(layout);

    m_editor->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_editor, SIGNAL(customContextMenuRequested(QPoint)),
                this, SLOT(slotContextMenuRequested(QPoint)));

    QSignalMapper *resourceActionMapper = new QSignalMapper(this);
    QSignalMapper *gradientActionMapper = new QSignalMapper(this);
    QSignalMapper *colorActionMapper = new QSignalMapper(this);

    resourceActionMapper->setMapping(m_addResourceAction, QString());
    gradientActionMapper->setMapping(m_addGradientAction, QString());
    colorActionMapper->setMapping(m_addColorAction, QString());

    connect(m_addResourceAction, SIGNAL(triggered()), resourceActionMapper, SLOT(map()));
    connect(m_addGradientAction, SIGNAL(triggered()), gradientActionMapper, SLOT(map()));
    connect(m_addColorAction, SIGNAL(triggered()), colorActionMapper, SLOT(map()));
    connect(m_addFontAction, SIGNAL(triggered()), this, SLOT(slotAddFont()));

    m_addResourceAction->setEnabled(mode == ModePerForm);

    const char * const resourceProperties[] = {
        "background-image",
        "border-image",
        "image",
        0
    };

    const char * const colorProperties[] = {
        "color",
        "background-color",
        "alternate-background-color",
        "border-color",
        "border-top-color",
        "border-right-color",
        "border-bottom-color",
        "border-left-color",
        "gridline-color",
        "selection-color",
        "selection-background-color",
        0
    };

    QMenu *resourceActionMenu = new QMenu(this);
    QMenu *gradientActionMenu = new QMenu(this);
    QMenu *colorActionMenu = new QMenu(this);

    for (int resourceProperty = 0; resourceProperties[resourceProperty]; ++resourceProperty) {
        QAction *action = resourceActionMenu->addAction(QLatin1String(resourceProperties[resourceProperty]));
        connect(action, SIGNAL(triggered()), resourceActionMapper, SLOT(map()));
        resourceActionMapper->setMapping(action, QLatin1String(resourceProperties[resourceProperty]));
    }

    for (int colorProperty = 0; colorProperties[colorProperty]; ++colorProperty) {
        QAction *gradientAction = gradientActionMenu->addAction(QLatin1String(colorProperties[colorProperty]));
        QAction *colorAction = colorActionMenu->addAction(QLatin1String(colorProperties[colorProperty]));
        connect(gradientAction, SIGNAL(triggered()), gradientActionMapper, SLOT(map()));
        connect(colorAction, SIGNAL(triggered()), colorActionMapper, SLOT(map()));
        gradientActionMapper->setMapping(gradientAction, QLatin1String(colorProperties[colorProperty]));
        colorActionMapper->setMapping(colorAction, QLatin1String(colorProperties[colorProperty]));
    }

    connect(resourceActionMapper, SIGNAL(mapped(QString)), this, SLOT(slotAddResource(QString)));
    connect(gradientActionMapper, SIGNAL(mapped(QString)), this, SLOT(slotAddGradient(QString)));
    connect(colorActionMapper, SIGNAL(mapped(QString)), this, SLOT(slotAddColor(QString)));

    m_addResourceAction->setMenu(resourceActionMenu);
    m_addGradientAction->setMenu(gradientActionMenu);
    m_addColorAction->setMenu(colorActionMenu);

    toolBar->addAction(m_addResourceAction);
    toolBar->addAction(m_addGradientAction);
    toolBar->addAction(m_addColorAction);
    toolBar->addAction(m_addFontAction);

    m_editor->setFocus();

    QDesignerSettingsInterface *settings = core->settingsManager();
    settings->beginGroup(QLatin1String(StyleSheetDialogC));

    if (settings->contains(QLatin1String(Geometry)))
        restoreGeometry(settings->value(QLatin1String(Geometry)).toByteArray());

    settings->endGroup();
}

StyleSheetEditorDialog::~StyleSheetEditorDialog()
{
    QDesignerSettingsInterface *settings = m_core->settingsManager();
    settings->beginGroup(QLatin1String(StyleSheetDialogC));

    settings->setValue(QLatin1String(Geometry), saveGeometry());
    settings->endGroup();
}

void StyleSheetEditorDialog::setOkButtonEnabled(bool v)
{
    m_buttonBox->button(QDialogButtonBox::Ok)->setEnabled(v);
    if (QPushButton *applyButton = m_buttonBox->button(QDialogButtonBox::Apply))
        applyButton->setEnabled(v);
}

void StyleSheetEditorDialog::slotContextMenuRequested(const QPoint &pos)
{
    QMenu *menu = m_editor->createStandardContextMenu();
    menu->addSeparator();
    menu->addAction(m_addResourceAction);
    menu->addAction(m_addGradientAction);
    menu->exec(mapToGlobal(pos));
    delete menu;
}

void StyleSheetEditorDialog::slotAddResource(const QString &property)
{
    const QString path = IconSelector::choosePixmapResource(m_core, m_core->resourceModel(), QString(), this);
    if (!path.isEmpty())
        insertCssProperty(property, QString(QStringLiteral("url(%1)")).arg(path));
}

void StyleSheetEditorDialog::slotAddGradient(const QString &property)
{
    bool ok;
    const QGradient grad = QtGradientViewDialog::getGradient(&ok, m_core->gradientManager(), this);
    if (ok)
        insertCssProperty(property, QtGradientUtils::styleSheetCode(grad));
}

void StyleSheetEditorDialog::slotAddColor(const QString &property)
{
    const QColor color = QColorDialog::getColor(0xffffffff, this, QString(), QColorDialog::ShowAlphaChannel);
    if (!color.isValid())
        return;

    QString colorStr;

    if (color.alpha() == 255) {
        colorStr = QString(QStringLiteral("rgb(%1, %2, %3)")).arg(
                color.red()).arg(color.green()).arg(color.blue());
    } else {
        colorStr = QString(QStringLiteral("rgba(%1, %2, %3, %4)")).arg(
                color.red()).arg(color.green()).arg(color.blue()).arg(color.alpha());
    }

    insertCssProperty(property, colorStr);
}

void StyleSheetEditorDialog::slotAddFont()
{
    bool ok;
    QFont font = QFontDialog::getFont(&ok, this);
    if (ok) {
        QString fontStr;
        if (font.weight() != QFont::Normal) {
            fontStr += QString::number(font.weight());
            fontStr += QLatin1Char(' ');
        }

        switch (font.style()) {
        case QFont::StyleItalic:
            fontStr += QStringLiteral("italic ");
            break;
        case QFont::StyleOblique:
            fontStr += QStringLiteral("oblique ");
            break;
        default:
            break;
        }
        fontStr += QString::number(font.pointSize());
        fontStr += QStringLiteral("pt \"");
        fontStr += font.family();
        fontStr += QLatin1Char('"');

        insertCssProperty(QStringLiteral("font"), fontStr);
        QString decoration;
        if (font.underline())
            decoration += QStringLiteral("underline");
        if (font.strikeOut()) {
            if (!decoration.isEmpty())
                decoration += QLatin1Char(' ');
            decoration += QStringLiteral("line-through");
        }
        insertCssProperty(QStringLiteral("text-decoration"), decoration);
    }
}

void StyleSheetEditorDialog::insertCssProperty(const QString &name, const QString &value)
{
    if (!value.isEmpty()) {
        QTextCursor cursor = m_editor->textCursor();
        if (!name.isEmpty()) {
            cursor.beginEditBlock();
            cursor.removeSelectedText();
            cursor.movePosition(QTextCursor::EndOfLine);

            // Simple check to see if we're in a selector scope
            const QTextDocument *doc = m_editor->document();
            const QTextCursor closing = doc->find(QStringLiteral("}"), cursor, QTextDocument::FindBackward);
            const QTextCursor opening = doc->find(QStringLiteral("{"), cursor, QTextDocument::FindBackward);
            const bool inSelector = !opening.isNull() && (closing.isNull() ||
                                                          closing.position() < opening.position());
            QString insertion;
            if (m_editor->textCursor().block().length() != 1)
                insertion += QLatin1Char('\n');
            if (inSelector)
                insertion += QLatin1Char('\t');
            insertion += name;
            insertion += QStringLiteral(": ");
            insertion += value;
            insertion += QLatin1Char(';');
            cursor.insertText(insertion);
            cursor.endEditBlock();
        } else {
            cursor.insertText(value);
        }
    }
}

void StyleSheetEditorDialog::slotRequestHelp()
{
    m_core->integration()->emitHelpRequested(QStringLiteral("qtwidgets"),
                                             QStringLiteral("stylesheet-reference.html"));
}

QDialogButtonBox * StyleSheetEditorDialog::buttonBox() const
{
   return m_buttonBox;
}

QString StyleSheetEditorDialog::text() const
{
    return m_editor->toPlainText();
}

void StyleSheetEditorDialog::setText(const QString &t)
{
    m_editor->setText(t);
}

bool StyleSheetEditorDialog::isStyleSheetValid(const QString &styleSheet)
{
    QCss::Parser parser(styleSheet);
    QCss::StyleSheet sheet;
    if (parser.parse(&sheet))
        return true;
    QString fullSheet = QStringLiteral("* { ");
    fullSheet += styleSheet;
    fullSheet += QLatin1Char('}');
    QCss::Parser parser2(fullSheet);
    return parser2.parse(&sheet);
}

void StyleSheetEditorDialog::validateStyleSheet()
{
    const bool valid = isStyleSheetValid(m_editor->toPlainText());
    setOkButtonEnabled(valid);
    if (valid) {
        m_validityLabel->setText(tr("Valid Style Sheet"));
        m_validityLabel->setStyleSheet(QStringLiteral("color: green"));
    } else {
        m_validityLabel->setText(tr("Invalid Style Sheet"));
        m_validityLabel->setStyleSheet(QStringLiteral("color: red"));
    }
}

// --- StyleSheetPropertyEditorDialog
StyleSheetPropertyEditorDialog::StyleSheetPropertyEditorDialog(QWidget *parent,
                                               QDesignerFormWindowInterface *fw,
                                               QWidget *widget):
    StyleSheetEditorDialog(fw->core(), parent),
    m_fw(fw),
    m_widget(widget)
{
    Q_ASSERT(m_fw != 0);

    QPushButton *apply = buttonBox()->addButton(QDialogButtonBox::Apply);
    QObject::connect(apply, SIGNAL(clicked()), this, SLOT(applyStyleSheet()));
    QObject::connect(buttonBox(), SIGNAL(accepted()), this, SLOT(applyStyleSheet()));

    QDesignerPropertySheetExtension *sheet =
            qt_extension<QDesignerPropertySheetExtension*>(m_fw->core()->extensionManager(), m_widget);
    Q_ASSERT(sheet != 0);
    const int index = sheet->indexOf(QLatin1String(styleSheetProperty));
    const PropertySheetStringValue value = qvariant_cast<PropertySheetStringValue>(sheet->property(index));
    setText(value.value());
}

void StyleSheetPropertyEditorDialog::applyStyleSheet()
{
    const PropertySheetStringValue value(text(), false);
    m_fw->cursor()->setWidgetProperty(m_widget, QLatin1String(styleSheetProperty), QVariant::fromValue(value));
}

} // namespace qdesigner_internal

QT_END_NAMESPACE

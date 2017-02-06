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

#include "widgetfactory_p.h"
#include "widgetdatabase_p.h"
#include "metadatabase_p.h"
#include "qlayout_widget_p.h"
#include "qdesigner_widget_p.h"
#include "qdesigner_tabwidget_p.h"
#include "qdesigner_toolbox_p.h"
#include "qdesigner_stackedbox_p.h"
#include "qdesigner_toolbar_p.h"
#include "qdesigner_menubar_p.h"
#include "qdesigner_menu_p.h"
#include "qdesigner_dockwidget_p.h"
#include "qdesigner_utils_p.h"
#include "formwindowbase_p.h"

// shared
#include "layoutinfo_p.h"
#include "spacer_widget_p.h"
#include "layout_p.h"
#include "abstractintrospection_p.h"

// sdk
#include <QtDesigner/QDesignerFormEditorInterface>
#include <QtDesigner/QDesignerContainerExtension>
#include <QtDesigner/QExtensionManager>
#include <QtDesigner/QDesignerPropertySheetExtension>
#include <QtDesigner/QDesignerLanguageExtension>
#include <QtDesigner/QDesignerFormWindowManagerInterface>
#include <QtDesigner/QDesignerFormWindowCursorInterface>

#include <QtUiPlugin/QDesignerCustomWidgetInterface>

#include <QtWidgets/QtWidgets>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QFontComboBox>
#include <QtWidgets/QAbstractSpinBox>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QStyle>
#include <QtWidgets/QStyleFactory>
#include <QtWidgets/QWizard>
#include <QtCore/qdebug.h>
#include <QtCore/QMetaObject>

QT_BEGIN_NAMESPACE

#ifdef Q_OS_WIN
static inline bool isAxWidget(const QObject *o)
{
    // Is it one of  QDesignerAxWidget/QDesignerAxPluginWidget?
    static const char *axWidgetName = "QDesignerAx";
    static const unsigned axWidgetNameLen = qstrlen(axWidgetName);
    return qstrncmp(o->metaObject()->className(), axWidgetName, axWidgetNameLen) == 0;
}
#endif

/* Dynamic boolean property indicating object was created by the factory
 * for the form editor. */

static const char *formEditorDynamicProperty = "_q_formEditorObject";

namespace qdesigner_internal {

// A friendly SpinBox that grants access to its QLineEdit
class FriendlySpinBox : public QAbstractSpinBox {
public:
    friend class WidgetFactory;
};

// An event filter for form-combo boxes that prevents the embedded line edit
// from getting edit focus (and drawing blue artifacts/lines). It catches the
// ChildPolished event when the "editable" property flips to true and the
// QLineEdit is created and turns off the LineEdit's focus policy.

class ComboEventFilter : public QObject {
public:
    explicit ComboEventFilter(QComboBox *parent) : QObject(parent) {}
    bool eventFilter(QObject *watched, QEvent *event) Q_DECL_OVERRIDE;
};

bool ComboEventFilter::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::ChildPolished) {
        QComboBox *cb = static_cast<QComboBox*>(watched);
        if (QLineEdit *le = cb->lineEdit()) {
            le->setFocusPolicy(Qt::NoFocus);
            le->setCursor(Qt::ArrowCursor);
        }
    }
    return QObject::eventFilter(watched, event);
}

/* Watch out for QWizards changing their pages and make sure that not some
 * selected widget becomes invisible on a hidden page (causing the selection
 * handles to shine through). Select the wizard in that case  in analogy to
 * the QTabWidget event filters, etc. */

class WizardPageChangeWatcher : public QObject {
    Q_OBJECT
public:
    explicit WizardPageChangeWatcher(QWizard *parent);

public slots:
    void pageChanged();
};

WizardPageChangeWatcher::WizardPageChangeWatcher(QWizard *parent) :
    QObject(parent)
{
    connect(parent, &QWizard::currentIdChanged, this, &WizardPageChangeWatcher::pageChanged);
}

void WizardPageChangeWatcher::pageChanged()
{
    /* Use a bit more conservative approach than that for the QTabWidget,
     * change the selection only if a selected child becomes invisible by
     * changing the page. */
    QWizard *wizard = static_cast<QWizard *>(parent());
    QDesignerFormWindowInterface *fw = QDesignerFormWindowInterface::findFormWindow(wizard);
    if (!fw)
        return;
    QDesignerFormWindowCursorInterface *cursor = fw->cursor();
    const int selCount = cursor->selectedWidgetCount();
    for (int i = 0; i <  selCount; i++) {
        if (!cursor->selectedWidget(i)->isVisible()) {
            fw->clearSelection(false);
            fw->selectWidget(wizard, true);
            break;
        }
    }
}

// ---------------- WidgetFactory::Strings
WidgetFactory::Strings::Strings() :
    m_alignment(QStringLiteral("alignment")),
    m_bottomMargin(QStringLiteral("bottomMargin")),
    m_geometry(QStringLiteral("geometry")),
    m_leftMargin(QStringLiteral("leftMargin")),
    m_line(QStringLiteral("Line")),
    m_objectName(QStringLiteral("objectName")),
    m_spacerName(QStringLiteral("spacerName")),
    m_orientation(QStringLiteral("orientation")),
    m_qAction(QStringLiteral("QAction")),
    m_qButtonGroup(QStringLiteral("QButtonGroup")),
    m_qAxWidget(QStringLiteral("QAxWidget")),
    m_qDialog(QStringLiteral("QDialog")),
    m_qDockWidget(QStringLiteral("QDockWidget")),
    m_qLayoutWidget(QStringLiteral("QLayoutWidget")),
    m_qMenu(QStringLiteral("QMenu")),
    m_qMenuBar(QStringLiteral("QMenuBar")),
    m_qWidget(QStringLiteral("QWidget")),
    m_rightMargin(QStringLiteral("rightMargin")),
    m_sizeHint(QStringLiteral("sizeHint")),
    m_spacer(QStringLiteral("Spacer")),
    m_text(QStringLiteral("text")),
    m_title(QStringLiteral("title")),
    m_topMargin(QStringLiteral("topMargin")),
    m_windowIcon(QStringLiteral("windowIcon")),
    m_windowTitle(QStringLiteral("windowTitle"))
{
}
// ---------------- WidgetFactory
QPointer<QWidget> *WidgetFactory::m_lastPassiveInteractor = new QPointer<QWidget>();
bool WidgetFactory::m_lastWasAPassiveInteractor = false;
const char *WidgetFactory::disableStyleCustomPaintingPropertyC = "_q_custom_style_disabled";

WidgetFactory::WidgetFactory(QDesignerFormEditorInterface *core, QObject *parent)
    : QDesignerWidgetFactoryInterface(parent),
      m_core(core),
      m_formWindow(0),
      m_currentStyle(0)
{
}

WidgetFactory::~WidgetFactory()
{
}

QDesignerFormWindowInterface *WidgetFactory::currentFormWindow(QDesignerFormWindowInterface *fw)
{
    QDesignerFormWindowInterface *was = m_formWindow;
    m_formWindow = fw;
    return was;
}

void WidgetFactory::loadPlugins()
{
    m_customFactory.clear();

    QDesignerPluginManager *pluginManager = m_core->pluginManager();

    QList<QDesignerCustomWidgetInterface*> lst = pluginManager->registeredCustomWidgets();
    foreach (QDesignerCustomWidgetInterface *c, lst) {
        m_customFactory.insert(c->name(), c);
    }
}

// Convencience to create non-widget objects. Returns 0 if unknown
QObject* WidgetFactory::createObject(const QString &className, QObject* parent) const
{
    if (className.isEmpty()) {
        qWarning("** WARNING %s called with an empty class name", Q_FUNC_INFO);
        return 0;
    }
    if (className == m_strings.m_qAction)
        return new QAction(parent);
    if (className == m_strings.m_qButtonGroup)
        return new QButtonGroup(parent);
    return 0;
}

// Check for mismatched class names in plugins, which is hard to track.
static bool classNameMatches(const QObject *created, const QString &className)
{
#ifdef Q_OS_WIN
    // Perform literal comparison first for QAxWidget, for which a meta object hack is in effect.
    if (isAxWidget(created))
       return true;
#endif
    const char *createdClassNameC = created->metaObject()->className();
    const QByteArray classNameB = className.toUtf8();
    const char *classNameC = classNameB.constData();
    if (qstrcmp(createdClassNameC, classNameC) == 0 || created->inherits(classNameC))
        return true;
    // QTBUG-53984: QWebEngineView property dummy
    if (classNameB == "QWebEngineView" && qstrcmp(createdClassNameC, "fake::QWebEngineView") == 0)
        return true;
    return false;
}

QWidget*  WidgetFactory::createCustomWidget(const QString &className, QWidget *parentWidget, bool *creationError) const
{
    *creationError = false;
    CustomWidgetFactoryMap::const_iterator it = m_customFactory.constFind(className);
    if (it == m_customFactory.constEnd())
        return 0;

    QDesignerCustomWidgetInterface *factory = it.value();
    QWidget *rc = factory->createWidget(parentWidget);
    // shouldn't happen
    if (!rc) {
        *creationError = true;
        designerWarning(tr("The custom widget factory registered for widgets of class %1 returned 0.").arg(className));
        return 0;
    }
    // Figure out the base class unless it is known
    static QSet<QString> knownCustomClasses;
    if (!knownCustomClasses.contains(className)) {
        QDesignerWidgetDataBaseInterface *wdb = m_core->widgetDataBase();
        const int widgetInfoIndex = wdb->indexOfObject(rc, false);
        if (widgetInfoIndex != -1) {
            if (wdb->item(widgetInfoIndex)->extends().isEmpty()) {
                const QDesignerMetaObjectInterface *mo = core()->introspection()->metaObject(rc)->superClass();
                // If we hit on a 'Q3DesignerXXWidget' that claims to be a 'Q3XXWidget', step
                // over.
                if (mo && mo->className() == className)
                    mo = mo->superClass();
                while (mo != 0) {
                    if (core()->widgetDataBase()->indexOfClassName(mo->className()) != -1) {
                        wdb->item(widgetInfoIndex)->setExtends(mo->className());
                        break;
                    }
                    mo = mo->superClass();
                }
            }
            knownCustomClasses.insert(className);
        }
    }
    // Since a language plugin may lie about its names, like Qt Jambi
    // does, return immediately here...
    QDesignerLanguageExtension *lang =
        qt_extension<QDesignerLanguageExtension *>(m_core->extensionManager(), m_core);
    if (lang)
        return rc;

    // Check for mismatched class names which is hard to track.
    if (!classNameMatches(rc, className)) {
        designerWarning(tr("A class name mismatch occurred when creating a widget using the custom widget factory registered for widgets of class %1."
                           " It returned a widget of class %2.")
                        .arg(className, QString::fromUtf8(rc->metaObject()->className())));
    }
    return rc;
}


QWidget *WidgetFactory::createWidget(const QString &widgetName, QWidget *parentWidget) const
{
    if (widgetName.isEmpty()) {
        qWarning("** WARNING %s called with an empty class name", Q_FUNC_INFO);
        return 0;
    }
    // Preview or for form window?
    QDesignerFormWindowInterface *fw = m_formWindow;
    if (! fw)
        fw = QDesignerFormWindowInterface::findFormWindow(parentWidget);

    QWidget *w = 0;
    do {
        // 1) custom. If there is an explicit failure(factory wants to indicate something is wrong),
        //    return 0, do not try to find fallback, which might be worse in the case of Q3 widget.
        bool customWidgetCreationError;
        w = createCustomWidget(widgetName, parentWidget, &customWidgetCreationError);
        if (w) {
            break;
        } else {
            if (customWidgetCreationError)
                return 0;
        }

        // 2) Special widgets
        if (widgetName == m_strings.m_line) {
            w = new Line(parentWidget);
        } else if (widgetName == m_strings.m_qDockWidget) {
            w = new QDesignerDockWidget(parentWidget);
        } else if (widgetName == m_strings.m_qMenuBar) {
            w = new QDesignerMenuBar(parentWidget);
        } else if (widgetName == m_strings.m_qMenu) {
            w = new QDesignerMenu(parentWidget);
        } else if (widgetName == m_strings.m_spacer) {
            w = new Spacer(parentWidget);
        } else if (widgetName == m_strings.m_qLayoutWidget) {
            w = fw ? new QLayoutWidget(fw, parentWidget) : new QWidget(parentWidget);
        } else if (widgetName == m_strings.m_qDialog) {
            if (fw) {
                w = new QDesignerDialog(fw, parentWidget);
            } else {
                w = new QDialog(parentWidget);
            }
        } else if (widgetName == m_strings.m_qWidget) {
            /* We want a 'QDesignerWidget' that draws a grid only for widget
             * forms and container extension pages (not for preview and not
             * for normal QWidget children on forms (legacy) */
            if (fw && parentWidget) {
                if (qt_extension<QDesignerContainerExtension*>(m_core->extensionManager(), parentWidget)) {
                    w = new QDesignerWidget(fw, parentWidget);
                } else {
                    if (parentWidget == fw->formContainer())
                        w = new QDesignerWidget(fw, parentWidget);
                }
            }
            if (!w)
                w = new QWidget(parentWidget);
        }
        if (w)
            break;

        // 3) table
        const QByteArray widgetNameBA = widgetName.toUtf8();
        const char *widgetNameC = widgetNameBA.constData();

        if (w) { // symmetry for macro
        }

#define DECLARE_LAYOUT(L, C)
#define DECLARE_COMPAT_WIDGET(W, C) /*DECLARE_WIDGET(W, C)*/
#define DECLARE_WIDGET(W, C) else if (!qstrcmp(widgetNameC, #W)) { Q_ASSERT(w == 0); w = new W(parentWidget); }
#define DECLARE_WIDGET_1(W, C) else if (!qstrcmp(widgetNameC, #W)) { Q_ASSERT(w == 0); w = new W(0, parentWidget); }

#include <widgets.table>

#undef DECLARE_COMPAT_WIDGET
#undef DECLARE_LAYOUT
#undef DECLARE_WIDGET
#undef DECLARE_WIDGET_1

        if (w)
            break;
        // 4) fallBack
        const QString fallBackBaseClass = m_strings.m_qWidget;
        QDesignerWidgetDataBaseInterface *db = core()->widgetDataBase();
        QDesignerWidgetDataBaseItemInterface *item = db->item(db->indexOfClassName(widgetName));
        if (item == 0) {
            // Emergency: Create, derived from QWidget
            QString includeFile = widgetName.toLower();
            includeFile +=  QStringLiteral(".h");
            item = appendDerived(db,widgetName, tr("%1 Widget").arg(widgetName),fallBackBaseClass,
                                 includeFile, true, true);
            Q_ASSERT(item);
        }
        QString baseClass = item->extends();
        if (baseClass.isEmpty()) {
            // Currently happens in the case of Q3-Support widgets
            baseClass =fallBackBaseClass;
        }
        if (QWidget *promotedWidget = createWidget(baseClass, parentWidget)) {
            promoteWidget(core(), promotedWidget, widgetName);
            return promotedWidget;  // Do not initialize twice.
        }
    } while (false);

    Q_ASSERT(w != 0);
    if (m_currentStyle)
        w->setStyle(m_currentStyle);
     initializeCommon(w);
    if (fw) { // form editor  initialization
        initialize(w);
    } else { // preview-only initialization
        initializePreview(w);
    }
    return w;
}

QString WidgetFactory::classNameOf(QDesignerFormEditorInterface *c, const QObject* o)
{
    if (o == 0)
        return QString();

    const char *className = o->metaObject()->className();
    if (!o->isWidgetType())
        return QLatin1String(className);
    const QWidget *w = static_cast<const QWidget*>(o);
    // check promoted before designer special
    const QString customClassName = promotedCustomClassName(c, const_cast<QWidget*>(w));
    if (!customClassName.isEmpty())
        return customClassName;
    if (qobject_cast<const QDesignerMenuBar*>(w))
        return QStringLiteral("QMenuBar");
    else if (qobject_cast<const QDesignerMenu*>(w))
        return QStringLiteral("QMenu");
     else if (qobject_cast<const QDesignerDockWidget*>(w))
        return QStringLiteral("QDockWidget");
    else if (qobject_cast<const QDesignerDialog*>(w))
        return QStringLiteral("QDialog");
    else if (qobject_cast<const QDesignerWidget*>(w))
        return QStringLiteral("QWidget");
#ifdef Q_OS_WIN
    else if (isAxWidget(w))
        return QStringLiteral("QAxWidget");
#endif
    return QLatin1String(className);
}

QLayout *WidgetFactory::createUnmanagedLayout(QWidget *parentWidget, int type)
{
    switch (type) {
    case LayoutInfo::HBox:
        return new QHBoxLayout(parentWidget);
    case LayoutInfo::VBox:
        return new QVBoxLayout(parentWidget);
    case LayoutInfo::Grid:
        return new QGridLayout(parentWidget);
    case LayoutInfo::Form:
        return new QFormLayout(parentWidget);
    default:
        Q_ASSERT(0);
        break;
    }
    return 0;
}


/*!  Creates a layout on the widget \a widget of the type \a type
  which can be \c HBox, \c VBox or \c Grid.
*/

QLayout *WidgetFactory::createLayout(QWidget *widget, QLayout *parentLayout, int type) const // ### (sizepolicy)
{
    QDesignerMetaDataBaseInterface *metaDataBase = core()->metaDataBase();

    if (parentLayout == 0) {
        QWidget *page = containerOfWidget(widget);
        if (page) {
            widget = page;
        } else {
            const QString msg = tr("The current page of the container '%1' (%2) could not be determined while creating a layout."
"This indicates an inconsistency in the ui-file, probably a layout being constructed on a container widget.").arg(widget->objectName()).arg(classNameOf(core(), widget));
            designerWarning(msg);
        }
    }

    Q_ASSERT(metaDataBase->item(widget) != 0); // ensure the widget is managed

    if (parentLayout == 0 && metaDataBase->item(widget->layout()) == 0) {
        parentLayout = widget->layout();
    }

    QWidget *parentWidget = parentLayout != 0 ? 0 : widget;

    QLayout *layout = createUnmanagedLayout(parentWidget, type);
    metaDataBase->add(layout); // add the layout in the MetaDataBase

    QDesignerPropertySheetExtension *sheet = qt_extension<QDesignerPropertySheetExtension*>(core()->extensionManager(), layout);

    if (sheet) {
        sheet->setChanged(sheet->indexOf(m_strings.m_objectName), true);
        if (widget->inherits("QLayoutWidget")) {
            sheet->setProperty(sheet->indexOf(m_strings.m_leftMargin), 0);
            sheet->setProperty(sheet->indexOf(m_strings.m_topMargin), 0);
            sheet->setProperty(sheet->indexOf(m_strings.m_rightMargin), 0);
            sheet->setProperty(sheet->indexOf(m_strings.m_bottomMargin), 0);
        }

        const int index = sheet->indexOf(m_strings.m_alignment);
        if (index != -1)
            sheet->setChanged(index, true);
    }

    if (metaDataBase->item(widget->layout()) == 0) {
        Q_ASSERT(layout->parent() == 0);
        QBoxLayout *box = qobject_cast<QBoxLayout*>(widget->layout());
        if (!box) {  // we support only unmanaged box layouts
            const QString msg = tr("Attempt to add a layout to a widget '%1' (%2) which already has an unmanaged layout of type %3.\n"
                                            "This indicates an inconsistency in the ui-file.").
                                 arg(widget->objectName()).arg(classNameOf(core(), widget)).arg(classNameOf(core(), widget->layout()));
            designerWarning(msg);
            return 0;
        }
        box->addLayout(layout);
    }

    return layout;
}

/*!  Returns the widget into which children should be inserted when \a
  w is a container known to designer.

  Usually, it is \a w itself, but there are exceptions (for example, a
  tabwidget is known to designer as a container, but the child
  widgets should be inserted into the current page of the
  tabwidget. In this case, the current page of
  the tabwidget  would be returned.)
 */
QWidget* WidgetFactory::containerOfWidget(QWidget *w) const
{
    if (QDesignerContainerExtension *container = qt_extension<QDesignerContainerExtension*>(core()->extensionManager(), w))
        return container->widget(container->currentIndex());

    return w;
}

/*!  Returns the actual designer widget of the container \a w. This is
  normally \a w itself, but it might be a parent or grand parent of \a w
  (for example, when working with a tabwidget and \a w is the container which
  contains and layouts children, but the actual widget known to
  designer is the tabwidget which is the parent of \a w. In this case,
  the tabwidget would be returned.)
*/

QWidget* WidgetFactory::widgetOfContainer(QWidget *w) const
{
    // ### cleanup
    if (!w)
        return 0;
    if (w->parentWidget() && w->parentWidget()->parentWidget() &&
         w->parentWidget()->parentWidget()->parentWidget() &&
         qobject_cast<QToolBox*>(w->parentWidget()->parentWidget()->parentWidget()))
        return w->parentWidget()->parentWidget()->parentWidget();

    while (w != 0) {
        if (core()->widgetDataBase()->isContainer(w) ||
             (w && qobject_cast<QDesignerFormWindowInterface*>(w->parentWidget())))
            return w;

        w = w->parentWidget();
    }

    return w;
}

QDesignerFormEditorInterface *WidgetFactory::core() const
{
    return m_core;
}

// Necessary initializations for form editor/preview objects
void WidgetFactory::initializeCommon(QWidget *widget) const
{
    // Apply style
    if (m_currentStyle)
        widget->setStyle(m_currentStyle);
}

// Necessary initializations for preview objects
void WidgetFactory::initializePreview(QWidget *widget) const
{

    if (QStackedWidget *stackedWidget = qobject_cast<QStackedWidget*>(widget)) {
        QStackedWidgetPreviewEventFilter::install(stackedWidget); // Add browse button only.
        return;
    }
}

// Necessary initializations for form editor objects
void WidgetFactory::initialize(QObject *object) const
{
    // Indicate that this is a form object (for QDesignerFormWindowInterface::findFormWindow)
    object->setProperty(formEditorDynamicProperty, QVariant(true));
    QDesignerPropertySheetExtension *sheet = qt_extension<QDesignerPropertySheetExtension*>(m_core->extensionManager(), object);
    if (!sheet)
        return;

    sheet->setChanged(sheet->indexOf(m_strings.m_objectName), true);

    if (!object->isWidgetType()) {
        if (qobject_cast<QAction*>(object))
            sheet->setChanged(sheet->indexOf(m_strings.m_text), true);
        return;
    }

    QWidget *widget = static_cast<QWidget*>(object);
    const bool isMenu = qobject_cast<QMenu*>(widget);
    const bool isMenuBar = !isMenu && qobject_cast<QMenuBar*>(widget);

    widget->setAttribute(Qt::WA_TransparentForMouseEvents, false);
    widget->setFocusPolicy((isMenu || isMenuBar) ? Qt::StrongFocus : Qt::NoFocus);

    if (!isMenu)
        sheet->setChanged(sheet->indexOf(m_strings.m_geometry), true);

    if (qobject_cast<Spacer*>(widget)) {
        sheet->setChanged(sheet->indexOf(m_strings.m_spacerName), true);
        return;
    }

    const int o = sheet->indexOf(m_strings.m_orientation);
    if (o != -1 && widget->inherits("QSplitter"))
        sheet->setChanged(o, true);

    if (QToolBar *toolBar = qobject_cast<QToolBar*>(widget)) {
        ToolBarEventFilter::install(toolBar);
        sheet->setVisible(sheet->indexOf(m_strings.m_windowTitle), true);
        toolBar->setFloatable(false);  // prevent toolbars from being dragged off
        return;
    }

    if (qobject_cast<QDockWidget*>(widget)) {
        sheet->setVisible(sheet->indexOf(m_strings.m_windowTitle), true);
        sheet->setVisible(sheet->indexOf(m_strings.m_windowIcon), true);
        return;
    }

    if (isMenu) {
        sheet->setChanged(sheet->indexOf(m_strings.m_title), true);
        return;
    }
    // helpers
    if (QToolBox *toolBox = qobject_cast<QToolBox*>(widget)) {
        QToolBoxHelper::install(toolBox);
        return;
    }
    if (QStackedWidget *stackedWidget = qobject_cast<QStackedWidget*>(widget)) {
        QStackedWidgetEventFilter::install(stackedWidget);
        return;
    }
    if (QTabWidget *tabWidget = qobject_cast<QTabWidget*>(widget)) {
        QTabWidgetEventFilter::install(tabWidget);
        return;
    }
    // Prevent embedded line edits from getting focus
    if (QAbstractSpinBox *asb = qobject_cast<QAbstractSpinBox *>(widget)) {
        if (QLineEdit *lineEdit = static_cast<FriendlySpinBox*>(asb)->lineEdit())
            lineEdit->setFocusPolicy(Qt::NoFocus);
        return;
    }
    if (QComboBox *cb =  qobject_cast<QComboBox *>(widget)) {
        if (QFontComboBox *fcb =  qobject_cast<QFontComboBox *>(widget)) {
            fcb->lineEdit()->setFocusPolicy(Qt::NoFocus); // Always present
            return;
        }
        cb->installEventFilter(new ComboEventFilter(cb));
        return;
    }
    if (QWizard *wz = qobject_cast<QWizard *>(widget)) {
        WizardPageChangeWatcher *pw = new WizardPageChangeWatcher(wz);
        Q_UNUSED(pw);
    }
}

static inline QString classNameOfStyle(const QStyle *s)
{
    return QLatin1String(s->metaObject()->className());
}

QString WidgetFactory::styleName() const
{
    return classNameOfStyle(style());
}

static inline bool isApplicationStyle(const QString &styleName)
{
    return styleName.isEmpty() || styleName == classNameOfStyle(qApp->style());
}

void WidgetFactory::setStyleName(const QString &styleName)
{
    m_currentStyle = isApplicationStyle(styleName) ? static_cast<QStyle*>(0) : getStyle(styleName);
}

QStyle *WidgetFactory::style() const
{
    return m_currentStyle ? m_currentStyle : qApp->style();
}

QStyle *WidgetFactory::getStyle(const QString &styleName)
{
    if (isApplicationStyle(styleName))
        return qApp->style();

    StyleCache::iterator it = m_styleCache.find(styleName);
    if (it == m_styleCache.end()) {
        QStyle *style = QStyleFactory::create(styleName);
        if (!style) {
            const QString msg = tr("Cannot create style '%1'.").arg(styleName);
            designerWarning(msg);
            return 0;
        }
        it = m_styleCache.insert(styleName, style);
    }
    return it.value();
}

void WidgetFactory::applyStyleTopLevel(const QString &styleName, QWidget *w)
{
    if (QStyle *style = getStyle(styleName))
        applyStyleToTopLevel(style, w);
}

void WidgetFactory::applyStyleToTopLevel(QStyle *style, QWidget *widget)
{
    if (!style)
        return;
    const QPalette standardPalette = style->standardPalette();
    if (widget->style() == style && widget->palette() == standardPalette)
        return;

    widget->setStyle(style);
    widget->setPalette(standardPalette);
    const QWidgetList lst = widget->findChildren<QWidget*>();
    const QWidgetList::const_iterator cend = lst.constEnd();
    for (QWidgetList::const_iterator it = lst.constBegin(); it != cend; ++it)
        (*it)->setStyle(style);
}

// Check for 'interactor' click on a tab bar,
// which can appear within a QTabWidget or as a standalone widget.

static bool isTabBarInteractor(const QTabBar *tabBar)
{
    // Tabbar embedded in Q(Designer)TabWidget, ie, normal tab widget case
    if (qobject_cast<const QTabWidget*>(tabBar->parentWidget()))
        return true;

    // Standalone tab bar on the form. Return true for tab rect areas
    // only to allow the user to select the tab bar by clicking outside the actual tabs.
    const int count = tabBar->count();
    if (count == 0)
        return false;

    // click into current tab: No Interaction
    const int currentIndex = tabBar->currentIndex();
    const QPoint pos = tabBar->mapFromGlobal(QCursor::pos());
    if (tabBar->tabRect(currentIndex).contains(pos))
        return false;

    // click outside: No Interaction
    const QRect geometry = QRect(QPoint(0, 0), tabBar->size());
    if (!geometry.contains(pos))
        return false;
    // click into another tab: Let's interact, switch tabs.
    for (int i = 0; i < count; i++)
        if (tabBar->tabRect(i).contains(pos))
            return true;
    return false;
}

bool WidgetFactory::isPassiveInteractor(QWidget *widget)
{
    static const QString qtPassive = QStringLiteral("__qt__passive_");
    static const QString qtMainWindowSplitter = QStringLiteral("qt_qmainwindow_extended_splitter");
    if (m_lastPassiveInteractor != 0 && (QWidget*)(*m_lastPassiveInteractor) == widget)
        return m_lastWasAPassiveInteractor;

    if (QApplication::activePopupWidget() || widget == 0) // if a popup is open, we have to make sure that this one is closed, else X might do funny things
        return true;

    m_lastWasAPassiveInteractor = false;
    (*m_lastPassiveInteractor) = widget;

    if (const QTabBar *tabBar = qobject_cast<const QTabBar*>(widget)) {
        if (isTabBarInteractor(tabBar))
            m_lastWasAPassiveInteractor = true;
        return m_lastWasAPassiveInteractor;
#ifndef QT_NO_SIZEGRIP
    }  else if (qobject_cast<QSizeGrip*>(widget)) {
        return (m_lastWasAPassiveInteractor = true);
#endif
    }  else if (qobject_cast<QMdiSubWindow*>(widget))
        return (m_lastWasAPassiveInteractor = true);
    else if (qobject_cast<QAbstractButton*>(widget) && (qobject_cast<QTabBar*>(widget->parent()) || qobject_cast<QToolBox*>(widget->parent())))
        return (m_lastWasAPassiveInteractor = true);
    else if (qobject_cast<QMenuBar*>(widget))
        return (m_lastWasAPassiveInteractor = true);
    else if (qobject_cast<QToolBar*>(widget))
        return (m_lastWasAPassiveInteractor = true);
    else if (qobject_cast<QScrollBar*>(widget)) {
        // A scroll bar is an interactor on a QAbstractScrollArea only.
        if (const QWidget *parent = widget->parentWidget()) {
            const QString objectName = parent->objectName();
            static const QString scrollAreaVContainer = QStringLiteral("qt_scrollarea_vcontainer");
            static const QString scrollAreaHContainer = QStringLiteral("qt_scrollarea_hcontainer");
            if (objectName == scrollAreaVContainer || objectName == scrollAreaHContainer) {
                m_lastWasAPassiveInteractor = true;
                return m_lastWasAPassiveInteractor;
            }
        }
    } else if (qstrcmp(widget->metaObject()->className(), "QDockWidgetTitle") == 0)
        return (m_lastWasAPassiveInteractor = true);
    else if (qstrcmp(widget->metaObject()->className(), "QWorkspaceTitleBar") == 0)
        return (m_lastWasAPassiveInteractor = true);
    const QString name = widget->objectName();
    if (name.startsWith(qtPassive) || name == qtMainWindowSplitter) {
        m_lastWasAPassiveInteractor = true;
        return m_lastWasAPassiveInteractor;
    }
    return m_lastWasAPassiveInteractor;
}

void WidgetFactory::formWindowAdded(QDesignerFormWindowInterface *formWindow)
{
    setFormWindowStyle(formWindow);
}

void WidgetFactory::activeFormWindowChanged(QDesignerFormWindowInterface *formWindow)
{
    setFormWindowStyle(formWindow);
}

void WidgetFactory::setFormWindowStyle(QDesignerFormWindowInterface *formWindow)
{
    if (FormWindowBase *fwb = qobject_cast<FormWindowBase *>(formWindow))
        setStyleName(fwb->styleName());
}

bool WidgetFactory::isFormEditorObject(const QObject *object)
{
    return object->property(formEditorDynamicProperty).isValid();
}
} // namespace qdesigner_internal

QT_END_NAMESPACE

#include "widgetfactory.moc"

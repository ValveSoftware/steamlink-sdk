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

#ifndef FORMWINDOW_H
#define FORMWINDOW_H

#include "formeditor_global.h"
#include <formwindowbase_p.h>

// Qt
#include <QtWidgets/QUndoStack>
#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QSet>
#include <QtCore/QPointer>

QT_BEGIN_NAMESPACE

class QDesignerDnDItemInterface;
class QDesignerTaskMenuExtension;
class DomConnections;
class DomUI;

class QWidget;
class QAction;
class QLabel;
class QTimer;
class QAction;
class QMenu;
class QRubberBand;

namespace qdesigner_internal {

class FormEditor;
class FormWindowCursor;
class WidgetEditorTool;
class FormWindowWidgetStack;
class FormWindowManager;
class FormWindowDnDItem;
class SetPropertyCommand;

class QT_FORMEDITOR_EXPORT FormWindow: public FormWindowBase
{
    Q_OBJECT

public:
    explicit FormWindow(FormEditor *core, QWidget *parent = 0, Qt::WindowFlags flags = 0);
    virtual ~FormWindow();

    QDesignerFormEditorInterface *core() const Q_DECL_OVERRIDE;

    QDesignerFormWindowCursorInterface *cursor() const Q_DECL_OVERRIDE;

    // Overwritten: FormWindowBase
    QWidget *formContainer() const Q_DECL_OVERRIDE;

    int toolCount() const Q_DECL_OVERRIDE;
    int currentTool() const Q_DECL_OVERRIDE;
    void setCurrentTool(int index) Q_DECL_OVERRIDE;
    QDesignerFormWindowToolInterface *tool(int index) const Q_DECL_OVERRIDE;
    void registerTool(QDesignerFormWindowToolInterface *tool) Q_DECL_OVERRIDE;

    QString author() const Q_DECL_OVERRIDE;
    void setAuthor(const QString &author) Q_DECL_OVERRIDE;

    QString comment() const Q_DECL_OVERRIDE;
    void setComment(const QString &comment) Q_DECL_OVERRIDE;

    void layoutDefault(int *margin, int *spacing) Q_DECL_OVERRIDE;
    void setLayoutDefault(int margin, int spacing) Q_DECL_OVERRIDE;

    void layoutFunction(QString *margin, QString *spacing) Q_DECL_OVERRIDE;
    void setLayoutFunction(const QString &margin, const QString &spacing) Q_DECL_OVERRIDE;

    QString pixmapFunction() const Q_DECL_OVERRIDE;
    void setPixmapFunction(const QString &pixmapFunction) Q_DECL_OVERRIDE;

    QString exportMacro() const Q_DECL_OVERRIDE;
    void setExportMacro(const QString &exportMacro) Q_DECL_OVERRIDE;

    QStringList includeHints() const Q_DECL_OVERRIDE;
    void setIncludeHints(const QStringList &includeHints) Q_DECL_OVERRIDE;

    QString fileName() const Q_DECL_OVERRIDE;
    void setFileName(const QString &fileName) Q_DECL_OVERRIDE;

    QString contents() const Q_DECL_OVERRIDE;
    bool setContents(QIODevice *dev, QString *errorMessage = 0) Q_DECL_OVERRIDE;
    bool setContents(const QString &) Q_DECL_OVERRIDE;

    QDir absoluteDir() const Q_DECL_OVERRIDE;

    void simplifySelection(QWidgetList *sel) const Q_DECL_OVERRIDE;

    void ensureUniqueObjectName(QObject *object) Q_DECL_OVERRIDE;

    QWidget *mainContainer() const Q_DECL_OVERRIDE;
    void setMainContainer(QWidget *mainContainer) Q_DECL_OVERRIDE;
    bool isMainContainer(const QWidget *w) const;

    QWidget *currentWidget() const;

    bool hasInsertedChildren(QWidget *w) const;

    QList<QWidget *> selectedWidgets() const;
    void clearSelection(bool changePropertyDisplay = true) Q_DECL_OVERRIDE;
    bool isWidgetSelected(QWidget *w) const;
    void selectWidget(QWidget *w, bool select = true) Q_DECL_OVERRIDE;

    void selectWidgets();
    void repaintSelection();
    void updateSelection(QWidget *w);
    void updateChildSelections(QWidget *w);
    void raiseChildSelections(QWidget *w);
    void raiseSelection(QWidget *w);

    inline const QList<QWidget *>& widgets() const { return m_widgets; }
    inline int widgetCount() const { return m_widgets.count(); }
    inline QWidget *widgetAt(int index) const { return m_widgets.at(index); }

    QList<QWidget *> widgets(QWidget *widget) const;

    QWidget *createWidget(DomUI *ui, const QRect &rect, QWidget *target);

    bool isManaged(QWidget *w) const Q_DECL_OVERRIDE;

    void manageWidget(QWidget *w) Q_DECL_OVERRIDE;
    void unmanageWidget(QWidget *w) Q_DECL_OVERRIDE;

    QUndoStack *commandHistory() const Q_DECL_OVERRIDE;
    void beginCommand(const QString &description) Q_DECL_OVERRIDE;
    void endCommand() Q_DECL_OVERRIDE;

    bool blockSelectionChanged(bool blocked) Q_DECL_OVERRIDE;
    void emitSelectionChanged() Q_DECL_OVERRIDE;

    bool unify(QObject *w, QString &s, bool changeIt);

    bool isDirty() const Q_DECL_OVERRIDE;
    void setDirty(bool dirty) Q_DECL_OVERRIDE;

    static FormWindow *findFormWindow(QWidget *w);

    virtual QWidget *containerAt(const QPoint &pos);
    QWidget *widgetAt(const QPoint &pos) Q_DECL_OVERRIDE;
    void highlightWidget(QWidget *w, const QPoint &pos, HighlightMode mode = Highlight) Q_DECL_OVERRIDE;

    void updateOrderIndicators();

    bool handleEvent(QWidget *widget, QWidget *managedWidget, QEvent *event);

    QStringList resourceFiles() const Q_DECL_OVERRIDE;
    void addResourceFile(const QString &path) Q_DECL_OVERRIDE;
    void removeResourceFile(const QString &path) Q_DECL_OVERRIDE;

    void resizeWidget(QWidget *widget, const QRect &geometry);

    bool dropDockWidget(QDesignerDnDItemInterface *item, const QPoint &global_mouse_pos);
    bool dropWidgets(const QList<QDesignerDnDItemInterface*> &item_list, QWidget *target,
                     const QPoint &global_mouse_pos) Q_DECL_OVERRIDE;

    QWidget *findContainer(QWidget *w, bool excludeLayout) const Q_DECL_OVERRIDE;
    // for WidgetSelection only.
    QWidget *designerWidget(QWidget *w) const;

    //  Initialize and return a popup menu for a managed widget
    QMenu *initializePopupMenu(QWidget *managedWidget) Q_DECL_OVERRIDE;

#ifndef QT_NO_CLIPBOARD
    void paste(PasteMode pasteMode) Q_DECL_OVERRIDE;
#endif
    virtual QEditorFormBuilder *createFormBuilder() Q_DECL_OVERRIDE;

    bool eventFilter(QObject *watched, QEvent *event) Q_DECL_OVERRIDE;

signals:
    void contextMenuRequested(QMenu *menu, QWidget *widget);

public slots:
    void deleteWidgets();
    void raiseWidgets();
    void lowerWidgets();
#ifndef QT_NO_CLIPBOARD
    void copy();
    void cut();
    void paste();
#endif
    void selectAll();

    void createLayout(int type, QWidget *container = 0);
    void morphLayout(QWidget *container, int newType);
    void breakLayout(QWidget *w);

    void editContents();

protected:
    virtual QMenu *createPopupMenu(QWidget *w);
    void resizeEvent(QResizeEvent *e) Q_DECL_OVERRIDE;

    void insertWidget(QWidget *w, const QRect &rect, QWidget *target, bool already_in_form = false);

private slots:
    void selectionChangedTimerDone();
    void checkSelection();
    void checkSelectionNow();
    void slotSelectWidget(QAction *);

private:
    enum MouseState {
        NoMouseState,
        // Double click received
        MouseDoubleClicked,
        // Drawing selection rubber band rectangle
        MouseDrawRubber,
        // Started a move operation
        MouseMoveDrag,
        // Click on a widget whose parent is selected. Defer selection to release
        MouseDeferredSelection
    };
    MouseState m_mouseState;
    QPointer<QWidget> m_lastClickedWidget;

    void init();
    void initializeCoreTools();

    int getValue(const QRect &rect, int key, bool size) const;
    int calcValue(int val, bool forward, bool snap, int snapOffset) const;
    void handleClickSelection(QWidget *managedWidget, unsigned mouseFlags);

    bool frameNeeded(QWidget *w) const;

    enum RectType { Insert, Rubber };

    void startRectDraw(const QPoint &global, QWidget *, RectType t);
    void continueRectDraw(const QPoint &global, QWidget *, RectType t);
    void endRectDraw();

    QWidget *containerAt(const QPoint &pos, QWidget *notParentOf);

    void checkPreviewGeometry(QRect &r);

    bool handleContextMenu(QWidget *widget, QWidget *managedWidget, QContextMenuEvent *e);
    bool handleMouseButtonDblClickEvent(QWidget *widget, QWidget *managedWidget, QMouseEvent *e);
    bool handleMousePressEvent(QWidget *widget, QWidget *managedWidget, QMouseEvent *e);
    bool handleMouseMoveEvent(QWidget *widget, QWidget *managedWidget, QMouseEvent *e);
    bool handleMouseReleaseEvent(QWidget *widget, QWidget *managedWidget, QMouseEvent *e);
    bool handleKeyPressEvent(QWidget *widget, QWidget *managedWidget, QKeyEvent *e);
    bool handleKeyReleaseEvent(QWidget *widget, QWidget *managedWidget, QKeyEvent *e);

    bool isCentralWidget(QWidget *w) const;

    bool setCurrentWidget(QWidget *currentWidget);
    bool trySelectWidget(QWidget *w, bool select);

    void dragWidgetWithinForm(QWidget *widget, const QRect &targetGeometry, QWidget *targetContainer);

    void setCursorToAll(const QCursor &c, QWidget *start);

    QPoint mapToForm(const QWidget *w, const QPoint &pos) const;
    bool canBeBuddy(QWidget *w) const;

    QWidget *findTargetContainer(QWidget *widget) const;

    void clearMainContainer();

    static int widgetDepth(const QWidget *w);
    static bool isChildOf(const QWidget *c, const QWidget *p);

    void editWidgets() Q_DECL_OVERRIDE;

    void updateWidgets();

    void handleArrowKeyEvent(int key, Qt::KeyboardModifiers modifiers);

    void layoutSelection(int type);
    void layoutContainer(QWidget *w, int type);

private:
    QWidget *innerContainer(QWidget *outerContainer) const;
    QWidget *containerForPaste() const;
    QAction *createSelectAncestorSubMenu(QWidget *w);
    void selectSingleWidget(QWidget *w);

    FormEditor *m_core;
    FormWindowCursor *m_cursor;
    QWidget *m_mainContainer;
    QWidget *m_currentWidget;

    bool m_blockSelectionChanged;

    QPoint m_rectAnchor;
    QRect m_currRect;

    QWidgetList m_widgets;
    QSet<QWidget*> m_insertedWidgets;

    class Selection;
    Selection *m_selection;

    QPoint m_startPos;

    QUndoStack m_undoStack;

    QString m_fileName;

    typedef QPair<QPalette ,bool> PaletteAndFill;
    typedef QMap<QWidget*, PaletteAndFill> WidgetPaletteMap;
    WidgetPaletteMap m_palettesBeforeHighlight;

    QRubberBand *m_rubberBand;

    QTimer *m_selectionChangedTimer;
    QTimer *m_checkSelectionTimer;
    QTimer *m_geometryChangedTimer;

    FormWindowWidgetStack *m_widgetStack;
    WidgetEditorTool *m_widgetEditor;

    QStringList m_resourceFiles;

    QString m_comment;
    QString m_author;
    QString m_pixmapFunction;
    int m_defaultMargin, m_defaultSpacing;
    QString m_marginFunction, m_spacingFunction;
    QString m_exportMacro;
    QStringList m_includeHints;

    QPoint m_contextMenuPosition;

private:
    friend class WidgetEditorTool;
};

}  // namespace qdesigner_internal

QT_END_NAMESPACE

#endif // FORMWINDOW_H

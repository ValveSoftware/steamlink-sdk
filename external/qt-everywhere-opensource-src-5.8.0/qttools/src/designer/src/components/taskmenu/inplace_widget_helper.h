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

#ifndef INPLACE_WIDGETHELPER_H
#define INPLACE_WIDGETHELPER_H


#include <QtCore/QObject>
#include <QtCore/QPoint>
#include <QtCore/QSize>
#include <QtCore/QPointer>
#include <qglobal.h>

QT_BEGIN_NAMESPACE

class QDesignerFormWindowInterface;

namespace qdesigner_internal {

    // A helper class to make an editor widget suitable for form inline
    // editing. Derive from the editor widget class and  make InPlaceWidgetHelper  a member.
    //
    // Sets "destructive close" on the editor widget and
    // wires "ESC" to it.
    // Installs an event filter on the parent to listen for
    // resize events and passes them on to the child.
    // You might want to connect editingFinished() to close() of the editor widget.
    class InPlaceWidgetHelper: public QObject
    {
        Q_OBJECT
    public:
        InPlaceWidgetHelper(QWidget *editorWidget, QWidget *parentWidget, QDesignerFormWindowInterface *fw);
        virtual ~InPlaceWidgetHelper();

        bool eventFilter(QObject *object, QEvent *event) Q_DECL_OVERRIDE;

        // returns a recommended alignment for the editor widget determined from the parent.
        Qt::Alignment alignment() const;
    private:
        QWidget *m_editorWidget;
        QPointer<QWidget> m_parentWidget;
        const bool m_noChildEvent;
        QPoint m_posOffset;
        QSize m_sizeOffset;
    };

}  // namespace qdesigner_internal

QT_END_NAMESPACE

#endif // INPLACE_WIDGETHELPER_H

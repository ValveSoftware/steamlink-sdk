/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QWEBENGINEVIEW_PLUGIN_H
#define QWEBENGINEVIEW_PLUGIN_H

#include <QtUiPlugin/QDesignerCustomWidgetInterface>
#include <QtWidgets/QWidget>
#include <QtGui/QIcon>
#include <QtCore/QUrl>

QT_BEGIN_NAMESPACE

namespace fake {
// A lightweight "fake" QWebEngineView class that is returned when Qt Designer
// queries the default property values by calling
// QDesignerCustomWidgetInterface::createWidget() with 0 parent, preventing
// crashes during QWebEngine initialization (QTBUG-53984).
// The property list needs to be kept in sync with QWebEngineView.
class QWebEngineView : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QString title READ title)
    Q_PROPERTY(QUrl url READ url WRITE setUrl) // Designable
    Q_PROPERTY(QUrl iconUrl READ iconUrl)
    Q_PROPERTY(QIcon icon READ icon)
    Q_PROPERTY(QString selectedText READ selectedText)
    Q_PROPERTY(bool hasSelection READ hasSelection)
    Q_PROPERTY(qreal zoomFactor READ zoomFactor WRITE setZoomFactor) // Designable

public:
    explicit QWebEngineView(QWidget *parent = Q_NULLPTR) : QWidget(parent) {}

    QString title() const { return QString(); }
    QUrl url() const { return QUrl(); }
    void setUrl(const QUrl &) {}
    QUrl iconUrl() const { return QUrl(); }
    QIcon icon() const { return QIcon(); }
    QString selectedText() { return QString(); }
    bool hasSelection() { return false; }
    qreal zoomFactor() const { return 1; }
    void setZoomFactor(qreal) {}
};
} // namespace fake

class QWebEngineViewPlugin: public QObject, public QDesignerCustomWidgetInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QDesignerCustomWidgetInterface")
    Q_INTERFACES(QDesignerCustomWidgetInterface)
public:
    explicit QWebEngineViewPlugin(QObject *parent = Q_NULLPTR);

    QString name() const Q_DECL_OVERRIDE;
    QString group() const Q_DECL_OVERRIDE;
    QString toolTip() const Q_DECL_OVERRIDE;
    QString whatsThis() const Q_DECL_OVERRIDE;
    QString includeFile() const Q_DECL_OVERRIDE;
    QIcon icon() const Q_DECL_OVERRIDE;
    bool isContainer() const Q_DECL_OVERRIDE;
    QWidget *createWidget(QWidget *parent) Q_DECL_OVERRIDE;
    bool isInitialized() const Q_DECL_OVERRIDE;
    void initialize(QDesignerFormEditorInterface *core) Q_DECL_OVERRIDE;
    QString domXml() const Q_DECL_OVERRIDE;

private:
    bool m_initialized;
};

QT_END_NAMESPACE

#endif // QWEBENGINEVIEW_PLUGIN_H

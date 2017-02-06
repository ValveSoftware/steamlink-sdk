/****************************************************************************
**
** Copyright (C) 2016 Stephen Kelly <steveire@gmail.com>
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
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

#include <QtDesigner>
#include <QtDesigner/QtDesigner>
#include <QtDesigner/QDesignerCustomWidgetInterface>
#include <QDesignerCustomWidgetInterface>

#include <QWidget>

#if TEST_UIPLUGIN_USAGE_REQUIREMENTS
#  ifndef QT_UIPLUGIN_LIB
#    error Expect QT_UIPLUGIN_LIB define
#  endif
#endif

class MyPlugin : public QObject, public QDesignerCustomWidgetInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QDesignerCustomWidgetInterface")
    Q_INTERFACES(QDesignerCustomWidgetInterface)
public:
    MyPlugin(QObject *parent = 0)
      : QObject(parent)
      , initialized(false)
    {

    }

    bool isContainer() const { return true; }
    bool isInitialized() const { return initialized; }
    QIcon icon() const { return QIcon(); }
    QString domXml() const { return QString(); }
    QString group() const { return QString(); }
    QString includeFile() const { return QString(); }
    QString name() const { return QString(); }
    QString toolTip() const { return QString(); }
    QString whatsThis() const { return QString(); }
    QWidget *createWidget(QWidget *parent) { return new QWidget(parent); }
    void initialize(QDesignerFormEditorInterface *)
    {
        if (initialized)
            return;
        initialized = true;
    }

private:
    bool initialized;
};

#include "my_designer_plugin.moc"

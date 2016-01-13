/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Quick Controls module of the Qt Toolkit.
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

#include "qquickstyleitem_p.h"

#include <qstringbuilder.h>
#include <qpainter.h>
#include <qpixmapcache.h>
#include <qstyle.h>
#include <qstyleoption.h>
#include <qapplication.h>
#include <qsgsimpletexturenode.h>
#include <qquickwindow.h>
#include "private/qguiapplication_p.h"
#include <QtQuick/private/qquickwindow_p.h>
#include <QtQuick/private/qquickitem_p.h>
#include <QtGui/qpa/qplatformtheme.h>
#include "../qquickmenuitem_p.h"

QT_BEGIN_NAMESPACE

#ifdef Q_OS_OSX
#include <Carbon/Carbon.h>

static inline HIRect qt_hirectForQRect(const QRect &convertRect, const QRect &rect = QRect())
{
    return CGRectMake(convertRect.x() + rect.x(), convertRect.y() + rect.y(),
                      convertRect.width() - rect.width(), convertRect.height() - rect.height());
}

/*! \internal

    Returns the CoreGraphics CGContextRef of the paint device. 0 is
    returned if it can't be obtained. It is the caller's responsibility to
    CGContextRelease the context when finished using it.

    \warning This function is only available on Mac OS X.
    \warning This function is duplicated in qmacstyle_mac.mm
*/
CGContextRef qt_mac_cg_context(const QPaintDevice *pdev)
{

    if (pdev->devType() == QInternal::Image) {
         const QImage *i = static_cast<const  QImage*>(pdev);
         QImage *image = const_cast< QImage*>(i);
        CGColorSpaceRef colorspace = CGColorSpaceCreateDeviceRGB();
        uint flags = kCGImageAlphaPremultipliedFirst;
        flags |= kCGBitmapByteOrder32Host;
        CGContextRef ret = 0;

        ret = CGBitmapContextCreate(image->bits(), image->width(), image->height(),
                                    8, image->bytesPerLine(), colorspace, flags);

        CGContextTranslateCTM(ret, 0, image->height());
        CGContextScaleCTM(ret, 1, -1);
        return ret;
    }
    return 0;
}

#endif

class QQuickStyleNode : public QSGNinePatchNode
{
public:
    QQuickStyleNode()
        : m_geometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4)
    {
        m_geometry.setDrawingMode(GL_TRIANGLE_STRIP);
        setGeometry(&m_geometry);
        // The texture material has mipmap filtering set to Nearest by default. This is not ideal.
        m_material.setMipmapFiltering(QSGTexture::None);
        setMaterial(&m_material);
    }

    ~QQuickStyleNode()
    {
        delete m_material.texture();
    }

    virtual void setTexture(QSGTexture *texture)
    {
        delete m_material.texture();
        m_material.setTexture(texture);
    }

    virtual void setBounds(const QRectF &bounds)
    {
        m_bounds = bounds;
    }

    virtual void setDevicePixelRatio(qreal ratio)
    {
        m_devicePixelRatio = ratio;
    }

    virtual void setPadding(qreal left, qreal top, qreal right, qreal bottom)
    {
        m_paddingLeft = left;
        m_paddingTop = top;
        m_paddingRight = right;
        m_paddingBottom = bottom;
    }

    virtual void update() {
        QSGTexture *texture = m_material.texture();

        if (m_paddingLeft <= 0 && m_paddingTop <= 0 && m_paddingRight <= 0 && m_paddingBottom <= 0) {
            m_geometry.allocate(4, 0);
            QSGGeometry::updateTexturedRectGeometry(&m_geometry, m_bounds, texture->normalizedTextureSubRect());
            markDirty(QSGNode::DirtyGeometry | QSGNode::DirtyMaterial);
            return;
        }

        QRectF tc = texture->normalizedTextureSubRect();
        QSize ts = texture->textureSize();
        ts.setHeight(ts.height() / m_devicePixelRatio);
        ts.setWidth(ts.width() / m_devicePixelRatio);

        qreal invtw = tc.width() / ts.width();
        qreal invth = tc.height() / ts.height();

        struct Coord { qreal p; qreal t; };
        Coord cx[4] = { { m_bounds.left(), tc.left() },
                        { m_bounds.left() + m_paddingLeft, tc.left() + m_paddingLeft * invtw },
                        { m_bounds.right() - m_paddingRight, tc.right() - m_paddingRight * invtw },
                        { m_bounds.right(), tc.right() }
                      };
        Coord cy[4] = { { m_bounds.top(), tc.top() },
                        { m_bounds.top() + m_paddingTop, tc.top() + m_paddingTop * invth },
                        { m_bounds.bottom() - m_paddingBottom, tc.bottom() - m_paddingBottom * invth },
                        { m_bounds.bottom(), tc.bottom() }
                      };

        m_geometry.allocate(16, 28);
        QSGGeometry::TexturedPoint2D *v = m_geometry.vertexDataAsTexturedPoint2D();
        for (int y=0; y<4; ++y) {
            for (int x=0; x<4; ++x) {
                v->set(cx[x].p, cy[y].p, cx[x].t, cy[y].t);
                ++v;
            }
        }

        quint16 *i = m_geometry.indexDataAsUShort();
        for (int r=0; r<3; ++r) {
            if (r > 0)
                *i++ = 4 * r;
            for (int c=0; c<4; ++c) {
                i[0] = 4 * r + c;
                i[1] = 4 * r + c + 4;
                i+=2;
            }
            if (r < 2)
                *i++ = 4 * r + 3 + 4;
        }

        markDirty(QSGNode::DirtyGeometry | QSGNode::DirtyMaterial);

//        v = m_geometry.vertexDataAsTexturedPoint2D();
//        for (int j=0; j<m_geometry.vertexCount(); ++j)
//            qDebug() << v[j].x << v[j].y << v[j].tx << v[j].ty;

//        i = m_geometry.indexDataAsUShort();
//        for (int j=0; j<m_geometry.indexCount(); ++j)
//            qDebug() << i[j];

    }

    QRectF m_bounds;
    qreal m_devicePixelRatio;
    qreal m_paddingLeft;
    qreal m_paddingTop;
    qreal m_paddingRight;
    qreal m_paddingBottom;
    QSGGeometry m_geometry;
    QSGTextureMaterial m_material;
};

QQuickStyleItem::QQuickStyleItem(QQuickItem *parent)
    : QQuickItem(parent),
    m_styleoption(0),
    m_itemType(Undefined),
    m_sunken(false),
    m_raised(false),
    m_active(true),
    m_selected(false),
    m_focus(false),
    m_hover(false),
    m_on(false),
    m_horizontal(true),
    m_transient(false),
    m_sharedWidget(false),
    m_minimum(0),
    m_maximum(100),
    m_value(0),
    m_step(0),
    m_paintMargins(0),
    m_contentWidth(0),
    m_contentHeight(0),
    m_textureWidth(0),
    m_textureHeight(0)
{
    m_font = qApp->font();
    setFlag(QQuickItem::ItemHasContents, true);
    setSmooth(false);

    connect(this, SIGNAL(visibleChanged()), this, SLOT(updateItem()));
    connect(this, SIGNAL(widthChanged()), this, SLOT(updateItem()));
    connect(this, SIGNAL(heightChanged()), this, SLOT(updateItem()));
    connect(this, SIGNAL(enabledChanged()), this, SLOT(updateItem()));
    connect(this, SIGNAL(infoChanged()), this, SLOT(updateItem()));
    connect(this, SIGNAL(onChanged()), this, SLOT(updateItem()));
    connect(this, SIGNAL(selectedChanged()), this, SLOT(updateItem()));
    connect(this, SIGNAL(activeChanged()), this, SLOT(updateItem()));
    connect(this, SIGNAL(textChanged()), this, SLOT(updateSizeHint()));
    connect(this, SIGNAL(textChanged()), this, SLOT(updateItem()));
    connect(this, SIGNAL(activeChanged()), this, SLOT(updateItem()));
    connect(this, SIGNAL(raisedChanged()), this, SLOT(updateItem()));
    connect(this, SIGNAL(sunkenChanged()), this, SLOT(updateItem()));
    connect(this, SIGNAL(hoverChanged()), this, SLOT(updateItem()));
    connect(this, SIGNAL(maximumChanged()), this, SLOT(updateItem()));
    connect(this, SIGNAL(minimumChanged()), this, SLOT(updateItem()));
    connect(this, SIGNAL(valueChanged()), this, SLOT(updateItem()));
    connect(this, SIGNAL(horizontalChanged()), this, SLOT(updateItem()));
    connect(this, SIGNAL(transientChanged()), this, SLOT(updateItem()));
    connect(this, SIGNAL(activeControlChanged()), this, SLOT(updateItem()));
    connect(this, SIGNAL(hasFocusChanged()), this, SLOT(updateItem()));
    connect(this, SIGNAL(activeControlChanged()), this, SLOT(updateItem()));
    connect(this, SIGNAL(hintChanged()), this, SLOT(updateItem()));
    connect(this, SIGNAL(propertiesChanged()), this, SLOT(updateSizeHint()));
    connect(this, SIGNAL(propertiesChanged()), this, SLOT(updateItem()));
    connect(this, SIGNAL(elementTypeChanged()), this, SLOT(updateItem()));
    connect(this, SIGNAL(contentWidthChanged(int)), this, SLOT(updateSizeHint()));
    connect(this, SIGNAL(contentHeightChanged(int)), this, SLOT(updateSizeHint()));
    connect(this, SIGNAL(widthChanged()), this, SLOT(updateRect()));
    connect(this, SIGNAL(heightChanged()), this, SLOT(updateRect()));

    connect(this, SIGNAL(heightChanged()), this, SLOT(updateBaselineOffset()));
    connect(this, SIGNAL(contentHeightChanged(int)), this, SLOT(updateBaselineOffset()));
}

QQuickStyleItem::~QQuickStyleItem()
{
    delete m_styleoption;
    m_styleoption = 0;
}

void QQuickStyleItem::initStyleOption()
{
    QString type = elementType();
    if (m_styleoption)
        m_styleoption->state = 0;

    QString sizeHint = m_hints.value("size").toString();
    QPlatformTheme::Font platformFont = (sizeHint == "mini") ? QPlatformTheme::MiniFont :
                                        (sizeHint == "small") ? QPlatformTheme::SmallFont :
                                        QPlatformTheme::SystemFont;

    bool needsResolvePalette = true;

    switch (m_itemType) {
    case Button: {
        if (!m_styleoption)
            m_styleoption = new QStyleOptionButton();

        QStyleOptionButton *opt = qstyleoption_cast<QStyleOptionButton*>(m_styleoption);
        opt->text = text();
        opt->icon = m_properties["icon"].value<QIcon>();
        int e = qApp->style()->pixelMetric(QStyle::PM_ButtonIconSize, m_styleoption, 0);
        opt->iconSize = QSize(e, e);
        opt->features = (activeControl() == "default") ?
                    QStyleOptionButton::DefaultButton :
                    QStyleOptionButton::None;
        if (platformFont == QPlatformTheme::SystemFont)
            platformFont = QPlatformTheme::PushButtonFont;
        const QFont *font = QGuiApplicationPrivate::platformTheme()->font(platformFont);
        if (font)
            opt->fontMetrics = QFontMetrics(*font);
        QObject * menu = m_properties["menu"].value<QObject *>();
        if (menu) {
            opt->features |= QStyleOptionButton::HasMenu;
#ifdef Q_OS_OSX
            if (style() == "mac") {
                if (platformFont == QPlatformTheme::PushButtonFont)
                    menu->setProperty("__xOffset", 12);
                else
                    menu->setProperty("__xOffset", 11);
                if (platformFont == QPlatformTheme::MiniFont)
                    menu->setProperty("__yOffset", 5);
                else if (platformFont == QPlatformTheme::SmallFont)
                    menu->setProperty("__yOffset", 6);
                else
                    menu->setProperty("__yOffset", 3);
                if (font)
                    menu->setProperty("__font", *font);
            }
#endif
        }
    }
        break;
    case ItemRow: {
        if (!m_styleoption)
            m_styleoption = new QStyleOptionViewItem();

        QStyleOptionViewItem *opt = qstyleoption_cast<QStyleOptionViewItem*>(m_styleoption);
        opt->features = 0;
        if (activeControl() == "alternate")
            opt->features |= QStyleOptionViewItem::Alternate;
    }
        break;

    case Splitter: {
        if (!m_styleoption) {
            m_styleoption = new QStyleOption;
        }
    }
        break;

    case Item: {
        if (!m_styleoption) {
            m_styleoption = new QStyleOptionViewItem();
        }
        QStyleOptionViewItem *opt = qstyleoption_cast<QStyleOptionViewItem*>(m_styleoption);
        opt->features = QStyleOptionViewItem::HasDisplay;
        opt->text = text();
        opt->textElideMode = Qt::ElideRight;
        opt->displayAlignment = Qt::AlignLeft | Qt::AlignVCenter;
        opt->decorationAlignment = Qt::AlignCenter;
        resolvePalette();
        needsResolvePalette = false;
        QPalette pal = m_styleoption->palette;
        pal.setBrush(QPalette::Base, Qt::NoBrush);
        m_styleoption->palette = pal;
        if (const QFont *font = QGuiApplicationPrivate::platformTheme()->font(QPlatformTheme::ItemViewFont)) {
            opt->fontMetrics = QFontMetrics(*font);
            opt->font = *font;
        }
    }
        break;
    case Header: {
        if (!m_styleoption)
            m_styleoption = new QStyleOptionHeader();

        QStyleOptionHeader *opt = qstyleoption_cast<QStyleOptionHeader*>(m_styleoption);
        opt->text = text();
        opt->textAlignment = static_cast<Qt::AlignmentFlag>(m_properties.value("textalignment").toInt());
        opt->sortIndicator = activeControl() == "down" ?
                    QStyleOptionHeader::SortDown
                  : activeControl() == "up" ?
                        QStyleOptionHeader::SortUp : QStyleOptionHeader::None;
        QString headerpos = m_properties.value("headerpos").toString();
        if (headerpos == "beginning")
            opt->position = QStyleOptionHeader::Beginning;
        else if (headerpos == "end")
            opt->position = QStyleOptionHeader::End;
        else if (headerpos == "only")
            opt->position = QStyleOptionHeader::OnlyOneSection;
        else
            opt->position = QStyleOptionHeader::Middle;
        if (const QFont *font = QGuiApplicationPrivate::platformTheme()->font(QPlatformTheme::HeaderViewFont))
            opt->fontMetrics = QFontMetrics(*font);
    }
        break;
    case ToolButton: {
        if (!m_styleoption)
            m_styleoption = new QStyleOptionToolButton();

        QStyleOptionToolButton *opt =
                qstyleoption_cast<QStyleOptionToolButton*>(m_styleoption);
        opt->subControls = QStyle::SC_ToolButton;
        opt->state |= QStyle::State_AutoRaise;
        opt->activeSubControls = QStyle::SC_ToolButton;
        opt->text = text();
        opt->icon = m_properties["icon"].value<QIcon>();

        if (m_properties.value("menu").toBool()) {
            opt->subControls |= QStyle::SC_ToolButtonMenu;
            opt->features = QStyleOptionToolButton::HasMenu;
        }

        // For now icon only is displayed by default.
        opt->toolButtonStyle = Qt::ToolButtonIconOnly;
        if (opt->icon.isNull() && !opt->text.isEmpty())
            opt->toolButtonStyle = Qt::ToolButtonTextOnly;

        int e = qApp->style()->pixelMetric(QStyle::PM_ToolBarIconSize, m_styleoption, 0);
        opt->iconSize = QSize(e, e);

        if (const QFont *font = QGuiApplicationPrivate::platformTheme()->font(QPlatformTheme::ToolButtonFont)) {
            opt->fontMetrics = QFontMetrics(*font);
            opt->font = *font;
        }

    }
        break;
    case ToolBar: {
        if (!m_styleoption)
            m_styleoption = new QStyleOptionToolBar();
    }
        break;
    case Tab: {
        if (!m_styleoption)
            m_styleoption = new QStyleOptionTab();

        QStyleOptionTab *opt = qstyleoption_cast<QStyleOptionTab*>(m_styleoption);
        opt->text = text();

        if (m_properties.value("hasFrame").toBool())
            opt->features |= QStyleOptionTab::HasFrame;

        QString orientation = m_properties.value("orientation").toString();
        QString position = m_properties.value("tabpos").toString();
        QString selectedPosition = m_properties.value("selectedpos").toString();

        opt->shape = (orientation == "Bottom") ? QTabBar::RoundedSouth : QTabBar::RoundedNorth;
        if (position == QLatin1String("beginning"))
            opt->position = QStyleOptionTab::Beginning;
        else if (position == QLatin1String("end"))
            opt->position = QStyleOptionTab::End;
        else if (position == QLatin1String("only"))
            opt->position = QStyleOptionTab::OnlyOneTab;
        else
            opt->position = QStyleOptionTab::Middle;

        if (selectedPosition == QLatin1String("next"))
            opt->selectedPosition = QStyleOptionTab::NextIsSelected;
        else if (selectedPosition == QLatin1String("previous"))
            opt->selectedPosition = QStyleOptionTab::PreviousIsSelected;
        else
            opt->selectedPosition = QStyleOptionTab::NotAdjacent;


    } break;

    case Frame: {
        if (!m_styleoption)
            m_styleoption = new QStyleOptionFrame();

        QStyleOptionFrame *opt = qstyleoption_cast<QStyleOptionFrame*>(m_styleoption);
        opt->frameShape = QFrame::StyledPanel;
        opt->lineWidth = 1;
        opt->midLineWidth = 1;
    }
        break;
    case FocusRect: {
        if (!m_styleoption)
            m_styleoption = new QStyleOptionFocusRect();
        // Needed on windows
        m_styleoption->state |= QStyle::State_KeyboardFocusChange;
    }
        break;
    case TabFrame: {
        if (!m_styleoption)
            m_styleoption = new QStyleOptionTabWidgetFrame();
        QStyleOptionTabWidgetFrame *opt = qstyleoption_cast<QStyleOptionTabWidgetFrame*>(m_styleoption);

        opt->selectedTabRect = m_properties["selectedTabRect"].toRect();
        opt->shape = m_properties["orientation"] == Qt::BottomEdge ? QTabBar::RoundedSouth : QTabBar::RoundedNorth;
        if (minimum())
            opt->selectedTabRect = QRect(value(), 0, minimum(), height());
        opt->tabBarSize = QSize(minimum() , height());
        // oxygen style needs this hack
        opt->leftCornerWidgetSize = QSize(value(), 0);
    }
        break;
    case MenuBar:
        if (!m_styleoption) {
            QStyleOptionMenuItem *menuOpt = new QStyleOptionMenuItem();
            menuOpt->menuItemType = QStyleOptionMenuItem::EmptyArea;
            m_styleoption = menuOpt;
        }

        break;
    case MenuBarItem:
    {
        if (!m_styleoption)
            m_styleoption = new QStyleOptionMenuItem();

        QStyleOptionMenuItem *opt = qstyleoption_cast<QStyleOptionMenuItem*>(m_styleoption);
        opt->text = text();
        opt->menuItemType = QStyleOptionMenuItem::Normal;
        setProperty("_q_showUnderlined", m_hints["showUnderlined"].toBool());

        if (const QFont *font = QGuiApplicationPrivate::platformTheme()->font(QPlatformTheme::MenuBarFont)) {
            opt->font = *font;
            opt->fontMetrics = QFontMetrics(opt->font);
            m_font = opt->font;
        }
    }
        break;
    case Menu: {
        if (!m_styleoption)
            m_styleoption = new QStyleOptionMenuItem();
    }
        break;
    case MenuItem:
    case ComboBoxItem:
    {
        if (!m_styleoption)
            m_styleoption = new QStyleOptionMenuItem();

        QStyleOptionMenuItem *opt = qstyleoption_cast<QStyleOptionMenuItem*>(m_styleoption);
        // For GTK style. See below, in setElementType()
        setProperty("_q_isComboBoxPopupItem", m_itemType == ComboBoxItem);

        QQuickMenuItemType::MenuItemType type =
                static_cast<QQuickMenuItemType::MenuItemType>(m_properties["type"].toInt());
        if (type == QQuickMenuItemType::ScrollIndicator) {
            int scrollerDirection = m_properties["scrollerDirection"].toInt();
            opt->menuItemType = QStyleOptionMenuItem::Scroller;
            opt->state |= scrollerDirection == Qt::UpArrow ?
                        QStyle::State_UpArrow : QStyle::State_DownArrow;
        } else if (type == QQuickMenuItemType::Separator) {
            opt->menuItemType = QStyleOptionMenuItem::Separator;
        } else {
            opt->text = text();

            if (type == QQuickMenuItemType::Menu) {
                opt->menuItemType = QStyleOptionMenuItem::SubMenu;
            } else {
                opt->menuItemType = QStyleOptionMenuItem::Normal;

                QString shortcut = m_properties["shortcut"].toString();
                if (!shortcut.isEmpty()) {
                    opt->text += QLatin1Char('\t') + shortcut;
                    opt->tabWidth = qMax(opt->tabWidth, qRound(textWidth(shortcut)));
                }

                if (m_properties["checkable"].toBool()) {
                    opt->checked = on();
                    QVariant exclusive = m_properties["exclusive"];
                    opt->checkType = exclusive.toBool() ? QStyleOptionMenuItem::Exclusive :
                                                          QStyleOptionMenuItem::NonExclusive;
                }
            }
            if (m_properties["icon"].canConvert<QIcon>())
                opt->icon = m_properties["icon"].value<QIcon>();
            setProperty("_q_showUnderlined", m_hints["showUnderlined"].toBool());

            if (const QFont *font = QGuiApplicationPrivate::platformTheme()->font(m_itemType == ComboBoxItem ? QPlatformTheme::ComboMenuItemFont : QPlatformTheme::MenuFont)) {
                opt->font = *font;
                opt->fontMetrics = QFontMetrics(opt->font);
                m_font = opt->font;
            }
        }
    }
        break;
    case CheckBox:
    case RadioButton:
    {
        if (!m_styleoption)
            m_styleoption = new QStyleOptionButton();

        QStyleOptionButton *opt = qstyleoption_cast<QStyleOptionButton*>(m_styleoption);
        if (!on())
            opt->state |= QStyle::State_Off;
        if (m_properties.value("partiallyChecked").toBool())
            opt->state |= QStyle::State_NoChange;
        opt->text = text();
    }
        break;
    case Edit: {
        if (!m_styleoption)
            m_styleoption = new QStyleOptionFrame();

        QStyleOptionFrame *opt = qstyleoption_cast<QStyleOptionFrame*>(m_styleoption);
        opt->lineWidth = 1; // this must be non-zero
    }
        break;
    case ComboBox :{
        if (!m_styleoption)
            m_styleoption = new QStyleOptionComboBox();

        QStyleOptionComboBox *opt = qstyleoption_cast<QStyleOptionComboBox*>(m_styleoption);

        if (platformFont == QPlatformTheme::SystemFont)
            platformFont = QPlatformTheme::PushButtonFont;
        const QFont *font = QGuiApplicationPrivate::platformTheme()->font(platformFont);
        if (font)
            opt->fontMetrics = QFontMetrics(*font);
        opt->currentText = text();
        opt->editable = m_properties["editable"].toBool();
#ifdef Q_OS_OSX
        if (m_properties["popup"].canConvert<QObject *>() && style() == "mac") {
            QObject *popup = m_properties["popup"].value<QObject *>();
            if (platformFont == QPlatformTheme::MiniFont) {
                popup->setProperty("__xOffset", -2);
                popup->setProperty("__yOffset", 5);
            } else {
                if (platformFont == QPlatformTheme::SmallFont)
                    popup->setProperty("__xOffset", -1);
                popup->setProperty("__yOffset", 6);
            }
            if (font)
                popup->setProperty("__font", *font);
        }
#endif
    }
        break;
    case SpinBox: {
        if (!m_styleoption)
            m_styleoption = new QStyleOptionSpinBox();

        QStyleOptionSpinBox *opt = qstyleoption_cast<QStyleOptionSpinBox*>(m_styleoption);
        opt->frame = true;
        opt->subControls = QStyle::SC_SpinBoxFrame | QStyle::SC_SpinBoxEditField;
        if (value() & 0x1)
            opt->activeSubControls = QStyle::SC_SpinBoxUp;
        else if (value() & (1<<1))
            opt->activeSubControls = QStyle::SC_SpinBoxDown;
        opt->subControls = QStyle::SC_All;
        opt->stepEnabled = 0;
        if (value() & (1<<2))
            opt->stepEnabled |= QAbstractSpinBox::StepUpEnabled;
        if (value() & (1<<3))
            opt->stepEnabled |= QAbstractSpinBox::StepDownEnabled;
    }
        break;
    case Slider:
    case Dial:
    {
        if (!m_styleoption)
            m_styleoption = new QStyleOptionSlider();

        QStyleOptionSlider *opt = qstyleoption_cast<QStyleOptionSlider*>(m_styleoption);
        opt->orientation = horizontal() ? Qt::Horizontal : Qt::Vertical;
        opt->upsideDown = !horizontal();
        opt->minimum = minimum();
        opt->maximum = maximum();
        opt->sliderPosition = value();
        opt->singleStep = step();

        if (opt->singleStep) {
            qreal numOfSteps = (opt->maximum - opt->minimum) / opt->singleStep;
            // at least 5 pixels between tick marks
            if (numOfSteps && (width() / numOfSteps < 5))
                opt->tickInterval = qRound((5 * numOfSteps / width()) + 0.5) * step();
            else
                opt->tickInterval = opt->singleStep;

        } else // default Qt-components implementation
            opt->tickInterval = opt->maximum != opt->minimum ? 1200 / (opt->maximum - opt->minimum) : 0;

        opt->sliderValue = value();
        opt->subControls = QStyle::SC_SliderGroove | QStyle::SC_SliderHandle;
        opt->tickPosition = (activeControl() == "ticks" ?
                    QSlider::TicksBelow : QSlider::NoTicks);
        if (opt->tickPosition != QSlider::NoTicks)
            opt->subControls |= QStyle::SC_SliderTickmarks;

        opt->activeSubControls = QStyle::SC_SliderHandle;
    }
        break;
    case ProgressBar: {
        if (!m_styleoption)
            m_styleoption = new QStyleOptionProgressBar();

        QStyleOptionProgressBar *opt = qstyleoption_cast<QStyleOptionProgressBar*>(m_styleoption);
        opt->orientation = horizontal() ? Qt::Horizontal : Qt::Vertical;
        opt->minimum = minimum();
        opt->maximum = maximum();
        opt->progress = value();
    }
        break;
    case GroupBox: {
        if (!m_styleoption)
            m_styleoption = new QStyleOptionGroupBox();

        QStyleOptionGroupBox *opt = qstyleoption_cast<QStyleOptionGroupBox*>(m_styleoption);
        opt->text = text();
        opt->lineWidth = 1;
        opt->subControls = QStyle::SC_GroupBoxLabel;
        opt->features = 0;
        if (m_properties["sunken"].toBool()) { // Qt draws an ugly line here so I ignore it
            opt->subControls |= QStyle::SC_GroupBoxFrame;
        } else {
            opt->features |= QStyleOptionFrame::Flat;
        }
        if (m_properties["checkable"].toBool())
            opt->subControls |= QStyle::SC_GroupBoxCheckBox;

    }
        break;
    case ScrollBar: {
        if (!m_styleoption)
            m_styleoption = new QStyleOptionSlider();

        QStyleOptionSlider *opt = qstyleoption_cast<QStyleOptionSlider*>(m_styleoption);
        opt->minimum = minimum();
        opt->maximum = maximum();
        opt->pageStep = qMax(0, int(horizontal() ? width() : height()));
        opt->orientation = horizontal() ? Qt::Horizontal : Qt::Vertical;
        opt->sliderPosition = value();
        opt->sliderValue = value();
        opt->activeSubControls = (activeControl() == QLatin1String("up"))
                ? QStyle::SC_ScrollBarSubLine : (activeControl() == QLatin1String("down")) ?
                      QStyle::SC_ScrollBarAddLine :
                  (activeControl() == QLatin1String("handle")) ?
                      QStyle::SC_ScrollBarSlider : hover() ? QStyle::SC_ScrollBarGroove : QStyle::SC_None;
        if (raised())
            opt->state |= QStyle::State_On;

        opt->sliderValue = value();
        opt->subControls = QStyle::SC_All;

        setTransient(qApp->style()->styleHint(QStyle::SH_ScrollBar_Transient, m_styleoption));
        break;
    }
    default:
        break;
    }

    if (!m_styleoption)
        m_styleoption = new QStyleOption();

    if (needsResolvePalette)
        resolvePalette();

    m_styleoption->styleObject = this;
    m_styleoption->direction = qApp->layoutDirection();

    int w = m_textureWidth > 0 ? m_textureWidth : width();
    int h = m_textureHeight > 0 ? m_textureHeight : height();

    m_styleoption->rect = QRect(m_paintMargins, 0, w - 2* m_paintMargins, h);

    if (isEnabled()) {
        m_styleoption->state |= QStyle::State_Enabled;
        m_styleoption->palette.setCurrentColorGroup(QPalette::Active);
    } else {
        m_styleoption->palette.setCurrentColorGroup(QPalette::Disabled);
    }
    if (m_active)
        m_styleoption->state |= QStyle::State_Active;
    else
        m_styleoption->palette.setCurrentColorGroup(QPalette::Inactive);
    if (m_sunken)
        m_styleoption->state |= QStyle::State_Sunken;
    if (m_raised)
        m_styleoption->state |= QStyle::State_Raised;
    if (m_selected)
        m_styleoption->state |= QStyle::State_Selected;
    if (m_focus)
        m_styleoption->state |= QStyle::State_HasFocus;
    if (m_on)
        m_styleoption->state |= QStyle::State_On;
    if (m_hover)
        m_styleoption->state |= QStyle::State_MouseOver;
    if (m_horizontal)
        m_styleoption->state |= QStyle::State_Horizontal;

    // some styles don't draw a focus rectangle if
    // QStyle::State_KeyboardFocusChange is not set
    if (window()) {
         Qt::FocusReason lastFocusReason = QQuickWindowPrivate::get(window())->lastFocusReason;
         if (lastFocusReason == Qt::TabFocusReason || lastFocusReason == Qt::BacktabFocusReason) {
             m_styleoption->state |= QStyle::State_KeyboardFocusChange;
         }
    }

    if (sizeHint == "mini") {
        m_styleoption->state |= QStyle::State_Mini;
    } else if (sizeHint == "small") {
        m_styleoption->state |= QStyle::State_Small;
    }

}

void QQuickStyleItem::resolvePalette()
{
    QPlatformTheme::Palette paletteType = QPlatformTheme::SystemPalette;
    switch (m_itemType) {
    case Button:
        paletteType = QPlatformTheme::ButtonPalette;
        break;
    case RadioButton:
        paletteType = QPlatformTheme::RadioButtonPalette;
        break;
    case CheckBox:
        paletteType = QPlatformTheme::CheckBoxPalette;
        break;
    case ComboBox:
    case ComboBoxItem:
        paletteType = QPlatformTheme::ComboBoxPalette;
        break;
    case ToolBar:
    case ToolButton:
        paletteType = QPlatformTheme::ToolButtonPalette;
        break;
    case Tab:
    case TabFrame:
        paletteType = QPlatformTheme::TabBarPalette;
        break;
    case Edit:
        paletteType = QPlatformTheme::TextEditPalette;
        break;
    case GroupBox:
        paletteType = QPlatformTheme::GroupBoxPalette;
        break;
    case Header:
        paletteType = QPlatformTheme::HeaderPalette;
        break;
    case Item:
    case ItemRow:
        paletteType = QPlatformTheme::ItemViewPalette;
        break;
    case Menu:
    case MenuItem:
        paletteType = QPlatformTheme::MenuPalette;
        break;
    case MenuBar:
    case MenuBarItem:
        paletteType = QPlatformTheme::MenuBarPalette;
        break;
    default:
        break;
    }

    const QPalette *platformPalette = QGuiApplicationPrivate::platformTheme()->palette(paletteType);
    if (platformPalette)
        m_styleoption->palette = *platformPalette;
    // Defaults to SystemPalette otherwise
}

/*
 *   Property style
 *
 *   Returns a simplified style name.
 *
 *   QMacStyle = "mac"
 *   QWindowsXPStyle = "windowsxp"
 *   QFusionStyle = "fusion"
 */

QString QQuickStyleItem::style() const
{
    QString style = qApp->style()->metaObject()->className();
    style = style.toLower();
    if (style.startsWith(QLatin1Char('q')))
        style = style.right(style.length() - 1);
    if (style.endsWith("style"))
        style = style.left(style.length() - 5);
    return style;
}

QString QQuickStyleItem::hitTest(int px, int py)
{
    QStyle::SubControl subcontrol = QStyle::SC_All;
    switch (m_itemType) {
    case SpinBox :{
        subcontrol = qApp->style()->hitTestComplexControl(QStyle::CC_SpinBox,
                                                          qstyleoption_cast<QStyleOptionComplex*>(m_styleoption),
                                                          QPoint(px,py), 0);
        if (subcontrol == QStyle::SC_SpinBoxUp)
            return "up";
        else if (subcontrol == QStyle::SC_SpinBoxDown)
            return "down";

    }
        break;

    case Slider: {
        subcontrol = qApp->style()->hitTestComplexControl(QStyle::CC_Slider,
                                                          qstyleoption_cast<QStyleOptionComplex*>(m_styleoption),
                                                          QPoint(px,py), 0);
        if (subcontrol == QStyle::SC_SliderHandle)
            return "handle";

    }
        break;
    case ScrollBar: {
        subcontrol = qApp->style()->hitTestComplexControl(QStyle::CC_ScrollBar,
                                                          qstyleoption_cast<QStyleOptionComplex*>(m_styleoption),
                                                          QPoint(px,py), 0);
        if (subcontrol == QStyle::SC_ScrollBarSlider)
            return "handle";

        if (subcontrol == QStyle::SC_ScrollBarSubLine)
            return "up";
        else if (subcontrol == QStyle::SC_ScrollBarSubPage)
            return "upPage";

        if (subcontrol == QStyle::SC_ScrollBarAddLine)
            return "down";
        else if (subcontrol == QStyle::SC_ScrollBarAddPage)
            return "downPage";
    }
        break;
    default:
        break;
    }
    return "none";
}

QSize QQuickStyleItem::sizeFromContents(int width, int height)
{
    initStyleOption();

    QSize size;
    switch (m_itemType) {
    case RadioButton:
        size =  qApp->style()->sizeFromContents(QStyle::CT_RadioButton, m_styleoption, QSize(width,height));
        break;
    case CheckBox:
        size =  qApp->style()->sizeFromContents(QStyle::CT_CheckBox, m_styleoption, QSize(width,height));
        break;
    case ToolBar:
        size = QSize(200, style().contains("windows") ? 30 : 42);
        break;
    case ToolButton: {
        QStyleOptionToolButton *btn = qstyleoption_cast<QStyleOptionToolButton*>(m_styleoption);
        int w = 0;
        int h = 0;
        if (btn->toolButtonStyle != Qt::ToolButtonTextOnly) {
            QSize icon = btn->iconSize;
            w = icon.width();
            h = icon.height();
        }
        if (btn->toolButtonStyle != Qt::ToolButtonIconOnly) {
            QSize textSize = btn->fontMetrics.size(Qt::TextShowMnemonic, btn->text);
            textSize.setWidth(textSize.width() + btn->fontMetrics.width(QLatin1Char(' '))*2);
            if (btn->toolButtonStyle == Qt::ToolButtonTextUnderIcon) {
                h += 4 + textSize.height();
                if (textSize.width() > w)
                    w = textSize.width();
            } else if (btn->toolButtonStyle == Qt::ToolButtonTextBesideIcon) {
                w += 4 + textSize.width();
                if (textSize.height() > h)
                    h = textSize.height();
            } else { // TextOnly
                w = textSize.width();
                h = textSize.height();
            }
        }
        btn->rect.setSize(QSize(w, h));
        size = qApp->style()->sizeFromContents(QStyle::CT_ToolButton, m_styleoption, QSize(w, h)); }
        break;
    case Button: {
        QStyleOptionButton *btn = qstyleoption_cast<QStyleOptionButton*>(m_styleoption);

        int contentWidth = btn->fontMetrics.width(btn->text);
        int contentHeight = btn->fontMetrics.height();
        if (!btn->icon.isNull()) {
            //+4 matches a hardcoded value in QStyle and acts as a margin between the icon and the text.
            contentWidth += btn->iconSize.width() + 4;
            contentHeight = qMax(btn->fontMetrics.height(), btn->iconSize.height());
        }
        int newWidth = qMax(width, contentWidth);
        int newHeight = qMax(height, contentHeight);
        size = qApp->style()->sizeFromContents(QStyle::CT_PushButton, m_styleoption, QSize(newWidth, newHeight)); }
#ifdef Q_OS_OSX
        if (style() == "mac") {
            // Cancel out QMacStylePrivate::PushButton*Offset, or part of it
            size -= QSize(7, 6);
        }
#endif
        break;
    case ComboBox: {
        QStyleOptionComboBox *btn = qstyleoption_cast<QStyleOptionComboBox*>(m_styleoption);
        int newWidth = qMax(width, btn->fontMetrics.width(btn->currentText));
        int newHeight = qMax(height, btn->fontMetrics.height());
        size = qApp->style()->sizeFromContents(QStyle::CT_ComboBox, m_styleoption, QSize(newWidth, newHeight)); }
        break;
    case Tab:
        size = qApp->style()->sizeFromContents(QStyle::CT_TabBarTab, m_styleoption, QSize(width,height));
        break;
    case Slider:
        size = qApp->style()->sizeFromContents(QStyle::CT_Slider, m_styleoption, QSize(width,height));
        break;
    case ProgressBar:
        size = qApp->style()->sizeFromContents(QStyle::CT_ProgressBar, m_styleoption, QSize(width,height));
        break;
    case SpinBox:
#ifdef Q_OS_OSX
        if (style() == "mac") {
            size = qApp->style()->sizeFromContents(QStyle::CT_SpinBox, m_styleoption, QSize(width, height + 5));
            break;
        }
#endif // fall through if not mac
    case Edit:
#ifdef Q_OS_OSX
        if (style() =="mac") {
            QString sizeHint = m_hints.value("size").toString();
            if ((sizeHint == "small") || (sizeHint == "mini"))
                size = QSize(width, 19);
            else
                size = QSize(width, 22);
            if (style() == "mac" && hints().value("rounded").toBool())
                size += QSize(4, 4);

        } else
#endif
        {
            // We have to create a new style option since we might be calling with a QStyleOptionSpinBox
            QStyleOptionFrame frame;
            frame.state = m_styleoption->state;
            frame.lineWidth = qApp->style()->pixelMetric(QStyle::PM_DefaultFrameWidth, m_styleoption, 0);
            frame.rect = m_styleoption->rect;
            frame.styleObject = this;
            size = qApp->style()->sizeFromContents(QStyle::CT_LineEdit, &frame, QSize(width, height));
            if (m_itemType == SpinBox)
                size.setWidth(qApp->style()->sizeFromContents(QStyle::CT_SpinBox,
                                                              m_styleoption, QSize(width + 2, height)).width());
        }
        break;
    case GroupBox: {
            QStyleOptionGroupBox *box = qstyleoption_cast<QStyleOptionGroupBox*>(m_styleoption);
            QFontMetrics metrics(box->fontMetrics);
            int baseWidth = metrics.width(box->text) + metrics.width(QLatin1Char(' '));
            int baseHeight = metrics.height() + m_contentHeight;
            if (box->subControls & QStyle::SC_GroupBoxCheckBox) {
                baseWidth += qApp->style()->pixelMetric(QStyle::PM_IndicatorWidth);
                baseWidth += qApp->style()->pixelMetric(QStyle::PM_CheckBoxLabelSpacing);
                baseHeight = qMax(baseHeight, qApp->style()->pixelMetric(QStyle::PM_IndicatorHeight));
            }
            size = qApp->style()->sizeFromContents(QStyle::CT_GroupBox, m_styleoption, QSize(qMax(baseWidth, m_contentWidth), baseHeight));
        }
        break;
    case Header:
        size = qApp->style()->sizeFromContents(QStyle::CT_HeaderSection, m_styleoption, QSize(width,height));
#ifdef Q_OS_OSX
        if (style() =="mac")
            size.setHeight(15);
#endif
        break;
    case ItemRow:
    case Item: //fall through
        size = qApp->style()->sizeFromContents(QStyle::CT_ItemViewItem, m_styleoption, QSize(width,height));
        break;
    case MenuBarItem:
        size = qApp->style()->sizeFromContents(QStyle::CT_MenuBarItem, m_styleoption, QSize(width,height));
        break;
    case MenuBar:
        size = qApp->style()->sizeFromContents(QStyle::CT_MenuBar, m_styleoption, QSize(width,height));
        break;
    case Menu:
        size = qApp->style()->sizeFromContents(QStyle::CT_Menu, m_styleoption, QSize(width,height));
        break;
    case MenuItem:
    case ComboBoxItem:
        if (static_cast<QStyleOptionMenuItem *>(m_styleoption)->menuItemType == QStyleOptionMenuItem::Scroller) {
            size.setHeight(qMax(QApplication::globalStrut().height(),
                                qApp->style()->pixelMetric(QStyle::PM_MenuScrollerHeight, 0, 0)));
        } else {
            size = qApp->style()->sizeFromContents(QStyle::CT_MenuItem, m_styleoption, QSize(width,height));
        }
        break;
    default:
        break;
    }    return size;
}

qreal QQuickStyleItem::baselineOffset()
{
    QRect r;
    bool ceilResult = true; // By default baseline offset rounding is done upwards
    switch (m_itemType) {
    case RadioButton:
        r = qApp->style()->subElementRect(QStyle::SE_RadioButtonContents, m_styleoption);
        break;
    case Button:
        r = qApp->style()->subElementRect(QStyle::SE_PushButtonContents, m_styleoption);
        break;
    case CheckBox:
        r = qApp->style()->subElementRect(QStyle::SE_CheckBoxContents, m_styleoption);
        break;
    case Edit:
        r = qApp->style()->subElementRect(QStyle::SE_LineEditContents, m_styleoption);
        break;
    case ComboBox:
        if (const QStyleOptionComboBox *combo = qstyleoption_cast<const QStyleOptionComboBox *>(m_styleoption)) {
            r = qApp->style()->subControlRect(QStyle::CC_ComboBox, combo, QStyle::SC_ComboBoxEditField);
            if (style() != QStringLiteral("mac"))
                r.adjust(0,0,0,1);
        }
        break;
    case SpinBox:
        if (const QStyleOptionSpinBox *spinbox = qstyleoption_cast<const QStyleOptionSpinBox *>(m_styleoption)) {
            r = qApp->style()->subControlRect(QStyle::CC_SpinBox, spinbox, QStyle::SC_SpinBoxEditField);
            ceilResult = false;
        }
        break;
    default:
        break;
    }
    if (r.height() > 0) {
        const QFontMetrics &fm = m_styleoption->fontMetrics;
        int surplus = r.height() - fm.height();
        if ((surplus & 1) && ceilResult)
            surplus++;
        int result = r.top() + surplus/2 + fm.ascent();
#ifdef Q_OS_OSX
        if (style() == QStringLiteral("mac")) {
            switch (m_itemType) {
            case Button:
            case Edit:
                result -= 1;
                break;
            case ComboBox:
                // adjust back the adjustments done in drawControl(CE_ComboBoxLabel)
                result += 1;
                break;
            default:
                break;
            }
        }
#endif
        return result;
    }

    return 0.;
}

void QQuickStyleItem::updateBaselineOffset()
{
    const qreal baseline = baselineOffset();
    if (baseline > 0)
        setBaselineOffset(baseline);
}

void QQuickStyleItem::setContentWidth(int arg)
{
    if (m_contentWidth != arg) {
        m_contentWidth = arg;
        emit contentWidthChanged(arg);
    }
}

void QQuickStyleItem::setContentHeight(int arg)
{
    if (m_contentHeight != arg) {
        m_contentHeight = arg;
        emit contentHeightChanged(arg);
    }
}

void QQuickStyleItem::updateSizeHint()
{
    QSize implicitSize = sizeFromContents(m_contentWidth, m_contentHeight);
    setImplicitSize(implicitSize.width(), implicitSize.height());
}

void QQuickStyleItem::updateRect()
{
    initStyleOption();
    m_styleoption->rect.setWidth(width());
    m_styleoption->rect.setHeight(height());
}

int QQuickStyleItem::pixelMetric(const QString &metric)
{

    if (metric == "scrollbarExtent")
        return qApp->style()->pixelMetric(QStyle::PM_ScrollBarExtent, 0 );
    else if (metric == "defaultframewidth")
        return qApp->style()->pixelMetric(QStyle::PM_DefaultFrameWidth, m_styleoption);
    else if (metric == "taboverlap")
        return qApp->style()->pixelMetric(QStyle::PM_TabBarTabOverlap, 0 );
    else if (metric == "tabbaseoverlap")
        return qApp->style()->pixelMetric(QStyle::PM_TabBarBaseOverlap, m_styleoption );
    else if (metric == "tabhspace")
        return qApp->style()->pixelMetric(QStyle::PM_TabBarTabHSpace, 0 );
    else if (metric == "indicatorwidth")
        return qApp->style()->pixelMetric(QStyle::PM_ExclusiveIndicatorWidth, 0 );
    else if (metric == "tabvspace")
        return qApp->style()->pixelMetric(QStyle::PM_TabBarTabVSpace, 0 );
    else if (metric == "tabbaseheight")
        return qApp->style()->pixelMetric(QStyle::PM_TabBarBaseHeight, 0 );
    else if (metric == "tabvshift")
        return qApp->style()->pixelMetric(QStyle::PM_TabBarTabShiftVertical, 0 );
    else if (metric == "menubarhmargin")
        return qApp->style()->pixelMetric(QStyle::PM_MenuBarHMargin, 0 );
    else if (metric == "menubarvmargin")
        return qApp->style()->pixelMetric(QStyle::PM_MenuBarVMargin, 0 );
    else if (metric == "menubarpanelwidth")
        return qApp->style()->pixelMetric(QStyle::PM_MenuBarPanelWidth, 0 );
    else if (metric == "menubaritemspacing")
        return qApp->style()->pixelMetric(QStyle::PM_MenuBarItemSpacing, 0 );
    else if (metric == "spacebelowmenubar")
        return qApp->style()->styleHint(QStyle::SH_MainWindow_SpaceBelowMenuBar, m_styleoption);
    else if (metric == "menuhmargin")
        return qApp->style()->pixelMetric(QStyle::PM_MenuHMargin, 0 );
    else if (metric == "menuvmargin")
        return qApp->style()->pixelMetric(QStyle::PM_MenuVMargin, 0 );
    else if (metric == "menupanelwidth")
        return qApp->style()->pixelMetric(QStyle::PM_MenuPanelWidth, 0 );
    else if (metric == "submenuoverlap")
        return qApp->style()->pixelMetric(QStyle::PM_SubMenuOverlap, 0 );
    else if (metric == "splitterwidth")
        return qApp->style()->pixelMetric(QStyle::PM_SplitterWidth, 0 );
    else if (metric == "scrollbarspacing")
        return abs(qApp->style()->pixelMetric(QStyle::PM_ScrollView_ScrollBarSpacing, 0 ));
    return 0;
}

QVariant QQuickStyleItem::styleHint(const QString &metric)
{
    initStyleOption();
    if (metric == "comboboxpopup") {
        return qApp->style()->styleHint(QStyle::SH_ComboBox_Popup, m_styleoption);
    } else if (metric == "highlightedTextColor") {
        return m_styleoption->palette.highlightedText().color().name();
    } else if (metric == "textColor") {
        QPalette pal = m_styleoption->palette;
        pal.setCurrentColorGroup(active()? QPalette::Active : QPalette::Inactive);
        return pal.text().color().name();
    } else if (metric == "focuswidget") {
        return qApp->style()->styleHint(QStyle::SH_FocusFrame_AboveWidget);
    } else if (metric == "tabbaralignment") {
        int result = qApp->style()->styleHint(QStyle::SH_TabBar_Alignment);
        if (result == Qt::AlignCenter)
            return "center";
        return "left";
    } else if (metric == "externalScrollBars") {
        return qApp->style()->styleHint(QStyle::SH_ScrollView_FrameOnlyAroundContents);
    } else if (metric == "scrollToClickPosition")
        return qApp->style()->styleHint(QStyle::SH_ScrollBar_LeftClickAbsolutePosition);
    else if (metric == "activateItemOnSingleClick")
        return qApp->style()->styleHint(QStyle::SH_ItemView_ActivateItemOnSingleClick);
    else if (metric == "submenupopupdelay")
        return qApp->style()->styleHint(QStyle::SH_Menu_SubMenuPopupDelay, m_styleoption);
    return 0;

    // Add SH_Menu_SpaceActivatesItem
}

void QQuickStyleItem::setHints(const QVariantMap &str)
{
    if (m_hints != str) {
        m_hints = str;
        initStyleOption();
        updateSizeHint();
        if (m_styleoption->state & QStyle::State_Mini) {
            m_font.setPointSize(9.);
            emit fontChanged();
        } else if (m_styleoption->state & QStyle::State_Small) {
            m_font.setPointSize(11.);
            emit fontChanged();
        } else {
            emit hintChanged();
        }
    }
}

void QQuickStyleItem::resetHints()
{
    m_hints.clear();
}


void QQuickStyleItem::setElementType(const QString &str)
{
    if (m_type == str)
        return;

    m_type = str;

    emit elementTypeChanged();
    if (m_styleoption) {
        delete m_styleoption;
        m_styleoption = 0;
    }

    // Only enable visible if the widget can animate
    if (str == "menu") {
        m_itemType = Menu;
    } else if (str == "menuitem") {
        m_itemType = MenuItem;
    } else if (str == "item" || str == "itemrow" || str == "header") {
#ifdef Q_OS_OSX
        m_font.setPointSize(11.0);
        emit fontChanged();
#endif
        if (str == "header") {
            m_itemType = Header;
        } else {
            m_itemType = (str == "item") ? Item : ItemRow;
        }
    } else if (str == "groupbox") {
        m_itemType = GroupBox;
    } else if (str == "tab") {
        m_itemType = Tab;
    } else if (str == "tabframe") {
        m_itemType = TabFrame;
    } else if (str == "comboboxitem")  {
        // Gtk uses qobject cast, hence we need to separate this from menuitem
        // On mac, we temporarily use the menu item because it has more accurate
        // palette.
        m_itemType = ComboBoxItem;
    } else if (str == "toolbar") {
        m_itemType = ToolBar;
    } else if (str == "toolbutton") {
        m_itemType = ToolButton;
    } else if (str == "slider") {
        m_itemType = Slider;
    } else if (str == "frame") {
        m_itemType = Frame;
    } else if (str == "combobox") {
        m_itemType = ComboBox;
    } else if (str == "splitter") {
        m_itemType = Splitter;
    } else if (str == "progressbar") {
        m_itemType = ProgressBar;
    } else if (str == "button") {
        m_itemType = Button;
    } else if (str == "checkbox") {
        m_itemType = CheckBox;
    } else if (str == "radiobutton") {
        m_itemType = RadioButton;
    } else if (str == "edit") {
        m_itemType = Edit;
    } else if (str == "spinbox") {
        m_itemType = SpinBox;
    } else if (str == "scrollbar") {
        m_itemType = ScrollBar;
    } else if (str == "widget") {
        m_itemType = Widget;
    } else if (str == "focusframe") {
        m_itemType = FocusFrame;
    } else if (str == "focusrect") {
        m_itemType = FocusRect;
    } else if (str == "dial") {
        m_itemType = Dial;
    } else if (str == "statusbar") {
        m_itemType = StatusBar;
    } else if (str == "machelpbutton") {
        m_itemType = MacHelpButton;
    } else if (str == "scrollareacorner") {
        m_itemType = ScrollAreaCorner;
    } else if (str == "menubar") {
        m_itemType = MenuBar;
    } else if (str == "menubaritem") {
        m_itemType = MenuBarItem;
    } else {
        m_itemType = Undefined;
    }
    updateSizeHint();
}

QRectF QQuickStyleItem::subControlRect(const QString &subcontrolString)
{
    QStyle::SubControl subcontrol = QStyle::SC_None;
    initStyleOption();
    switch (m_itemType) {
    case SpinBox:
    {
        QStyle::ComplexControl control = QStyle::CC_SpinBox;
        if (subcontrolString == QLatin1String("down"))
            subcontrol = QStyle::SC_SpinBoxDown;
        else if (subcontrolString == QLatin1String("up"))
            subcontrol = QStyle::SC_SpinBoxUp;
        else if (subcontrolString == QLatin1String("edit")){
            subcontrol = QStyle::SC_SpinBoxEditField;
        }
        return qApp->style()->subControlRect(control,
                                             qstyleoption_cast<QStyleOptionComplex*>(m_styleoption),
                                             subcontrol);

    }
        break;
    case Slider:
    {
        QStyle::ComplexControl control = QStyle::CC_Slider;
        if (subcontrolString == QLatin1String("handle"))
            subcontrol = QStyle::SC_SliderHandle;
        else if (subcontrolString == QLatin1String("groove"))
            subcontrol = QStyle::SC_SliderGroove;
        return qApp->style()->subControlRect(control,
                                             qstyleoption_cast<QStyleOptionComplex*>(m_styleoption),
                                             subcontrol);

    }
        break;
    case ScrollBar:
    {
        QStyle::ComplexControl control = QStyle::CC_ScrollBar;
        if (subcontrolString == QLatin1String("slider"))
            subcontrol = QStyle::SC_ScrollBarSlider;
        if (subcontrolString == QLatin1String("groove"))
            subcontrol = QStyle::SC_ScrollBarGroove;
        else if (subcontrolString == QLatin1String("handle"))
            subcontrol = QStyle::SC_ScrollBarSlider;
        else if (subcontrolString == QLatin1String("add"))
            subcontrol = QStyle::SC_ScrollBarAddPage;
        else if (subcontrolString == QLatin1String("sub"))
            subcontrol = QStyle::SC_ScrollBarSubPage;
        return qApp->style()->subControlRect(control,
                                             qstyleoption_cast<QStyleOptionComplex*>(m_styleoption),
                                             subcontrol);
    }
        break;
    default:
        break;
    }
    return QRectF();
}

namespace  {
class QHighDpiPixmapsEnabler {
public:
    QHighDpiPixmapsEnabler()
    :wasEnabled(false)
    {
        if (!qApp->testAttribute(Qt::AA_UseHighDpiPixmaps)) {
            qApp->setAttribute(Qt::AA_UseHighDpiPixmaps);
            wasEnabled = true;
        }
    }

    ~QHighDpiPixmapsEnabler()
    {
        if (wasEnabled)
            qApp->setAttribute(Qt::AA_UseHighDpiPixmaps, false);
    }
private:
    bool wasEnabled;
};
}

void QQuickStyleItem::paint(QPainter *painter)
{
    initStyleOption();
    if (QStyleOptionMenuItem *opt = qstyleoption_cast<QStyleOptionMenuItem*>(m_styleoption))
        painter->setFont(opt->font);
    else {
        QPlatformTheme::Font platformFont = (m_styleoption->state & QStyle::State_Mini) ? QPlatformTheme::MiniFont :
                                            (m_styleoption->state & QStyle::State_Small) ? QPlatformTheme::SmallFont :
                                            QPlatformTheme::NFonts;
        if (platformFont != QPlatformTheme::NFonts)
            if (const QFont *font = QGuiApplicationPrivate::platformTheme()->font(platformFont))
                painter->setFont(*font);
    }

    // Set AA_UseHighDpiPixmaps when calling style code to make QIcon return
    // "retina" pixmaps. The flag is controlled by the application so we can't
    // set it unconditinally.
    QHighDpiPixmapsEnabler enabler;

    switch (m_itemType) {
    case Button:
#ifdef Q_OS_OSX
        if (style() == "mac") {
            // Add back what was substracted in sizeFromContents()
            m_styleoption->rect.adjust(-4, -2, 3, 4);
        }
#endif
        qApp->style()->drawControl(QStyle::CE_PushButton, m_styleoption, painter);
        break;
    case ItemRow :{
        QPixmap pixmap;
        // Only draw through style once
        const QString pmKey = QLatin1Literal("itemrow") % QString::number(m_styleoption->state,16) % activeControl();
        if (!QPixmapCache::find(pmKey, pixmap) || pixmap.width() < width() || height() != pixmap.height()) {
            int newSize = width();
            pixmap = QPixmap(newSize, height());
            pixmap.fill(Qt::transparent);
            QPainter pixpainter(&pixmap);
            qApp->style()->drawPrimitive(QStyle::PE_PanelItemViewRow, m_styleoption, &pixpainter);
            if ((style() == "mac" || !qApp->style()->styleHint(QStyle::SH_ItemView_ShowDecorationSelected)) && selected()) {
                QPalette pal = QApplication::palette("QAbstractItemView");
                pal.setCurrentColorGroup(m_styleoption->palette.currentColorGroup());
                pixpainter.fillRect(m_styleoption->rect, pal.highlight());
            }
            QPixmapCache::insert(pmKey, pixmap);
        }
        painter->drawPixmap(0, 0, pixmap);
    }
        break;
    case Item:
        qApp->style()->drawControl(QStyle::CE_ItemViewItem, m_styleoption, painter);
        break;
    case Header:
        qApp->style()->drawControl(QStyle::CE_Header, m_styleoption, painter);
        break;
    case ToolButton:

#ifdef Q_OS_OSX
        if (style() == "mac" && hints().value("segmented").toBool()) {
            const QPaintDevice *target = painter->device();
             HIThemeSegmentDrawInfo sgi;
            sgi.version = 0;
            sgi.state = isEnabled() ? kThemeStateActive : kThemeStateDisabled;
            if (sunken()) sgi.state |= kThemeStatePressed;
            sgi.size = kHIThemeSegmentSizeNormal;
            sgi.kind = kHIThemeSegmentKindTextured;
            sgi.value = on() && !sunken() ? kThemeButtonOn : kThemeButtonOff;

            sgi.adornment |= kHIThemeSegmentAdornmentLeadingSeparator;
            if (sunken()) {
                sgi.adornment |= kHIThemeSegmentAdornmentTrailingSeparator;
            }
            SInt32 button_height;
            GetThemeMetric(kThemeMetricButtonRoundedHeight, &button_height);
            QString buttonPos = m_properties.value("position").toString();
            sgi.position = buttonPos == "leftmost" ? kHIThemeSegmentPositionFirst :
                           buttonPos == "rightmost" ? kHIThemeSegmentPositionLast :
                           buttonPos == "h_middle" ? kHIThemeSegmentPositionMiddle :
                           kHIThemeSegmentPositionOnly;
            QRect centered = m_styleoption->rect;
            centered.setHeight(button_height);
            centered.moveCenter(m_styleoption->rect.center());
            HIRect hirect = qt_hirectForQRect(centered.translated(0, -1), QRect(0, 0, 0, 0));
            HIThemeDrawSegment(&hirect, &sgi, qt_mac_cg_context(target), kHIThemeOrientationNormal);
        } else
#endif
        qApp->style()->drawComplexControl(QStyle::CC_ToolButton, qstyleoption_cast<QStyleOptionComplex*>(m_styleoption), painter);
        break;
    case Tab:
#ifdef Q_OS_OSX
        if (style() == "mac") {
            m_styleoption->rect.translate(0, 1); // Unhack QMacStyle's hack
            qApp->style()->drawControl(QStyle::CE_TabBarTabShape, m_styleoption, painter);
            m_styleoption->rect.translate(0, -1);
            qApp->style()->drawControl(QStyle::CE_TabBarTabLabel, m_styleoption, painter);
        } else
#endif
        {
            qApp->style()->drawControl(QStyle::CE_TabBarTab, m_styleoption, painter);
        }
        break;
    case Frame:
        qApp->style()->drawControl(QStyle::CE_ShapedFrame, m_styleoption, painter);
        break;
    case FocusFrame:
        qApp->style()->drawControl(QStyle::CE_FocusFrame, m_styleoption, painter);
        break;
    case FocusRect:
        qApp->style()->drawPrimitive(QStyle::PE_FrameFocusRect, m_styleoption, painter);
        break;
    case TabFrame:
        qApp->style()->drawPrimitive(QStyle::PE_FrameTabWidget, m_styleoption, painter);
        break;
    case MenuBar:
        qApp->style()->drawControl(QStyle::CE_MenuBarEmptyArea, m_styleoption, painter);
        break;
    case MenuBarItem:
        qApp->style()->drawControl(QStyle::CE_MenuBarItem, m_styleoption, painter);
        break;
    case MenuItem:
    case ComboBoxItem: { // fall through
        QStyle::ControlElement menuElement =
                static_cast<QStyleOptionMenuItem *>(m_styleoption)->menuItemType == QStyleOptionMenuItem::Scroller ?
                    QStyle::CE_MenuScroller : QStyle::CE_MenuItem;
        qApp->style()->drawControl(menuElement, m_styleoption, painter);
        }
        break;
    case CheckBox:
        qApp->style()->drawControl(QStyle::CE_CheckBox, m_styleoption, painter);
        break;
    case RadioButton:
        qApp->style()->drawControl(QStyle::CE_RadioButton, m_styleoption, painter);
        break;
    case Edit: {
#ifdef Q_OS_OSX
        if (style() == "mac" && hints().value("rounded").toBool()) {
            const QPaintDevice *target = painter->device();
            HIThemeFrameDrawInfo fdi;
            fdi.version = 0;
            fdi.state = kThemeStateActive;
            SInt32 frame_size;
            GetThemeMetric(kThemeMetricEditTextFrameOutset, &frame_size);
            fdi.kind = kHIThemeFrameTextFieldRound;
            if ((m_styleoption->state & QStyle::State_ReadOnly) || !(m_styleoption->state & QStyle::State_Enabled))
                fdi.state = kThemeStateInactive;
            fdi.isFocused = hasFocus();
            HIRect hirect = qt_hirectForQRect(m_styleoption->rect.adjusted(2, 2, -2, 2), QRect(0, 0, 0, 0));
            HIThemeDrawFrame(&hirect, &fdi, qt_mac_cg_context(target), kHIThemeOrientationNormal);
        } else
#endif
        qApp->style()->drawPrimitive(QStyle::PE_PanelLineEdit, m_styleoption, painter);
    }
        break;
    case MacHelpButton:
#ifdef Q_OS_OSX
    {
        const QPaintDevice *target = painter->device();
        HIThemeButtonDrawInfo fdi;
        fdi.kind = kThemeRoundButtonHelp;
        fdi.version = 0;
        fdi.adornment = 0;
        fdi.state = sunken() ? kThemeStatePressed : kThemeStateActive;
        HIRect hirect = qt_hirectForQRect(m_styleoption->rect,QRect(0, 0, 0, 0));
        HIThemeDrawButton(&hirect, &fdi, qt_mac_cg_context(target), kHIThemeOrientationNormal, NULL);
    }
#endif
        break;
    case Widget:
        qApp->style()->drawPrimitive(QStyle::PE_Widget, m_styleoption, painter);
        break;
    case ScrollAreaCorner:
        qApp->style()->drawPrimitive(QStyle::PE_PanelScrollAreaCorner, m_styleoption, painter);
        break;
    case Splitter:
        if (m_styleoption->rect.width() == 1)
            painter->fillRect(0, 0, width(), height(), m_styleoption->palette.dark().color());
        else
            qApp->style()->drawControl(QStyle::CE_Splitter, m_styleoption, painter);
        break;
    case ComboBox:
    {
        qApp->style()->drawComplexControl(QStyle::CC_ComboBox,
                                          qstyleoption_cast<QStyleOptionComplex*>(m_styleoption),
                                          painter);
        // This is needed on mac as it will use the painter color and ignore the palette
        QPen pen = painter->pen();
        painter->setPen(m_styleoption->palette.text().color());
        qApp->style()->drawControl(QStyle::CE_ComboBoxLabel, m_styleoption, painter);
        painter->setPen(pen);
    }    break;
    case SpinBox:
#ifdef Q_OS_MAC
        // macstyle depends on the embedded qlineedit to fill the editfield background
        if (style() == "mac") {
            QRect editRect = qApp->style()->subControlRect(QStyle::CC_SpinBox,
                                                           qstyleoption_cast<QStyleOptionComplex*>(m_styleoption),
                                                           QStyle::SC_SpinBoxEditField);
            painter->fillRect(editRect.adjusted(-1, -1, 1, 1), m_styleoption->palette.base());
        }
#endif
        qApp->style()->drawComplexControl(QStyle::CC_SpinBox,
                                          qstyleoption_cast<QStyleOptionComplex*>(m_styleoption),
                                          painter);
        break;
    case Slider:
        qApp->style()->drawComplexControl(QStyle::CC_Slider,
                                          qstyleoption_cast<QStyleOptionComplex*>(m_styleoption),
                                          painter);
        break;
    case Dial:
        qApp->style()->drawComplexControl(QStyle::CC_Dial,
                                          qstyleoption_cast<QStyleOptionComplex*>(m_styleoption),
                                          painter);
        break;
    case ProgressBar:
        qApp->style()->drawControl(QStyle::CE_ProgressBar, m_styleoption, painter);
        break;
    case ToolBar:
        painter->fillRect(m_styleoption->rect, m_styleoption->palette.window().color());
        qApp->style()->drawControl(QStyle::CE_ToolBar, m_styleoption, painter);
        painter->save();
        painter->setPen(style() != "fusion" ? m_styleoption->palette.dark().color().darker(120) :
                                              m_styleoption->palette.window().color().lighter(107));
        painter->drawLine(m_styleoption->rect.bottomLeft(), m_styleoption->rect.bottomRight());
        painter->restore();
        break;
    case StatusBar:
#ifdef Q_OS_OSX
        if (style() == "mac") {
            qApp->style()->drawControl(QStyle::CE_ToolBar, m_styleoption, painter);
            painter->setPen(m_styleoption->palette.dark().color().darker(120));
            painter->drawLine(m_styleoption->rect.topLeft(), m_styleoption->rect.topRight());
        } else
#endif
        {
            painter->fillRect(m_styleoption->rect, m_styleoption->palette.window().color());
            painter->setPen(m_styleoption->palette.dark().color().darker(120));
            painter->drawLine(m_styleoption->rect.topLeft(), m_styleoption->rect.topRight());
            qApp->style()->drawPrimitive(QStyle::PE_PanelStatusBar, m_styleoption, painter);
        }
        break;
    case GroupBox:
        qApp->style()->drawComplexControl(QStyle::CC_GroupBox, qstyleoption_cast<QStyleOptionComplex*>(m_styleoption), painter);
        break;
    case ScrollBar:
        qApp->style()->drawComplexControl(QStyle::CC_ScrollBar, qstyleoption_cast<QStyleOptionComplex*>(m_styleoption), painter);
        break;
    case Menu: {
        QStyleHintReturnMask val;
        qApp->style()->styleHint(QStyle::SH_Menu_Mask, m_styleoption, 0, &val);
        painter->save();
        painter->setClipRegion(val.region);
        painter->fillRect(m_styleoption->rect, m_styleoption->palette.window());
        painter->restore();
        qApp->style()->drawPrimitive(QStyle::PE_PanelMenu, m_styleoption, painter);

        if (int fw = qApp->style()->pixelMetric(QStyle::PM_MenuPanelWidth)) {
            QStyleOptionFrame frame;
            frame.state = QStyle::State_None;
            frame.lineWidth = fw;
            frame.midLineWidth = 0;
            frame.rect = m_styleoption->rect;
            frame.styleObject = this;
            frame.palette = m_styleoption->palette;
            qApp->style()->drawPrimitive(QStyle::PE_FrameMenu, &frame, painter);
        }
    }
        break;
    default:
        break;
    }
}

qreal QQuickStyleItem::textWidth(const QString &text)
{
    QFontMetricsF fm = QFontMetricsF(m_styleoption->fontMetrics);
    return fm.boundingRect(text).width();
}

qreal QQuickStyleItem::textHeight(const QString &text)
{
    QFontMetricsF fm = QFontMetricsF(m_styleoption->fontMetrics);
    return text.isEmpty() ? fm.height() :
                            fm.boundingRect(text).height();
}

QString QQuickStyleItem::elidedText(const QString &text, int elideMode, int width)
{
    return m_styleoption->fontMetrics.elidedText(text, Qt::TextElideMode(elideMode), width);
}

bool QQuickStyleItem::hasThemeIcon(const QString &icon) const
{
    return QIcon::hasThemeIcon(icon);
}

bool QQuickStyleItem::event(QEvent *ev)
{
    if (ev->type() == QEvent::StyleAnimationUpdate) {
        if (isVisible()) {
            ev->accept();
            polish();
        }
        return true;
    } else if (ev->type() == QEvent::StyleChange) {
        if (m_itemType == ScrollBar)
            initStyleOption();
    }
    return QQuickItem::event(ev);
}

void QQuickStyleItem::setTextureWidth(int w)
{
    if (m_textureWidth == w)
        return;
    m_textureWidth = w;
    emit textureWidthChanged(m_textureWidth);
    update();
}

void QQuickStyleItem::setTextureHeight(int h)
{
    if (m_textureHeight == h)
        return;
    m_textureHeight = h;
    emit textureHeightChanged(m_textureHeight);
    update();
}

QSGNode *QQuickStyleItem::updatePaintNode(QSGNode *node, UpdatePaintNodeData *)
{
    if (m_image.isNull()) {
        delete node;
        return 0;
    }

    QSGNinePatchNode *styleNode = static_cast<QSGNinePatchNode *>(node);
    if (!styleNode) {
        styleNode = QQuickItemPrivate::get(this)->sceneGraphContext()->createNinePatchNode();
        if (!styleNode)
            styleNode = new QQuickStyleNode;
    }

#ifdef QSG_RUNTIME_DESCRIPTION
    qsgnode_set_description(styleNode,
                            QString::fromLatin1("%1:%2, '%3'")
                            .arg(style())
                            .arg(elementType())
                            .arg(text()));
#endif

    styleNode->setTexture(window()->createTextureFromImage(m_image, QQuickWindow::TextureCanUseAtlas));
    styleNode->setBounds(boundingRect());
    styleNode->setDevicePixelRatio(window()->devicePixelRatio());
    styleNode->setPadding(m_border.left(), m_border.top(), m_border.right(), m_border.bottom());
    styleNode->update();

    return styleNode;
}

void QQuickStyleItem::updatePolish()
{
    if (width() >= 1 && height() >= 1) { // Note these are reals so 1 pixel is minimum
        float devicePixelRatio = window() ? window()->devicePixelRatio() : qApp->devicePixelRatio();
        int w = m_textureWidth > 0 ? m_textureWidth : width();
        int h = m_textureHeight > 0 ? m_textureHeight : height();
        m_image = QImage(w * devicePixelRatio, h * devicePixelRatio, QImage::Format_ARGB32_Premultiplied);
        m_image.setDevicePixelRatio(devicePixelRatio);
        m_image.fill(Qt::transparent);
        QPainter painter(&m_image);
        painter.setLayoutDirection(qApp->layoutDirection());
        paint(&painter);
        QQuickItem::update();
    } else if (!m_image.isNull()) {
        m_image = QImage();
        QQuickItem::update();
    }
}

QPixmap QQuickTableRowImageProvider::requestPixmap(const QString &id, QSize *size, const QSize &requestedSize)
{
    Q_UNUSED (requestedSize);
    int width = 16;
    int height = 16;
    if (size)
        *size = QSize(width, height);

    QPixmap pixmap(width, height);

    QStyleOptionViewItem opt;
    opt.state |= QStyle::State_Enabled;
    opt.rect = QRect(0, 0, width, height);
    QString style = qApp->style()->metaObject()->className();
    opt.features = 0;

    if (id.contains("selected"))
        opt.state |= QStyle::State_Selected;

    if (id.contains("active")) {
        opt.state |= QStyle::State_Active;
        opt.palette.setCurrentColorGroup(QPalette::Active);
    } else
        opt.palette.setCurrentColorGroup(QPalette::Inactive);

    if (id.contains("alternate"))
        opt.features |= QStyleOptionViewItem::Alternate;

    QPalette pal = QApplication::palette("QAbstractItemView");
    if (opt.state & QStyle::State_Selected && (style.contains("Mac") ||
                                               !qApp->style()->styleHint(QStyle::SH_ItemView_ShowDecorationSelected))) {
        pal.setCurrentColorGroup(opt.palette.currentColorGroup());
        pixmap.fill(pal.highlight().color());
    } else {
        pixmap.fill(pal.base().color());
        QPainter pixpainter(&pixmap);
        qApp->style()->drawPrimitive(QStyle::PE_PanelItemViewRow, &opt, &pixpainter);
    }
    return pixmap;
}

QT_END_NAMESPACE

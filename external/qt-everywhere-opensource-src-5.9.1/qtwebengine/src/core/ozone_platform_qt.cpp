/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "ozone_platform_qt.h"

#if defined(USE_OZONE)

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "ui/display/types/native_display_delegate.h"
#include "ui/events/ozone/events_ozone.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/ozone/common/stub_client_native_pixmap_factory.h"
#include "ui/ozone/common/stub_overlay_manager.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/cursor_factory_ozone.h"
#include "ui/ozone/public/gpu_platform_support_host.h"
#include "ui/ozone/public/input_controller.h"
#include "ui/ozone/public/system_input_injector.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/platform_window_delegate.h"

#include "surface_factory_qt.h"

namespace ui {

namespace {

class PlatformWindowQt : public PlatformWindow, public PlatformEventDispatcher
{
public:
    PlatformWindowQt(PlatformWindowDelegate* delegate,
                     const gfx::Rect& bounds)
        : delegate_(delegate)
        , bounds_(bounds)
    {
        ui::PlatformEventSource::GetInstance()->AddPlatformEventDispatcher(this);
    }

    ~PlatformWindowQt() override
    {
        ui::PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(this);
    }

    // PlatformWindow:
    gfx::Rect GetBounds() override;
    void SetBounds(const gfx::Rect& bounds) override;
    void Show() override { }
    void Hide() override { }
    void Close() override { }
    void SetTitle(const base::string16&) override { }
    void SetCapture() override { }
    void ReleaseCapture() override { }
    void ToggleFullscreen() override { }
    void Maximize() override { }
    void Minimize() override { }
    void Restore() override { }
    void SetCursor(PlatformCursor) override { }
    void MoveCursorTo(const gfx::Point&) override { }
    void ConfineCursorToBounds(const gfx::Rect&) override { }
    PlatformImeController* GetPlatformImeController() override { return nullptr; }
    // PlatformEventDispatcher:
    bool CanDispatchEvent(const PlatformEvent& event) override;
    uint32_t DispatchEvent(const PlatformEvent& event) override;

private:
    PlatformWindowDelegate* delegate_;
    gfx::Rect bounds_;

    DISALLOW_COPY_AND_ASSIGN(PlatformWindowQt);
};

gfx::Rect PlatformWindowQt::GetBounds()
{
    return bounds_;
}

void PlatformWindowQt::SetBounds(const gfx::Rect& bounds)
{
    if (bounds == bounds_)
        return;
    bounds_ = bounds;
    delegate_->OnBoundsChanged(bounds);
}

bool PlatformWindowQt::CanDispatchEvent(const ui::PlatformEvent& /*ne*/)
{
    return true;
}

uint32_t PlatformWindowQt::DispatchEvent(const ui::PlatformEvent& native_event)
{
    DispatchEventFromNativeUiEvent(
                native_event, base::Bind(&PlatformWindowDelegate::DispatchEvent,
                                         base::Unretained(delegate_)));

    return ui::POST_DISPATCH_STOP_PROPAGATION;
}

class OzonePlatformQt : public OzonePlatform {
public:
    OzonePlatformQt();
    ~OzonePlatformQt() override;

    ui::SurfaceFactoryOzone* GetSurfaceFactoryOzone() override;
    ui::CursorFactoryOzone* GetCursorFactoryOzone() override;
    GpuPlatformSupportHost* GetGpuPlatformSupportHost() override;
    std::unique_ptr<PlatformWindow> CreatePlatformWindow(PlatformWindowDelegate* delegate, const gfx::Rect& bounds) override;
    std::unique_ptr<ui::NativeDisplayDelegate> CreateNativeDisplayDelegate() override;
    ui::InputController* GetInputController() override;
    std::unique_ptr<ui::SystemInputInjector> CreateSystemInputInjector() override;
    ui::OverlayManagerOzone* GetOverlayManager() override;

private:
    void InitializeUI() override;
    void InitializeGPU() override;

    std::unique_ptr<QtWebEngineCore::SurfaceFactoryQt> surface_factory_ozone_;
    std::unique_ptr<CursorFactoryOzone> cursor_factory_ozone_;

    std::unique_ptr<GpuPlatformSupportHost> gpu_platform_support_host_;
    std::unique_ptr<InputController> input_controller_;
    std::unique_ptr<OverlayManagerOzone> overlay_manager_;

    DISALLOW_COPY_AND_ASSIGN(OzonePlatformQt);
};


OzonePlatformQt::OzonePlatformQt() {}

OzonePlatformQt::~OzonePlatformQt() {}

ui::SurfaceFactoryOzone* OzonePlatformQt::GetSurfaceFactoryOzone()
{
    return surface_factory_ozone_.get();
}

ui::CursorFactoryOzone* OzonePlatformQt::GetCursorFactoryOzone()
{
    return cursor_factory_ozone_.get();
}

GpuPlatformSupportHost* OzonePlatformQt::GetGpuPlatformSupportHost()
{
    return gpu_platform_support_host_.get();
}

std::unique_ptr<PlatformWindow> OzonePlatformQt::CreatePlatformWindow(PlatformWindowDelegate* delegate, const gfx::Rect& bounds)
{
    return base::WrapUnique(new PlatformWindowQt(delegate, bounds));
}

ui::InputController* OzonePlatformQt::GetInputController()
{
    return input_controller_.get();
}

std::unique_ptr<ui::SystemInputInjector> OzonePlatformQt::CreateSystemInputInjector()
{
    return nullptr;  // no input injection support.
}

ui::OverlayManagerOzone* OzonePlatformQt::GetOverlayManager()
{
    return overlay_manager_.get();
}

std::unique_ptr<ui::NativeDisplayDelegate> OzonePlatformQt::CreateNativeDisplayDelegate()
{
    NOTREACHED();
    return nullptr;
}

void OzonePlatformQt::InitializeUI()
{
    overlay_manager_.reset(new StubOverlayManager());
    cursor_factory_ozone_.reset(new CursorFactoryOzone());
    gpu_platform_support_host_.reset(ui::CreateStubGpuPlatformSupportHost());
    input_controller_ = CreateStubInputController();
}

void OzonePlatformQt::InitializeGPU()
{
    surface_factory_ozone_.reset(new QtWebEngineCore::SurfaceFactoryQt());
}

} // namespace


OzonePlatform* CreateOzonePlatformQt() { return new OzonePlatformQt; }

ClientNativePixmapFactory* CreateClientNativePixmapFactoryQt()
{
    return CreateStubClientNativePixmapFactory();
}

}  // namespace ui

#endif


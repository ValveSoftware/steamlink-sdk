// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/quick_launch/quick_launch_application.h"

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/mus/common/gpu_service.h"
#include "mash/public/interfaces/launchable.mojom.h"
#include "mojo/public/c/system/main.h"
#include "services/catalog/public/interfaces/catalog.mojom.h"
#include "services/shell/public/cpp/application_runner.h"
#include "services/shell/public/cpp/connector.h"
#include "services/shell/public/cpp/shell_client.h"
#include "services/tracing/public/cpp/tracing_impl.h"
#include "ui/views/background.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/mus/aura_init.h"
#include "ui/views/mus/window_manager_connection.h"
#include "ui/views/widget/widget_delegate.h"
#include "url/gurl.h"

namespace views {
class AuraInit;
}

namespace mash {
namespace quick_launch {

class QuickLaunchUI : public views::WidgetDelegateView,
                      public views::TextfieldController {
 public:
  QuickLaunchUI(QuickLaunchApplication* quick_launch,
                shell::Connector* connector,
                catalog::mojom::CatalogPtr catalog)
      : quick_launch_(quick_launch),
        connector_(connector),
        prompt_(new views::Textfield),
        catalog_(std::move(catalog)) {
    set_background(views::Background::CreateStandardPanelBackground());
    prompt_->set_controller(this);
    AddChildView(prompt_);

    UpdateEntries();
  }
  ~QuickLaunchUI() override {
    quick_launch_->RemoveWindow(GetWidget());
  }

 private:
  // Overridden from views::WidgetDelegate:
  views::View* GetContentsView() override { return this; }
  base::string16 GetWindowTitle() const override {
    // TODO(beng): use resources.
    return base::ASCIIToUTF16("QuickLaunch");
  }

  // Overridden from views::View:
  void Layout() override {
    gfx::Rect bounds = GetLocalBounds();
    bounds.Inset(5, 5);
    prompt_->SetBoundsRect(bounds);
  }
  gfx::Size GetPreferredSize() const override {
    gfx::Size ps = prompt_->GetPreferredSize();
    ps.Enlarge(500, 10);
    return ps;
  }

  // Overridden from views::TextFieldController:
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override {
    if (key_event.type() != ui::ET_KEY_PRESSED)
      return false;

    // The user didn't like our suggestion, don't make another until they
    // type another character.
    suggestion_rejected_ = key_event.key_code() == ui::VKEY_BACK ||
                           key_event.key_code() == ui::VKEY_DELETE;
    if (key_event.key_code() == ui::VKEY_RETURN) {
      Launch(Canonicalize(prompt_->text()), key_event.IsControlDown());
      prompt_->SetText(base::string16());
      UpdateEntries();
    }
    return false;
  }
  void ContentsChanged(views::Textfield* sender,
                       const base::string16& new_contents) override {
    // Don't keep making a suggestion if the user didn't like what we offered.
    if (suggestion_rejected_)
      return;

    // TODO(beng): it'd be nice if we persisted some history/scoring here.
    for (const auto& name : app_names_) {
      if (base::StartsWith(name, new_contents,
                           base::CompareCase::INSENSITIVE_ASCII)) {
        base::string16 suffix = name;
        base::ReplaceSubstringsAfterOffset(&suffix, 0, new_contents,
                                           base::string16());
        gfx::Range range(static_cast<uint32_t>(new_contents.size()),
                         static_cast<uint32_t>(name.size()));
        prompt_->SetText(name);
        prompt_->SelectRange(range);
        break;
      }
    }
  }

  std::string Canonicalize(const base::string16& input) const {
    base::string16 working;
    base::TrimWhitespace(input, base::TRIM_ALL, &working);
    GURL url(working);
    if (url.scheme() != "mojo" && url.scheme() != "exe")
      working = base::ASCIIToUTF16("mojo:") + working;
    return base::UTF16ToUTF8(working);
  }

  void UpdateEntries() {
    catalog_->GetEntriesProvidingClass(
        "mash:launchable",
        base::Bind(&QuickLaunchUI::OnGotCatalogEntries,
                   base::Unretained(this)));
  }

  void OnGotCatalogEntries(mojo::Array<catalog::mojom::EntryPtr> entries) {
    for (const auto& entry : entries)
      app_names_.insert(base::UTF8ToUTF16(entry->name.get()));
  }

  void Launch(const std::string& name, bool new_window) {
    std::unique_ptr<shell::Connection> connection = connector_->Connect(name);
    mojom::LaunchablePtr launchable;
    connection->GetInterface(&launchable);
    connections_.push_back(std::move(connection));
    launchable->Launch(mojom::kWindow,
                       new_window ? mojom::LaunchMode::MAKE_NEW
                                  : mojom::LaunchMode::REUSE);
  }

  QuickLaunchApplication* quick_launch_;
  shell::Connector* connector_;
  views::Textfield* prompt_;
  std::vector<std::unique_ptr<shell::Connection>> connections_;
  catalog::mojom::CatalogPtr catalog_;
  std::set<base::string16> app_names_;
  bool suggestion_rejected_ = false;

  DISALLOW_COPY_AND_ASSIGN(QuickLaunchUI);
};

QuickLaunchApplication::QuickLaunchApplication() {}
QuickLaunchApplication::~QuickLaunchApplication() {}

void QuickLaunchApplication::RemoveWindow(views::Widget* window) {
  auto it = std::find(windows_.begin(), windows_.end(), window);
  DCHECK(it != windows_.end());
  windows_.erase(it);
  if (windows_.empty() && base::MessageLoop::current()->is_running())
    base::MessageLoop::current()->QuitWhenIdle();
}

void QuickLaunchApplication::Initialize(shell::Connector* connector,
                                        const shell::Identity& identity,
                                        uint32_t id) {
  connector_ = connector;
  mus::GpuService::Initialize(connector);
  tracing_.Initialize(connector, identity.name());

  aura_init_.reset(new views::AuraInit(connector, "views_mus_resources.pak"));
  window_manager_connection_ =
      views::WindowManagerConnection::Create(connector, identity);

  Launch(mojom::kWindow, mojom::LaunchMode::MAKE_NEW);
}

bool QuickLaunchApplication::AcceptConnection(shell::Connection* connection) {
  connection->AddInterface<mojom::Launchable>(this);
  return true;
}

void QuickLaunchApplication::Launch(uint32_t what, mojom::LaunchMode how) {
  bool reuse = how == mojom::LaunchMode::REUSE ||
               how == mojom::LaunchMode::DEFAULT;
  if (reuse && !windows_.empty()) {
    windows_.back()->Activate();
    return;
  }
  catalog::mojom::CatalogPtr catalog;
  connector_->ConnectToInterface("mojo:catalog", &catalog);

  views::Widget* window = views::Widget::CreateWindowWithContextAndBounds(
      new QuickLaunchUI(this, connector_, std::move(catalog)), nullptr,
      gfx::Rect(10, 640, 0, 0));
  window->Show();
  windows_.push_back(window);
}

void QuickLaunchApplication::Create(shell::Connection* connection,
                                    mojom::LaunchableRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

}  // namespace quick_launch
}  // namespace mash

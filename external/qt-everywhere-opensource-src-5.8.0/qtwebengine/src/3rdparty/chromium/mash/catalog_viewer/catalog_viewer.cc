// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/catalog_viewer/catalog_viewer.h"

#include <stddef.h>
#include <stdint.h>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/catalog/public/interfaces/catalog.mojom.h"
#include "services/shell/public/cpp/connection.h"
#include "services/shell/public/cpp/connector.h"
#include "ui/base/models/table_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/background.h"
#include "ui/views/controls/table/table_view.h"
#include "ui/views/controls/table/table_view_observer.h"
#include "ui/views/mus/aura_init.h"
#include "ui/views/mus/window_manager_connection.h"
#include "ui/views/widget/widget_delegate.h"

namespace mash {
namespace catalog_viewer {
namespace {

using shell::mojom::InstanceInfoPtr;

class CatalogViewerContents : public views::WidgetDelegateView,
                              public ui::TableModel {
 public:
  CatalogViewerContents(CatalogViewer* catalog_viewer,
                        catalog::mojom::CatalogPtr catalog)
      : catalog_viewer_(catalog_viewer),
        catalog_(std::move(catalog)),
        table_view_(nullptr),
        table_view_parent_(nullptr),
        observer_(nullptr),
        weak_ptr_factory_(this) {
    table_view_ = new views::TableView(this, GetColumns(), views::TEXT_ONLY,
                                       false);
    set_background(views::Background::CreateStandardPanelBackground());

    table_view_parent_ = table_view_->CreateParentIfNecessary();
    AddChildView(table_view_parent_);

    catalog_->GetEntries(nullptr,
                         base::Bind(&CatalogViewerContents::OnGotCatalogEntries,
                                    weak_ptr_factory_.GetWeakPtr()));
    // We don't want to show an empty UI so we just block until we have all the
    // data.
    catalog_.WaitForIncomingResponse();
  }
  ~CatalogViewerContents() override {
    table_view_->SetModel(nullptr);
    catalog_viewer_->RemoveWindow(GetWidget());
  }

 private:
  struct Entry {
    Entry(const std::string& name, const std::string& url)
        : name(name), url(url) {}
    std::string name;
    std::string url;
  };


  // Overridden from views::WidgetDelegate:
  views::View* GetContentsView() override { return this; }
  base::string16 GetWindowTitle() const override {
    // TODO(beng): use resources.
    return base::ASCIIToUTF16("Applications");
  }
  bool CanResize() const override { return true; }
  bool CanMaximize() const override { return true; }
  bool CanMinimize() const override { return true; }

  gfx::ImageSkia GetWindowAppIcon() override {
    // TODO(jamescook): Create a new .pak file for this app and make a custom
    // icon, perhaps one that looks like the Chrome OS task viewer icon.
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    return *rb.GetImageSkiaNamed(IDR_NOTIFICATION_SETTINGS);
  }

  // Overridden from views::View:
  void Layout() override {
    gfx::Rect bounds = GetLocalBounds();
    bounds.Inset(10, 10);
    table_view_parent_->SetBoundsRect(bounds);
  }

  // Overridden from ui::TableModel:
  int RowCount() override {
    return static_cast<int>(entries_.size());
  }
  base::string16 GetText(int row, int column_id) override {
    switch(column_id) {
    case 0:
      DCHECK(row < static_cast<int>(entries_.size()));
      return base::UTF8ToUTF16(entries_[row].name);
    case 1:
      DCHECK(row < static_cast<int>(entries_.size()));
      return base::UTF8ToUTF16(entries_[row].url);
    default:
      NOTREACHED();
      break;
    }
    return base::string16();
  }
  void SetObserver(ui::TableModelObserver* observer) override {
    observer_ = observer;
  }

  void OnGotCatalogEntries(mojo::Array<catalog::mojom::EntryPtr> entries) {
    entries_.clear();
    for (auto& entry : entries)
      entries_.push_back(Entry(entry->display_name, entry->name));
    observer_->OnModelChanged();
  }

  static std::vector<ui::TableColumn> GetColumns() {
    std::vector<ui::TableColumn> columns;

    ui::TableColumn name_column;
    name_column.id = 0;
    // TODO(beng): use resources.
    name_column.title = base::ASCIIToUTF16("Name");
    name_column.width = -1;
    name_column.percent = 0.4f;
    name_column.sortable = true;
    columns.push_back(name_column);

    ui::TableColumn url_column;
    url_column.id = 1;
    // TODO(beng): use resources.
    url_column.title = base::ASCIIToUTF16("URL");
    url_column.width = -1;
    url_column.percent = 0.4f;
    url_column.sortable = true;
    columns.push_back(url_column);

    return columns;
  }

  CatalogViewer* catalog_viewer_;
  catalog::mojom::CatalogPtr catalog_;

  views::TableView* table_view_;
  views::View* table_view_parent_;
  ui::TableModelObserver* observer_;

  std::vector<Entry> entries_;

  base::WeakPtrFactory<CatalogViewerContents> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CatalogViewerContents);
};

}  // namespace

CatalogViewer::CatalogViewer() {}
CatalogViewer::~CatalogViewer() {}

void CatalogViewer::RemoveWindow(views::Widget* window) {
  auto it = std::find(windows_.begin(), windows_.end(), window);
  DCHECK(it != windows_.end());
  windows_.erase(it);
  if (windows_.empty())
    base::MessageLoop::current()->QuitWhenIdle();
}

void CatalogViewer::Initialize(shell::Connector* connector,
                               const shell::Identity& identity,
                               uint32_t id) {
  connector_ = connector;
  tracing_.Initialize(connector, identity.name());

  aura_init_.reset(new views::AuraInit(connector, "views_mus_resources.pak"));
  window_manager_connection_ =
      views::WindowManagerConnection::Create(connector, identity);
}

bool CatalogViewer::AcceptConnection(shell::Connection* connection) {
  connection->AddInterface<mojom::Launchable>(this);
  return true;
}

void CatalogViewer::Launch(uint32_t what, mojom::LaunchMode how) {
  bool reuse = how == mojom::LaunchMode::REUSE ||
               how == mojom::LaunchMode::DEFAULT;
  if (reuse && !windows_.empty()) {
    windows_.back()->Activate();
    return;
  }
  catalog::mojom::CatalogPtr catalog;
  connector_->ConnectToInterface("mojo:catalog", &catalog);

  views::Widget* window = views::Widget::CreateWindowWithContextAndBounds(
      new CatalogViewerContents(this, std::move(catalog)), nullptr,
      gfx::Rect(25, 25, 500, 600));
  window->Show();
  windows_.push_back(window);
}

void CatalogViewer::Create(shell::Connection* connection,
                           mojom::LaunchableRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

}  // namespace catalog_viewer
}  // namespace mash

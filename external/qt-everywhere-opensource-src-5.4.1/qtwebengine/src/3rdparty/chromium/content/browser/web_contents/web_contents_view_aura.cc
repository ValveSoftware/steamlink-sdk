// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/web_contents_view_aura.h"

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/metrics/histogram.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/download/drag_download_util.h"
#include "content/browser/frame_host/interstitial_page_impl.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/renderer_host/dip_util.h"
#include "content/browser/renderer_host/overscroll_controller.h"
#include "content/browser/renderer_host/render_view_host_factory.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "content/browser/web_contents/aura/gesture_nav_simple.h"
#include "content/browser/web_contents/aura/image_window_delegate.h"
#include "content/browser/web_contents/aura/overscroll_navigation_overlay.h"
#include "content/browser/web_contents/aura/shadow_layer_delegate.h"
#include "content/browser/web_contents/aura/window_slider.h"
#include "content/browser/web_contents/touch_editable_impl_aura.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/overscroll_configuration.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_view_delegate.h"
#include "content/public/browser/web_drag_dest_delegate.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/drop_data.h"
#include "net/base/filename_util.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/window_tree_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_tree_host.h"
#include "ui/aura/window_tree_host_observer.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/drag_utils.h"
#include "ui/base/dragdrop/drop_target_event.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_png_rep.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/screen.h"
#include "ui/wm/public/drag_drop_client.h"
#include "ui/wm/public/drag_drop_delegate.h"

namespace content {
WebContentsView* CreateWebContentsView(
    WebContentsImpl* web_contents,
    WebContentsViewDelegate* delegate,
    RenderViewHostDelegateView** render_view_host_delegate_view) {
  WebContentsViewAura* rv = new WebContentsViewAura(web_contents, delegate);
  *render_view_host_delegate_view = rv;
  return rv;
}

namespace {

bool IsScrollEndEffectEnabled() {
  return CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
      switches::kScrollEndEffect) == "1";
}

bool ShouldNavigateForward(const NavigationController& controller,
                           OverscrollMode mode) {
  return mode == (base::i18n::IsRTL() ? OVERSCROLL_EAST : OVERSCROLL_WEST) &&
         controller.CanGoForward();
}

bool ShouldNavigateBack(const NavigationController& controller,
                        OverscrollMode mode) {
  return mode == (base::i18n::IsRTL() ? OVERSCROLL_WEST : OVERSCROLL_EAST) &&
         controller.CanGoBack();
}

// Update the |web contents| to be |visible|.
void UpdateWebContentsVisibility(WebContentsImpl* web_contents, bool visible) {
  if (visible) {
    if (!web_contents->should_normally_be_visible())
      web_contents->WasShown();
  } else {
    if (web_contents->should_normally_be_visible())
      web_contents->WasHidden();
  }
}

RenderWidgetHostViewAura* ToRenderWidgetHostViewAura(
    RenderWidgetHostView* view) {
  if (!view || RenderViewHostFactory::has_factory())
    return NULL;  // Can't cast to RenderWidgetHostViewAura in unit tests.

  RenderViewHost* rvh = RenderViewHost::From(view->GetRenderWidgetHost());
  WebContentsImpl* web_contents = static_cast<WebContentsImpl*>(
      rvh ? WebContents::FromRenderViewHost(rvh) : NULL);
  if (BrowserPluginGuest::IsGuest(web_contents))
    return NULL;
  return static_cast<RenderWidgetHostViewAura*>(view);
}

// The window delegate for the overscroll window. This redirects trackpad events
// to the web-contents window. The delegate destroys itself when the window is
// destroyed.
class OverscrollWindowDelegate : public ImageWindowDelegate {
 public:
  OverscrollWindowDelegate(WebContentsImpl* web_contents,
                           OverscrollMode overscroll_mode)
      : web_contents_(web_contents),
        forward_events_(true) {
    const NavigationControllerImpl& controller = web_contents->GetController();
    const NavigationEntryImpl* entry = NULL;
    if (ShouldNavigateForward(controller, overscroll_mode)) {
      entry = NavigationEntryImpl::FromNavigationEntry(
          controller.GetEntryAtOffset(1));
    } else if (ShouldNavigateBack(controller, overscroll_mode)) {
      entry = NavigationEntryImpl::FromNavigationEntry(
          controller.GetEntryAtOffset(-1));
    }

    gfx::Image image;
    if (entry && entry->screenshot().get()) {
      std::vector<gfx::ImagePNGRep> image_reps;
      image_reps.push_back(gfx::ImagePNGRep(entry->screenshot(),
          ui::GetScaleFactorForNativeView(web_contents_window())));
      image = gfx::Image(image_reps);
    }
    SetImage(image);
  }

  void stop_forwarding_events() { forward_events_ = false; }

 private:
  virtual ~OverscrollWindowDelegate() {}

  aura::Window* web_contents_window() {
    return web_contents_->GetView()->GetContentNativeView();
  }

  // Overridden from ui::EventHandler.
  virtual void OnScrollEvent(ui::ScrollEvent* event) OVERRIDE {
    if (forward_events_ && web_contents_window())
      web_contents_window()->delegate()->OnScrollEvent(event);
  }

  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE {
    if (forward_events_ && web_contents_window())
      web_contents_window()->delegate()->OnGestureEvent(event);
  }

  WebContentsImpl* web_contents_;

  // The window is displayed both during the gesture, and after the gesture
  // while the navigation is in progress. During the gesture, it is necessary to
  // forward input events to the content page (e.g. when the overscroll window
  // slides under the cursor and starts receiving scroll events). However, once
  // the gesture is complete, and the window is being displayed as an overlay
  // window during navigation, events should not be forwarded anymore.
  bool forward_events_;

  DISALLOW_COPY_AND_ASSIGN(OverscrollWindowDelegate);
};

// Listens to all mouse drag events during a drag and drop and sends them to
// the renderer.
class WebDragSourceAura : public NotificationObserver {
 public:
  WebDragSourceAura(aura::Window* window, WebContentsImpl* contents)
      : window_(window),
        contents_(contents) {
    registrar_.Add(this,
                   NOTIFICATION_WEB_CONTENTS_DISCONNECTED,
                   Source<WebContents>(contents));
  }

  virtual ~WebDragSourceAura() {
  }

  // NotificationObserver:
  virtual void Observe(int type,
      const NotificationSource& source,
      const NotificationDetails& details) OVERRIDE {
    if (type != NOTIFICATION_WEB_CONTENTS_DISCONNECTED)
      return;

    // Cancel the drag if it is still in progress.
    aura::client::DragDropClient* dnd_client =
        aura::client::GetDragDropClient(window_->GetRootWindow());
    if (dnd_client && dnd_client->IsDragDropInProgress())
      dnd_client->DragCancel();

    window_ = NULL;
    contents_ = NULL;
  }

  aura::Window* window() const { return window_; }

 private:
  aura::Window* window_;
  WebContentsImpl* contents_;
  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(WebDragSourceAura);
};

#if (!defined(OS_CHROMEOS) && defined(USE_X11)) || defined(OS_WIN)
// Fill out the OSExchangeData with a file contents, synthesizing a name if
// necessary.
void PrepareDragForFileContents(const DropData& drop_data,
                                ui::OSExchangeData::Provider* provider) {
  base::FilePath file_name =
      base::FilePath::FromUTF16Unsafe(drop_data.file_description_filename);
  // Images without ALT text will only have a file extension so we need to
  // synthesize one from the provided extension and URL.
  if (file_name.BaseName().RemoveExtension().empty()) {
    const base::FilePath::StringType extension = file_name.Extension();
    // Retrieve the name from the URL.
    file_name = net::GenerateFileName(drop_data.url, "", "", "", "", "")
                    .ReplaceExtension(extension);
  }
  provider->SetFileContents(file_name, drop_data.file_contents);
}
#endif

#if defined(OS_WIN)
void PrepareDragForDownload(
    const DropData& drop_data,
    ui::OSExchangeData::Provider* provider,
    WebContentsImpl* web_contents) {
  const GURL& page_url = web_contents->GetLastCommittedURL();
  const std::string& page_encoding = web_contents->GetEncoding();

  // Parse the download metadata.
  base::string16 mime_type;
  base::FilePath file_name;
  GURL download_url;
  if (!ParseDownloadMetadata(drop_data.download_metadata,
                             &mime_type,
                             &file_name,
                             &download_url))
    return;

  // Generate the file name based on both mime type and proposed file name.
  std::string default_name =
      GetContentClient()->browser()->GetDefaultDownloadName();
  base::FilePath generated_download_file_name =
      net::GenerateFileName(download_url,
                            std::string(),
                            std::string(),
                            base::UTF16ToUTF8(file_name.value()),
                            base::UTF16ToUTF8(mime_type),
                            default_name);

  // http://crbug.com/332579
  base::ThreadRestrictions::ScopedAllowIO allow_file_operations;

  base::FilePath temp_dir_path;
  if (!base::CreateNewTempDirectory(FILE_PATH_LITERAL("chrome_drag"),
                                    &temp_dir_path))
    return;

  base::FilePath download_path =
      temp_dir_path.Append(generated_download_file_name);

  // We cannot know when the target application will be done using the temporary
  // file, so schedule it to be deleted after rebooting.
  base::DeleteFileAfterReboot(download_path);
  base::DeleteFileAfterReboot(temp_dir_path);

  // Provide the data as file (CF_HDROP). A temporary download file with the
  // Zone.Identifier ADS (Alternate Data Stream) attached will be created.
  scoped_refptr<DragDownloadFile> download_file =
      new DragDownloadFile(
          download_path,
          base::File(),
          download_url,
          Referrer(page_url, drop_data.referrer_policy),
          page_encoding,
          web_contents);
  ui::OSExchangeData::DownloadFileInfo file_download(base::FilePath(),
                                                     download_file.get());
  provider->SetDownloadFileInfo(file_download);
}
#endif  // defined(OS_WIN)

// Returns the CustomFormat to store file system files.
const ui::OSExchangeData::CustomFormat& GetFileSystemFileCustomFormat() {
  static const char kFormatString[] = "chromium/x-file-system-files";
  CR_DEFINE_STATIC_LOCAL(ui::OSExchangeData::CustomFormat,
                         format,
                         (ui::Clipboard::GetFormatType(kFormatString)));
  return format;
}

// Writes file system files to the pickle.
void WriteFileSystemFilesToPickle(
    const std::vector<DropData::FileSystemFileInfo>& file_system_files,
    Pickle* pickle) {
  pickle->WriteUInt64(file_system_files.size());
  for (size_t i = 0; i < file_system_files.size(); ++i) {
    pickle->WriteString(file_system_files[i].url.spec());
    pickle->WriteInt64(file_system_files[i].size);
  }
}

// Reads file system files from the pickle.
bool ReadFileSystemFilesFromPickle(
    const Pickle& pickle,
    std::vector<DropData::FileSystemFileInfo>* file_system_files) {
  PickleIterator iter(pickle);

  uint64 num_files = 0;
  if (!pickle.ReadUInt64(&iter, &num_files))
    return false;
  file_system_files->resize(num_files);

  for (uint64 i = 0; i < num_files; ++i) {
    std::string url_string;
    int64 size = 0;
    if (!pickle.ReadString(&iter, &url_string) ||
        !pickle.ReadInt64(&iter, &size))
      return false;

    GURL url(url_string);
    if (!url.is_valid())
      return false;

    (*file_system_files)[i].url = url;
    (*file_system_files)[i].size = size;
  }
  return true;
}

// Utility to fill a ui::OSExchangeDataProvider object from DropData.
void PrepareDragData(const DropData& drop_data,
                     ui::OSExchangeData::Provider* provider,
                     WebContentsImpl* web_contents) {
  provider->MarkOriginatedFromRenderer();
#if defined(OS_WIN)
  // Put download before file contents to prefer the download of a image over
  // its thumbnail link.
  if (!drop_data.download_metadata.empty())
    PrepareDragForDownload(drop_data, provider, web_contents);
#endif
#if (!defined(OS_CHROMEOS) && defined(USE_X11)) || defined(OS_WIN)
  // We set the file contents before the URL because the URL also sets file
  // contents (to a .URL shortcut).  We want to prefer file content data over
  // a shortcut so we add it first.
  if (!drop_data.file_contents.empty())
    PrepareDragForFileContents(drop_data, provider);
#endif
  if (!drop_data.text.string().empty())
    provider->SetString(drop_data.text.string());
  if (drop_data.url.is_valid())
    provider->SetURL(drop_data.url, drop_data.url_title);
  if (!drop_data.html.string().empty())
    provider->SetHtml(drop_data.html.string(), drop_data.html_base_url);
  if (!drop_data.filenames.empty())
    provider->SetFilenames(drop_data.filenames);
  if (!drop_data.file_system_files.empty()) {
    Pickle pickle;
    WriteFileSystemFilesToPickle(drop_data.file_system_files, &pickle);
    provider->SetPickledData(GetFileSystemFileCustomFormat(), pickle);
  }
  if (!drop_data.custom_data.empty()) {
    Pickle pickle;
    ui::WriteCustomDataToPickle(drop_data.custom_data, &pickle);
    provider->SetPickledData(ui::Clipboard::GetWebCustomDataFormatType(),
                             pickle);
  }
}

// Utility to fill a DropData object from ui::OSExchangeData.
void PrepareDropData(DropData* drop_data, const ui::OSExchangeData& data) {
  drop_data->did_originate_from_renderer = data.DidOriginateFromRenderer();

  base::string16 plain_text;
  data.GetString(&plain_text);
  if (!plain_text.empty())
    drop_data->text = base::NullableString16(plain_text, false);

  GURL url;
  base::string16 url_title;
  data.GetURLAndTitle(
      ui::OSExchangeData::DO_NOT_CONVERT_FILENAMES, &url, &url_title);
  if (url.is_valid()) {
    drop_data->url = url;
    drop_data->url_title = url_title;
  }

  base::string16 html;
  GURL html_base_url;
  data.GetHtml(&html, &html_base_url);
  if (!html.empty())
    drop_data->html = base::NullableString16(html, false);
  if (html_base_url.is_valid())
    drop_data->html_base_url = html_base_url;

  data.GetFilenames(&drop_data->filenames);

  Pickle pickle;
  std::vector<DropData::FileSystemFileInfo> file_system_files;
  if (data.GetPickledData(GetFileSystemFileCustomFormat(), &pickle) &&
      ReadFileSystemFilesFromPickle(pickle, &file_system_files))
    drop_data->file_system_files = file_system_files;

  if (data.GetPickledData(ui::Clipboard::GetWebCustomDataFormatType(), &pickle))
    ui::ReadCustomDataIntoMap(
        pickle.data(), pickle.size(), &drop_data->custom_data);
}

// Utilities to convert between blink::WebDragOperationsMask and
// ui::DragDropTypes.
int ConvertFromWeb(blink::WebDragOperationsMask ops) {
  int drag_op = ui::DragDropTypes::DRAG_NONE;
  if (ops & blink::WebDragOperationCopy)
    drag_op |= ui::DragDropTypes::DRAG_COPY;
  if (ops & blink::WebDragOperationMove)
    drag_op |= ui::DragDropTypes::DRAG_MOVE;
  if (ops & blink::WebDragOperationLink)
    drag_op |= ui::DragDropTypes::DRAG_LINK;
  return drag_op;
}

blink::WebDragOperationsMask ConvertToWeb(int drag_op) {
  int web_drag_op = blink::WebDragOperationNone;
  if (drag_op & ui::DragDropTypes::DRAG_COPY)
    web_drag_op |= blink::WebDragOperationCopy;
  if (drag_op & ui::DragDropTypes::DRAG_MOVE)
    web_drag_op |= blink::WebDragOperationMove;
  if (drag_op & ui::DragDropTypes::DRAG_LINK)
    web_drag_op |= blink::WebDragOperationLink;
  return (blink::WebDragOperationsMask) web_drag_op;
}

int ConvertAuraEventFlagsToWebInputEventModifiers(int aura_event_flags) {
  int web_input_event_modifiers = 0;
  if (aura_event_flags & ui::EF_SHIFT_DOWN)
    web_input_event_modifiers |= blink::WebInputEvent::ShiftKey;
  if (aura_event_flags & ui::EF_CONTROL_DOWN)
    web_input_event_modifiers |= blink::WebInputEvent::ControlKey;
  if (aura_event_flags & ui::EF_ALT_DOWN)
    web_input_event_modifiers |= blink::WebInputEvent::AltKey;
  if (aura_event_flags & ui::EF_COMMAND_DOWN)
    web_input_event_modifiers |= blink::WebInputEvent::MetaKey;
  return web_input_event_modifiers;
}

}  // namespace

class WebContentsViewAura::WindowObserver
    : public aura::WindowObserver, public aura::WindowTreeHostObserver {
 public:
  explicit WindowObserver(WebContentsViewAura* view)
      : view_(view),
        parent_(NULL) {
    view_->window_->AddObserver(this);

#if defined(OS_WIN)
    if (view_->window_->GetRootWindow())
      view_->window_->GetRootWindow()->AddObserver(this);
#endif
  }

  virtual ~WindowObserver() {
    view_->window_->RemoveObserver(this);
    if (view_->window_->GetHost())
      view_->window_->GetHost()->RemoveObserver(this);
    if (parent_)
      parent_->RemoveObserver(this);

#if defined(OS_WIN)
    if (parent_) {
      const aura::Window::Windows& children = parent_->children();
      for (size_t i = 0; i < children.size(); ++i)
        children[i]->RemoveObserver(this);
    }

    aura::Window* root_window = view_->window_->GetRootWindow();
    if (root_window) {
      root_window->RemoveObserver(this);
      const aura::Window::Windows& root_children = root_window->children();
      for (size_t i = 0; i < root_children.size(); ++i)
        root_children[i]->RemoveObserver(this);
    }
#endif
  }

  // Overridden from aura::WindowObserver:
#if defined(OS_WIN)
  // Constrained windows are added as children of the parent's parent's view
  // which may overlap with windowed NPAPI plugins. In that case, tell the RWHV
  // so that it can update the plugins' cutout rects accordingly.
  // Note: this is hard coding how Chrome layer adds its dialogs. Since NPAPI is
  // going to be deprecated in a year, this is ok for now. The test for this is
  // PrintPreviewTest.WindowedNPAPIPluginHidden.
  virtual void OnWindowAdded(aura::Window* new_window) OVERRIDE {
    if (new_window != view_->window_) {
      // Skip the case when the parent moves to the root window.
      if (new_window != parent_) {
        // Observe sibling windows of the WebContents, or children of the root
        // window.
        if (new_window->parent() == parent_ ||
            new_window->parent() == view_->window_->GetRootWindow()) {
          new_window->AddObserver(this);
        }
      }
    }

    if (new_window->parent() == parent_) {
      UpdateConstrainedWindows(NULL);
    }
  }

  virtual void OnWillRemoveWindow(aura::Window* window) OVERRIDE {
    if (window == view_->window_)
      return;

    window->RemoveObserver(this);
    UpdateConstrainedWindows(window);
  }

  virtual void OnWindowVisibilityChanged(aura::Window* window,
                                         bool visible) OVERRIDE {
    if (window == view_->window_ ||
        window->parent() == parent_ ||
        window->parent() == view_->window_->GetRootWindow()) {
      UpdateConstrainedWindows(NULL);
    }
  }
#endif

  virtual void OnWindowParentChanged(aura::Window* window,
                                     aura::Window* parent) OVERRIDE {
    if (window != view_->window_)
      return;
    if (parent_)
      parent_->RemoveObserver(this);

#if defined(OS_WIN)
    if (parent_) {
      const aura::Window::Windows& children = parent_->children();
      for (size_t i = 0; i < children.size(); ++i)
        children[i]->RemoveObserver(this);

      RenderWidgetHostViewAura* view = ToRenderWidgetHostViewAura(
          view_->web_contents_->GetRenderWidgetHostView());
      if (view)
        view->UpdateConstrainedWindowRects(std::vector<gfx::Rect>());
    }

    // When we get parented to the root window, the code below will watch the
    // parent, aka root window. Since we already watch the root window on
    // Windows, unregister first so that the debug check doesn't fire.
    if (parent && parent == window->GetRootWindow())
      parent->RemoveObserver(this);

    // We need to undo the above if we were parented to the root window and then
    // got parented to another window. At that point, the code before the ifdef
    // would have stopped watching the root window.
    if (window->GetRootWindow() &&
        parent != window->GetRootWindow() &&
        !window->GetRootWindow()->HasObserver(this)) {
      window->GetRootWindow()->AddObserver(this);
    }
#endif

    parent_ = parent;
    if (parent) {
      parent->AddObserver(this);
#if defined(OS_WIN)
      if (parent != window->GetRootWindow()) {
        const aura::Window::Windows& children = parent->children();
        for (size_t i = 0; i < children.size(); ++i) {
          if (children[i] != view_->window_)
            children[i]->AddObserver(this);
        }
      }
#endif
    }
  }

  virtual void OnWindowBoundsChanged(aura::Window* window,
                                     const gfx::Rect& old_bounds,
                                     const gfx::Rect& new_bounds) OVERRIDE {
    if (window == parent_ || window == view_->window_) {
      SendScreenRects();
      if (view_->touch_editable_)
        view_->touch_editable_->UpdateEditingController();
#if defined(OS_WIN)
    } else {
      UpdateConstrainedWindows(NULL);
#endif
    }
  }

  virtual void OnWindowAddedToRootWindow(aura::Window* window) OVERRIDE {
    if (window == view_->window_) {
      window->GetHost()->AddObserver(this);
#if defined(OS_WIN)
      if (!window->GetRootWindow()->HasObserver(this))
        window->GetRootWindow()->AddObserver(this);
#endif
    }
  }

  virtual void OnWindowRemovingFromRootWindow(aura::Window* window,
                                              aura::Window* new_root) OVERRIDE {
    if (window == view_->window_) {
      window->GetHost()->RemoveObserver(this);
#if defined(OS_WIN)
      window->GetRootWindow()->RemoveObserver(this);

      const aura::Window::Windows& root_children =
          window->GetRootWindow()->children();
      for (size_t i = 0; i < root_children.size(); ++i) {
        if (root_children[i] != view_->window_ && root_children[i] != parent_)
          root_children[i]->RemoveObserver(this);
      }
#endif
    }
  }

  // Overridden WindowTreeHostObserver:
  virtual void OnHostMoved(const aura::WindowTreeHost* host,
                           const gfx::Point& new_origin) OVERRIDE {
    TRACE_EVENT1("ui",
                 "WebContentsViewAura::WindowObserver::OnHostMoved",
                 "new_origin", new_origin.ToString());

    // This is for the desktop case (i.e. Aura desktop).
    SendScreenRects();
  }

 private:
  void SendScreenRects() {
    RenderWidgetHostImpl::From(view_->web_contents_->GetRenderViewHost())->
        SendScreenRects();
  }

#if defined(OS_WIN)
  void UpdateConstrainedWindows(aura::Window* exclude) {
    RenderWidgetHostViewAura* view = ToRenderWidgetHostViewAura(
        view_->web_contents_->GetRenderWidgetHostView());
    if (!view)
      return;

    std::vector<gfx::Rect> constrained_windows;
    if (parent_) {
      const aura::Window::Windows& children = parent_->children();
      for (size_t i = 0; i < children.size(); ++i) {
        if (children[i] != view_->window_ &&
            children[i] != exclude &&
            children[i]->IsVisible()) {
          constrained_windows.push_back(children[i]->GetBoundsInRootWindow());
        }
      }
    }

    aura::Window* root_window = view_->window_->GetRootWindow();
    const aura::Window::Windows& root_children = root_window->children();
    if (root_window) {
      for (size_t i = 0; i < root_children.size(); ++i) {
        if (root_children[i]->IsVisible() &&
            !root_children[i]->Contains(view_->window_.get())) {
          constrained_windows.push_back(
              root_children[i]->GetBoundsInRootWindow());
        }
      }
    }

    view->UpdateConstrainedWindowRects(constrained_windows);
  }
#endif

  WebContentsViewAura* view_;

  // We cache the old parent so that we can unregister when it's not the parent
  // anymore.
  aura::Window* parent_;

  DISALLOW_COPY_AND_ASSIGN(WindowObserver);
};

////////////////////////////////////////////////////////////////////////////////
// WebContentsViewAura, public:

WebContentsViewAura::WebContentsViewAura(
    WebContentsImpl* web_contents,
    WebContentsViewDelegate* delegate)
    : web_contents_(web_contents),
      delegate_(delegate),
      current_drag_op_(blink::WebDragOperationNone),
      drag_dest_delegate_(NULL),
      current_rvh_for_drag_(NULL),
      overscroll_change_brightness_(false),
      current_overscroll_gesture_(OVERSCROLL_NONE),
      completed_overscroll_gesture_(OVERSCROLL_NONE),
      touch_editable_(TouchEditableImplAura::Create()) {
}

////////////////////////////////////////////////////////////////////////////////
// WebContentsViewAura, private:

WebContentsViewAura::~WebContentsViewAura() {
  if (!window_)
    return;

  window_observer_.reset();
  window_->RemoveObserver(this);

  // Window needs a valid delegate during its destructor, so we explicitly
  // delete it here.
  window_.reset();
}

void WebContentsViewAura::SetTouchEditableForTest(
    TouchEditableImplAura* touch_editable) {
  touch_editable_.reset(touch_editable);
  AttachTouchEditableToRenderView();
}

void WebContentsViewAura::SizeChangedCommon(const gfx::Size& size) {
  if (web_contents_->GetInterstitialPage())
    web_contents_->GetInterstitialPage()->SetSize(size);
  RenderWidgetHostView* rwhv =
      web_contents_->GetRenderWidgetHostView();
  if (rwhv)
    rwhv->SetSize(size);
}

void WebContentsViewAura::EndDrag(blink::WebDragOperationsMask ops) {
  aura::Window* root_window = GetNativeView()->GetRootWindow();
  gfx::Point screen_loc =
      gfx::Screen::GetScreenFor(GetNativeView())->GetCursorScreenPoint();
  gfx::Point client_loc = screen_loc;
  RenderViewHost* rvh = web_contents_->GetRenderViewHost();
  aura::Window* window = rvh->GetView()->GetNativeView();
  aura::Window::ConvertPointToTarget(root_window, window, &client_loc);
  if (!web_contents_)
    return;
  web_contents_->DragSourceEndedAt(client_loc.x(), client_loc.y(),
      screen_loc.x(), screen_loc.y(), ops);
}

void WebContentsViewAura::InstallOverscrollControllerDelegate(
    RenderWidgetHostViewAura* view) {
  const std::string value = CommandLine::ForCurrentProcess()->
      GetSwitchValueASCII(switches::kOverscrollHistoryNavigation);
  if (value == "0") {
    navigation_overlay_.reset();
    return;
  }
  if (value == "2") {
    navigation_overlay_.reset();
    if (!gesture_nav_simple_)
      gesture_nav_simple_.reset(new GestureNavSimple(web_contents_));
    view->overscroll_controller()->set_delegate(gesture_nav_simple_.get());
    return;
  }
  view->overscroll_controller()->set_delegate(this);
  if (!navigation_overlay_)
    navigation_overlay_.reset(new OverscrollNavigationOverlay(web_contents_));
}

void WebContentsViewAura::PrepareOverscrollWindow() {
  // If there is an existing |overscroll_window_| which is in the middle of an
  // animation, then destroying the window here causes the animation to be
  // completed immidiately, which triggers |OnImplicitAnimationsCompleted()|
  // callback, and that tries to reset |overscroll_window_| again, causing a
  // double-free. So use a temporary variable here.
  if (overscroll_window_) {
    base::AutoReset<OverscrollMode> reset_state(&current_overscroll_gesture_,
                                                current_overscroll_gesture_);
    scoped_ptr<aura::Window> reset_window(overscroll_window_.release());
  }

  OverscrollWindowDelegate* overscroll_delegate = new OverscrollWindowDelegate(
      web_contents_,
      current_overscroll_gesture_);
  overscroll_window_.reset(new aura::Window(overscroll_delegate));
  overscroll_window_->SetType(ui::wm::WINDOW_TYPE_CONTROL);
  overscroll_window_->SetTransparent(true);
  overscroll_window_->Init(aura::WINDOW_LAYER_TEXTURED);
  overscroll_window_->layer()->SetMasksToBounds(false);
  overscroll_window_->SetName("OverscrollOverlay");

  overscroll_change_brightness_ = overscroll_delegate->has_image();
  window_->AddChild(overscroll_window_.get());

  gfx::Rect bounds = gfx::Rect(window_->bounds().size());
  if (ShouldNavigateForward(web_contents_->GetController(),
                            current_overscroll_gesture_)) {
    // The overlay will be sliding in from the right edge towards the left in
    // non-RTL, or sliding in from the left edge towards the right in RTL.
    // So position the overlay window accordingly.
    bounds.Offset(base::i18n::IsRTL() ? -bounds.width() : bounds.width(), 0);
  }

  aura::Window* animate_window = GetWindowToAnimateForOverscroll();
  if (animate_window == overscroll_window_)
    window_->StackChildAbove(overscroll_window_.get(), GetContentNativeView());
  else
    window_->StackChildBelow(overscroll_window_.get(), GetContentNativeView());

  UpdateOverscrollWindowBrightness(0.f);

  overscroll_window_->SetBounds(bounds);
  overscroll_window_->Show();

  overscroll_shadow_.reset(new ShadowLayerDelegate(animate_window->layer()));
}

void WebContentsViewAura::PrepareContentWindowForOverscroll() {
  StopObservingImplicitAnimations();
  aura::Window* content = GetContentNativeView();
  content->layer()->GetAnimator()->AbortAllAnimations();
  content->SetTransform(gfx::Transform());
  content->layer()->SetLayerBrightness(0.f);
}

void WebContentsViewAura::ResetOverscrollTransform() {
  if (!web_contents_->GetRenderWidgetHostView())
    return;
  aura::Window* target = GetWindowToAnimateForOverscroll();
  if (!target)
    return;
  {
    ui::ScopedLayerAnimationSettings settings(target->layer()->GetAnimator());
    settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    settings.SetTweenType(gfx::Tween::EASE_OUT);
    settings.AddObserver(this);
    target->SetTransform(gfx::Transform());
  }
  {
    ui::ScopedLayerAnimationSettings settings(target->layer()->GetAnimator());
    settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    settings.SetTweenType(gfx::Tween::EASE_OUT);
    UpdateOverscrollWindowBrightness(0.f);
  }
}

void WebContentsViewAura::CompleteOverscrollNavigation(OverscrollMode mode) {
  if (!web_contents_->GetRenderWidgetHostView())
    return;

  // Animate out the current view first. Navigate to the requested history at
  // the end of the animation.
  if (current_overscroll_gesture_ == OVERSCROLL_NONE)
    return;

  UMA_HISTOGRAM_ENUMERATION("Overscroll.Navigated",
                            current_overscroll_gesture_, OVERSCROLL_COUNT);
  OverscrollWindowDelegate* delegate = static_cast<OverscrollWindowDelegate*>(
      overscroll_window_->delegate());
  delegate->stop_forwarding_events();

  completed_overscroll_gesture_ = mode;
  aura::Window* target = GetWindowToAnimateForOverscroll();
  ui::ScopedLayerAnimationSettings settings(target->layer()->GetAnimator());
  settings.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  settings.SetTweenType(gfx::Tween::EASE_OUT);
  settings.AddObserver(this);
  gfx::Transform transform;
  int content_width =
      web_contents_->GetRenderWidgetHostView()->GetViewBounds().width();
  int translate_x = mode == OVERSCROLL_WEST ? -content_width : content_width;
  transform.Translate(translate_x, 0);
  target->SetTransform(transform);
  UpdateOverscrollWindowBrightness(translate_x);
}

aura::Window* WebContentsViewAura::GetWindowToAnimateForOverscroll() {
  if (current_overscroll_gesture_ == OVERSCROLL_NONE)
    return NULL;

  return ShouldNavigateForward(web_contents_->GetController(),
                               current_overscroll_gesture_) ?
      overscroll_window_.get() : GetContentNativeView();
}

gfx::Vector2d WebContentsViewAura::GetTranslationForOverscroll(int delta_x,
                                                               int delta_y) {
  if (current_overscroll_gesture_ == OVERSCROLL_NORTH ||
      current_overscroll_gesture_ == OVERSCROLL_SOUTH) {
    return gfx::Vector2d(0, delta_y);
  }
  // For horizontal overscroll, scroll freely if a navigation is possible. Do a
  // resistive scroll otherwise.
  const NavigationControllerImpl& controller = web_contents_->GetController();
  const gfx::Rect& bounds = GetViewBounds();
  if (ShouldNavigateForward(controller, current_overscroll_gesture_))
    return gfx::Vector2d(std::max(-bounds.width(), delta_x), 0);
  else if (ShouldNavigateBack(controller, current_overscroll_gesture_))
    return gfx::Vector2d(std::min(bounds.width(), delta_x), 0);
  return gfx::Vector2d();
}

void WebContentsViewAura::PrepareOverscrollNavigationOverlay() {
  OverscrollWindowDelegate* delegate = static_cast<OverscrollWindowDelegate*>(
      overscroll_window_->delegate());
  overscroll_window_->SchedulePaintInRect(
      gfx::Rect(overscroll_window_->bounds().size()));
  overscroll_window_->SetBounds(gfx::Rect(window_->bounds().size()));
  overscroll_window_->SetTransform(gfx::Transform());
  navigation_overlay_->SetOverlayWindow(overscroll_window_.Pass(),
                                        delegate);
  navigation_overlay_->StartObserving();
}

void WebContentsViewAura::UpdateOverscrollWindowBrightness(float delta_x) {
  if (!overscroll_change_brightness_)
    return;

  const float kBrightnessMin = -.1f;
  const float kBrightnessMax = -.01f;

  float ratio = fabs(delta_x) / GetViewBounds().width();
  ratio = std::min(1.f, ratio);
  if (base::i18n::IsRTL())
    ratio = 1.f - ratio;
  float brightness = current_overscroll_gesture_ == OVERSCROLL_WEST ?
      kBrightnessMin + ratio * (kBrightnessMax - kBrightnessMin) :
      kBrightnessMax - ratio * (kBrightnessMax - kBrightnessMin);
  brightness = std::max(kBrightnessMin, brightness);
  brightness = std::min(kBrightnessMax, brightness);
  aura::Window* window = GetWindowToAnimateForOverscroll();
  window->layer()->SetLayerBrightness(brightness);
}

void WebContentsViewAura::AttachTouchEditableToRenderView() {
  if (!touch_editable_)
    return;
  RenderWidgetHostViewAura* rwhva = ToRenderWidgetHostViewAura(
      web_contents_->GetRenderWidgetHostView());
  touch_editable_->AttachToView(rwhva);
}

void WebContentsViewAura::OverscrollUpdateForWebContentsDelegate(int delta_y) {
  if (web_contents_->GetDelegate() && IsScrollEndEffectEnabled())
    web_contents_->GetDelegate()->OverscrollUpdate(delta_y);
}

////////////////////////////////////////////////////////////////////////////////
// WebContentsViewAura, WebContentsView implementation:

gfx::NativeView WebContentsViewAura::GetNativeView() const {
  return window_.get();
}

gfx::NativeView WebContentsViewAura::GetContentNativeView() const {
  RenderWidgetHostView* rwhv = web_contents_->GetRenderWidgetHostView();
  return rwhv ? rwhv->GetNativeView() : NULL;
}

gfx::NativeWindow WebContentsViewAura::GetTopLevelNativeWindow() const {
  return window_->GetToplevelWindow();
}

void WebContentsViewAura::GetContainerBounds(gfx::Rect *out) const {
  *out = window_->GetBoundsInScreen();
}

void WebContentsViewAura::SizeContents(const gfx::Size& size) {
  gfx::Rect bounds = window_->bounds();
  if (bounds.size() != size) {
    bounds.set_size(size);
    window_->SetBounds(bounds);
  } else {
    // Our size matches what we want but the renderers size may not match.
    // Pretend we were resized so that the renderers size is updated too.
    SizeChangedCommon(size);
  }
}

void WebContentsViewAura::Focus() {
  if (web_contents_->GetInterstitialPage()) {
    web_contents_->GetInterstitialPage()->Focus();
    return;
  }

  if (delegate_.get() && delegate_->Focus())
    return;

  RenderWidgetHostView* rwhv = web_contents_->GetRenderWidgetHostView();
  if (rwhv)
    rwhv->Focus();
}

void WebContentsViewAura::SetInitialFocus() {
  if (web_contents_->FocusLocationBarByDefault())
    web_contents_->SetFocusToLocationBar(false);
  else
    Focus();
}

void WebContentsViewAura::StoreFocus() {
  if (delegate_)
    delegate_->StoreFocus();
}

void WebContentsViewAura::RestoreFocus() {
  if (delegate_)
    delegate_->RestoreFocus();
}

DropData* WebContentsViewAura::GetDropData() const {
  return current_drop_data_.get();
}

gfx::Rect WebContentsViewAura::GetViewBounds() const {
  return window_->GetBoundsInScreen();
}

////////////////////////////////////////////////////////////////////////////////
// WebContentsViewAura, WebContentsView implementation:

void WebContentsViewAura::CreateView(
    const gfx::Size& initial_size, gfx::NativeView context) {
  // NOTE: we ignore |initial_size| since in some cases it's wrong (such as
  // if the bookmark bar is not shown and you create a new tab). The right
  // value is set shortly after this, so its safe to ignore.

  aura::Env::CreateInstance(true);
  window_.reset(new aura::Window(this));
  window_->set_owned_by_parent(false);
  window_->SetType(ui::wm::WINDOW_TYPE_CONTROL);
  window_->SetTransparent(false);
  window_->Init(aura::WINDOW_LAYER_NOT_DRAWN);
  window_->AddObserver(this);
  aura::Window* root_window = context ? context->GetRootWindow() : NULL;
  if (root_window) {
    // There are places where there is no context currently because object
    // hierarchies are built before they're attached to a Widget. (See
    // views::WebView as an example; GetWidget() returns NULL at the point
    // where we are created.)
    //
    // It should be OK to not set a default parent since such users will
    // explicitly add this WebContentsViewAura to their tree after they create
    // us.
    if (root_window) {
      aura::client::ParentWindowWithContext(
          window_.get(), root_window, root_window->GetBoundsInScreen());
    }
  }
  window_->layer()->SetMasksToBounds(true);
  window_->SetName("WebContentsViewAura");

  // WindowObserver is not interesting and is problematic for Browser Plugin
  // guests.
  // The use cases for WindowObserver do not apply to Browser Plugins:
  // 1) guests do not support NPAPI plugins.
  // 2) guests' window bounds are supposed to come from its embedder.
  if (!BrowserPluginGuest::IsGuest(web_contents_))
    window_observer_.reset(new WindowObserver(this));

  // delegate_->GetDragDestDelegate() creates a new delegate on every call.
  // Hence, we save a reference to it locally. Similar model is used on other
  // platforms as well.
  if (delegate_)
    drag_dest_delegate_ = delegate_->GetDragDestDelegate();
}

RenderWidgetHostViewBase* WebContentsViewAura::CreateViewForWidget(
    RenderWidgetHost* render_widget_host) {
  if (render_widget_host->GetView()) {
    // During testing, the view will already be set up in most cases to the
    // test view, so we don't want to clobber it with a real one. To verify that
    // this actually is happening (and somebody isn't accidentally creating the
    // view twice), we check for the RVH Factory, which will be set when we're
    // making special ones (which go along with the special views).
    DCHECK(RenderViewHostFactory::has_factory());
    return static_cast<RenderWidgetHostViewBase*>(
        render_widget_host->GetView());
  }

  RenderWidgetHostViewAura* view =
      new RenderWidgetHostViewAura(render_widget_host);
  view->InitAsChild(NULL);
  GetNativeView()->AddChild(view->GetNativeView());

  if (navigation_overlay_.get() && navigation_overlay_->has_window()) {
    navigation_overlay_->StartObserving();
  }

  RenderWidgetHostImpl* host_impl =
      RenderWidgetHostImpl::From(render_widget_host);

  if (!host_impl->is_hidden())
    view->Show();

  // We listen to drag drop events in the newly created view's window.
  aura::client::SetDragDropDelegate(view->GetNativeView(), this);

  if (view->overscroll_controller() &&
      (!web_contents_->GetDelegate() ||
       web_contents_->GetDelegate()->CanOverscrollContent())) {
    InstallOverscrollControllerDelegate(view);
  }

  AttachTouchEditableToRenderView();
  return view;
}

RenderWidgetHostViewBase* WebContentsViewAura::CreateViewForPopupWidget(
    RenderWidgetHost* render_widget_host) {
  return new RenderWidgetHostViewAura(render_widget_host);
}

void WebContentsViewAura::SetPageTitle(const base::string16& title) {
  window_->set_title(title);
}

void WebContentsViewAura::RenderViewCreated(RenderViewHost* host) {
}

void WebContentsViewAura::RenderViewSwappedIn(RenderViewHost* host) {
  if (navigation_overlay_.get() && navigation_overlay_->has_window())
    navigation_overlay_->StartObserving();
  AttachTouchEditableToRenderView();
}

void WebContentsViewAura::SetOverscrollControllerEnabled(bool enabled) {
  RenderWidgetHostViewAura* view =
      ToRenderWidgetHostViewAura(web_contents_->GetRenderWidgetHostView());
  if (view) {
    view->SetOverscrollControllerEnabled(enabled);
    if (enabled)
      InstallOverscrollControllerDelegate(view);
  }

  if (!enabled)
    navigation_overlay_.reset();
  else if (!navigation_overlay_)
    navigation_overlay_.reset(new OverscrollNavigationOverlay(web_contents_));
}

////////////////////////////////////////////////////////////////////////////////
// WebContentsViewAura, RenderViewHostDelegateView implementation:

void WebContentsViewAura::ShowContextMenu(RenderFrameHost* render_frame_host,
                                          const ContextMenuParams& params) {
  if (touch_editable_) {
    touch_editable_->EndTouchEditing(false);
  }
  if (delegate_) {
    delegate_->ShowContextMenu(render_frame_host, params);
    // WARNING: we may have been deleted during the call to ShowContextMenu().
  }
}

void WebContentsViewAura::StartDragging(
    const DropData& drop_data,
    blink::WebDragOperationsMask operations,
    const gfx::ImageSkia& image,
    const gfx::Vector2d& image_offset,
    const DragEventSourceInfo& event_info) {
  aura::Window* root_window = GetNativeView()->GetRootWindow();
  if (!aura::client::GetDragDropClient(root_window)) {
    web_contents_->SystemDragEnded();
    return;
  }

  if (touch_editable_)
    touch_editable_->EndTouchEditing(false);

  ui::OSExchangeData::Provider* provider = ui::OSExchangeData::CreateProvider();
  PrepareDragData(drop_data, provider, web_contents_);

  ui::OSExchangeData data(provider);  // takes ownership of |provider|.

  if (!image.isNull())
    drag_utils::SetDragImageOnDataObject(image, image_offset, &data);

  scoped_ptr<WebDragSourceAura> drag_source(
      new WebDragSourceAura(GetNativeView(), web_contents_));

  // We need to enable recursive tasks on the message loop so we can get
  // updates while in the system DoDragDrop loop.
  int result_op = 0;
  {
    gfx::NativeView content_native_view = GetContentNativeView();
    base::MessageLoop::ScopedNestableTaskAllower allow(
        base::MessageLoop::current());
    result_op = aura::client::GetDragDropClient(root_window)
        ->StartDragAndDrop(data,
                           root_window,
                           content_native_view,
                           event_info.event_location,
                           ConvertFromWeb(operations),
                           event_info.event_source);
  }

  // Bail out immediately if the contents view window is gone. Note that it is
  // not safe to access any class members in this case since |this| may already
  // be destroyed. The local variable |drag_source| will still be valid though,
  // so we can use it to determine if the window is gone.
  if (!drag_source->window()) {
    // Note that in this case, we don't need to call SystemDragEnded() since the
    // renderer is going away.
    return;
  }

  EndDrag(ConvertToWeb(result_op));
  web_contents_->SystemDragEnded();
}

void WebContentsViewAura::UpdateDragCursor(blink::WebDragOperation operation) {
  current_drag_op_ = operation;
}

void WebContentsViewAura::GotFocus() {
  if (web_contents_->GetDelegate())
    web_contents_->GetDelegate()->WebContentsFocused(web_contents_);
}

void WebContentsViewAura::TakeFocus(bool reverse) {
  if (web_contents_->GetDelegate() &&
      !web_contents_->GetDelegate()->TakeFocus(web_contents_, reverse) &&
      delegate_.get()) {
    delegate_->TakeFocus(reverse);
  }
}

////////////////////////////////////////////////////////////////////////////////
// WebContentsViewAura, OverscrollControllerDelegate implementation:

gfx::Rect WebContentsViewAura::GetVisibleBounds() const {
  RenderWidgetHostView* rwhv = web_contents_->GetRenderWidgetHostView();
  if (!rwhv || !rwhv->IsShowing())
    return gfx::Rect();

  return rwhv->GetViewBounds();
}

void WebContentsViewAura::OnOverscrollUpdate(float delta_x, float delta_y) {
  if (current_overscroll_gesture_ == OVERSCROLL_NONE)
    return;

  aura::Window* target = GetWindowToAnimateForOverscroll();
  gfx::Vector2d translate = GetTranslationForOverscroll(delta_x, delta_y);
  gfx::Transform transform;

  // Vertical overscrolls don't participate in the navigation gesture.
  if (current_overscroll_gesture_ != OVERSCROLL_NORTH &&
      current_overscroll_gesture_ != OVERSCROLL_SOUTH) {
    transform.Translate(translate.x(), translate.y());
    target->SetTransform(transform);
    UpdateOverscrollWindowBrightness(delta_x);
  }

  OverscrollUpdateForWebContentsDelegate(translate.y());
}

void WebContentsViewAura::OnOverscrollComplete(OverscrollMode mode) {
  UMA_HISTOGRAM_ENUMERATION("Overscroll.Completed", mode, OVERSCROLL_COUNT);
  OverscrollUpdateForWebContentsDelegate(0);
  NavigationControllerImpl& controller = web_contents_->GetController();
  if (ShouldNavigateForward(controller, mode) ||
      ShouldNavigateBack(controller, mode)) {
    CompleteOverscrollNavigation(mode);
    return;
  }

  ResetOverscrollTransform();
}

void WebContentsViewAura::OnOverscrollModeChange(OverscrollMode old_mode,
                                                 OverscrollMode new_mode) {
  // Reset any in-progress overscroll animation first.
  ResetOverscrollTransform();

  if (new_mode != OVERSCROLL_NONE && touch_editable_)
    touch_editable_->OverscrollStarted();

  if (new_mode == OVERSCROLL_NONE ||
      !GetContentNativeView() ||
      ((new_mode == OVERSCROLL_EAST || new_mode == OVERSCROLL_WEST) &&
       navigation_overlay_.get() && navigation_overlay_->has_window())) {
    current_overscroll_gesture_ = OVERSCROLL_NONE;
    OverscrollUpdateForWebContentsDelegate(0);
  } else {
    aura::Window* target = GetWindowToAnimateForOverscroll();
    if (target) {
      StopObservingImplicitAnimations();
      target->layer()->GetAnimator()->AbortAllAnimations();
    }
    // Cleanup state of the content window first, because that can reset the
    // value of |current_overscroll_gesture_|.
    PrepareContentWindowForOverscroll();

    current_overscroll_gesture_ = new_mode;
    if (current_overscroll_gesture_ == OVERSCROLL_EAST ||
        current_overscroll_gesture_ == OVERSCROLL_WEST)
      PrepareOverscrollWindow();

    UMA_HISTOGRAM_ENUMERATION("Overscroll.Started", new_mode, OVERSCROLL_COUNT);
  }
  completed_overscroll_gesture_ = OVERSCROLL_NONE;
}

////////////////////////////////////////////////////////////////////////////////
// WebContentsViewAura, ui::ImplicitAnimationObserver implementation:

void WebContentsViewAura::OnImplicitAnimationsCompleted() {
  overscroll_shadow_.reset();

  if (ShouldNavigateForward(web_contents_->GetController(),
                            completed_overscroll_gesture_)) {
    web_contents_->GetController().GoForward();
    PrepareOverscrollNavigationOverlay();
  } else if (ShouldNavigateBack(web_contents_->GetController(),
                                completed_overscroll_gesture_)) {
    web_contents_->GetController().GoBack();
    PrepareOverscrollNavigationOverlay();
  } else {
    if (touch_editable_)
      touch_editable_->OverscrollCompleted();
  }

  aura::Window* content = GetContentNativeView();
  if (content) {
    content->SetTransform(gfx::Transform());
    content->layer()->SetLayerBrightness(0.f);
  }
  current_overscroll_gesture_ = OVERSCROLL_NONE;
  completed_overscroll_gesture_ = OVERSCROLL_NONE;
  overscroll_window_.reset();
}

////////////////////////////////////////////////////////////////////////////////
// WebContentsViewAura, aura::WindowDelegate implementation:

gfx::Size WebContentsViewAura::GetMinimumSize() const {
  return gfx::Size();
}

gfx::Size WebContentsViewAura::GetMaximumSize() const {
  return gfx::Size();
}

void WebContentsViewAura::OnBoundsChanged(const gfx::Rect& old_bounds,
                                          const gfx::Rect& new_bounds) {
  SizeChangedCommon(new_bounds.size());
  if (delegate_)
    delegate_->SizeChanged(new_bounds.size());

  // Constrained web dialogs, need to be kept centered over our content area.
  for (size_t i = 0; i < window_->children().size(); i++) {
    if (window_->children()[i]->GetProperty(
            aura::client::kConstrainedWindowKey)) {
      gfx::Rect bounds = window_->children()[i]->bounds();
      bounds.set_origin(
          gfx::Point((new_bounds.width() - bounds.width()) / 2,
                     (new_bounds.height() - bounds.height()) / 2));
      window_->children()[i]->SetBounds(bounds);
    }
  }
}

gfx::NativeCursor WebContentsViewAura::GetCursor(const gfx::Point& point) {
  return gfx::kNullCursor;
}

int WebContentsViewAura::GetNonClientComponent(const gfx::Point& point) const {
  return HTCLIENT;
}

bool WebContentsViewAura::ShouldDescendIntoChildForEventHandling(
    aura::Window* child,
    const gfx::Point& location) {
  return true;
}

bool WebContentsViewAura::CanFocus() {
  // Do not take the focus if the render widget host view aura is gone or
  // is in the process of shutting down because neither the view window nor
  // this window can handle key events.
  RenderWidgetHostViewAura* view = ToRenderWidgetHostViewAura(
      web_contents_->GetRenderWidgetHostView());
  if (view != NULL && !view->IsClosing())
    return true;

  return false;
}

void WebContentsViewAura::OnCaptureLost() {
}

void WebContentsViewAura::OnPaint(gfx::Canvas* canvas) {
}

void WebContentsViewAura::OnDeviceScaleFactorChanged(
    float device_scale_factor) {
}

void WebContentsViewAura::OnWindowDestroying(aura::Window* window) {
  // This means the destructor is going to be called soon. If there is an
  // overscroll gesture in progress (i.e. |overscroll_window_| is not NULL),
  // then destroying it in the WebContentsViewAura destructor can trigger other
  // virtual functions to be called (e.g. OnImplicitAnimationsCompleted()). So
  // destroy the overscroll window here.
  navigation_overlay_.reset();
  overscroll_window_.reset();
}

void WebContentsViewAura::OnWindowDestroyed(aura::Window* window) {
}

void WebContentsViewAura::OnWindowTargetVisibilityChanged(bool visible) {
}

bool WebContentsViewAura::HasHitTestMask() const {
  return false;
}

void WebContentsViewAura::GetHitTestMask(gfx::Path* mask) const {
}

////////////////////////////////////////////////////////////////////////////////
// WebContentsViewAura, ui::EventHandler implementation:

void WebContentsViewAura::OnKeyEvent(ui::KeyEvent* event) {
}

void WebContentsViewAura::OnMouseEvent(ui::MouseEvent* event) {
  if (!web_contents_->GetDelegate())
    return;

  switch (event->type()) {
    case ui::ET_MOUSE_PRESSED:
      web_contents_->GetDelegate()->ActivateContents(web_contents_);
      break;
    case ui::ET_MOUSE_MOVED:
    case ui::ET_MOUSE_EXITED:
      web_contents_->GetDelegate()->ContentsMouseEvent(
          web_contents_,
          gfx::Screen::GetScreenFor(GetNativeView())->GetCursorScreenPoint(),
          event->type() == ui::ET_MOUSE_MOVED);
      break;
    default:
      break;
  }
}

////////////////////////////////////////////////////////////////////////////////
// WebContentsViewAura, aura::client::DragDropDelegate implementation:

void WebContentsViewAura::OnDragEntered(const ui::DropTargetEvent& event) {
  current_rvh_for_drag_ = web_contents_->GetRenderViewHost();
  current_drop_data_.reset(new DropData());

  PrepareDropData(current_drop_data_.get(), event.data());
  blink::WebDragOperationsMask op = ConvertToWeb(event.source_operations());

  // Give the delegate an opportunity to cancel the drag.
  if (!web_contents_->GetDelegate()->CanDragEnter(web_contents_,
                                                  *current_drop_data_.get(),
                                                  op)) {
    current_drop_data_.reset(NULL);
    return;
  }

  if (drag_dest_delegate_)
    drag_dest_delegate_->DragInitialize(web_contents_);

  gfx::Point screen_pt =
      gfx::Screen::GetScreenFor(GetNativeView())->GetCursorScreenPoint();
  web_contents_->GetRenderViewHost()->DragTargetDragEnter(
      *current_drop_data_.get(), event.location(), screen_pt, op,
      ConvertAuraEventFlagsToWebInputEventModifiers(event.flags()));

  if (drag_dest_delegate_) {
    drag_dest_delegate_->OnReceiveDragData(event.data());
    drag_dest_delegate_->OnDragEnter();
  }
}

int WebContentsViewAura::OnDragUpdated(const ui::DropTargetEvent& event) {
  DCHECK(current_rvh_for_drag_);
  if (current_rvh_for_drag_ != web_contents_->GetRenderViewHost())
    OnDragEntered(event);

  if (!current_drop_data_)
    return ui::DragDropTypes::DRAG_NONE;

  blink::WebDragOperationsMask op = ConvertToWeb(event.source_operations());
  gfx::Point screen_pt =
      gfx::Screen::GetScreenFor(GetNativeView())->GetCursorScreenPoint();
  web_contents_->GetRenderViewHost()->DragTargetDragOver(
      event.location(), screen_pt, op,
      ConvertAuraEventFlagsToWebInputEventModifiers(event.flags()));

  if (drag_dest_delegate_)
    drag_dest_delegate_->OnDragOver();

  return ConvertFromWeb(current_drag_op_);
}

void WebContentsViewAura::OnDragExited() {
  DCHECK(current_rvh_for_drag_);
  if (current_rvh_for_drag_ != web_contents_->GetRenderViewHost())
    return;

  if (!current_drop_data_)
    return;

  web_contents_->GetRenderViewHost()->DragTargetDragLeave();
  if (drag_dest_delegate_)
    drag_dest_delegate_->OnDragLeave();

  current_drop_data_.reset();
}

int WebContentsViewAura::OnPerformDrop(const ui::DropTargetEvent& event) {
  DCHECK(current_rvh_for_drag_);
  if (current_rvh_for_drag_ != web_contents_->GetRenderViewHost())
    OnDragEntered(event);

  if (!current_drop_data_)
    return ui::DragDropTypes::DRAG_NONE;

  web_contents_->GetRenderViewHost()->DragTargetDrop(
      event.location(),
      gfx::Screen::GetScreenFor(GetNativeView())->GetCursorScreenPoint(),
      ConvertAuraEventFlagsToWebInputEventModifiers(event.flags()));
  if (drag_dest_delegate_)
    drag_dest_delegate_->OnDrop();
  current_drop_data_.reset();
  return ConvertFromWeb(current_drag_op_);
}

void WebContentsViewAura::OnWindowParentChanged(aura::Window* window,
                                                aura::Window* parent) {
  // On Windows we will get called with a parent of NULL as part of the shut
  // down process. As such we do only change the visibility when a parent gets
  // set.
  if (parent)
    UpdateWebContentsVisibility(web_contents_, window->IsVisible());
}

void WebContentsViewAura::OnWindowVisibilityChanged(aura::Window* window,
                                                    bool visible) {
  // Ignore any visibility changes in the hierarchy below.
  if (window != window_.get() && window_->Contains(window))
    return;

  UpdateWebContentsVisibility(web_contents_, visible);
}

}  // namespace content

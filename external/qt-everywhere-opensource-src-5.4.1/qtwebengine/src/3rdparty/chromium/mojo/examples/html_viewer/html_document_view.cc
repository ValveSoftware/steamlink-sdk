// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/examples/html_viewer/html_document_view.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "mojo/services/public/cpp/view_manager/node.h"
#include "mojo/services/public/cpp/view_manager/view.h"
#include "skia/ext/refptr.h"
#include "third_party/WebKit/public/web/WebConsoleMessage.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebScriptSource.h"
#include "third_party/WebKit/public/web/WebSettings.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkDevice.h"

namespace mojo {
namespace examples {
namespace {

blink::WebData CopyToWebData(DataPipeConsumerHandle handle) {
  std::vector<char> data;
  for (;;) {
    char buf[4096];
    uint32_t num_bytes = sizeof(buf);
    MojoResult result = ReadDataRaw(
        handle,
        buf,
        &num_bytes,
        MOJO_READ_DATA_FLAG_NONE);
    if (result == MOJO_RESULT_SHOULD_WAIT) {
      Wait(handle,
           MOJO_HANDLE_SIGNAL_READABLE,
           MOJO_DEADLINE_INDEFINITE);
    } else if (result == MOJO_RESULT_OK) {
      data.insert(data.end(), buf, buf + num_bytes);
    } else {
      break;
    }
  }
  return blink::WebData(data);
}

void ConfigureSettings(blink::WebSettings* settings) {
  settings->setAcceleratedCompositingEnabled(false);
  settings->setLoadsImagesAutomatically(true);
  settings->setJavaScriptEnabled(true);
}

}  // namespace

HTMLDocumentView::HTMLDocumentView(view_manager::ViewManager* view_manager)
    : view_manager_(view_manager),
      view_(view_manager::View::Create(view_manager_)),
      web_view_(NULL),
      repaint_pending_(false),
      weak_factory_(this) {
}

HTMLDocumentView::~HTMLDocumentView() {
  if (web_view_)
    web_view_->close();
}

void HTMLDocumentView::AttachToNode(view_manager::Node* node) {
  node->SetActiveView(view_);
  view_->SetColor(SK_ColorCYAN);  // Dummy background color.

  web_view_ = blink::WebView::create(this);
  ConfigureSettings(web_view_->settings());
  web_view_->setMainFrame(blink::WebLocalFrame::create(this));

  // TODO(darin): Track size of view_manager::Node.
  web_view_->resize(gfx::Size(800, 600));
}

void HTMLDocumentView::Load(URLResponsePtr response,
                            ScopedDataPipeConsumerHandle response_body_stream) {
  DCHECK(web_view_);

  // TODO(darin): A better solution would be to use loadRequest, but intercept
  // the network request and connect it to the response we already have.
  blink::WebData data = CopyToWebData(response_body_stream.get());
  web_view_->mainFrame()->loadHTMLString(
      data, GURL(response->url), GURL(response->url));
}

void HTMLDocumentView::didInvalidateRect(const blink::WebRect& rect) {
  if (!repaint_pending_) {
    repaint_pending_ = true;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&HTMLDocumentView::Repaint, weak_factory_.GetWeakPtr()));
  }
}

bool HTMLDocumentView::allowsBrokenNullLayerTreeView() const {
  // TODO(darin): Switch to using compositor bindings.
  //
  // NOTE: Note to Blink maintainers, feel free to just break this code if it
  // is the last using compositor bindings and you want to delete the old path.
  //
  return true;
}

void HTMLDocumentView::didAddMessageToConsole(
    const blink::WebConsoleMessage& message,
    const blink::WebString& source_name,
    unsigned source_line,
    const blink::WebString& stack_trace) {
  printf("### console: %s\n", std::string(message.text.utf8()).c_str());
}

void HTMLDocumentView::Repaint() {
  repaint_pending_ = false;

  web_view_->animate(0.0);
  web_view_->layout();

  int width = web_view_->size().width;
  int height = web_view_->size().height;

  skia::RefPtr<SkCanvas> canvas = skia::AdoptRef(SkCanvas::NewRaster(
      SkImageInfo::MakeN32(width, height, kOpaque_SkAlphaType)));

  web_view_->paint(canvas.get(), gfx::Rect(0, 0, width, height));

  view_->SetContents(canvas->getDevice()->accessBitmap(false));
}

}  // namespace examples
}  // namespace mojo

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONSTRAINED_WINDOW_CONSTRAINED_WINDOW_VIEWS_H_
#define COMPONENTS_CONSTRAINED_WINDOW_CONSTRAINED_WINDOW_VIEWS_H_

#include <memory>

#include "ui/gfx/native_widget_types.h"

namespace content {
class WebContents;
}

namespace views {
class DialogDelegate;
class Widget;
class WidgetDelegate;
}

namespace web_modal {
class ModalDialogHost;
class WebContentsModalDialogHost;
}

namespace constrained_window {

class ConstrainedWindowViewsClient;

// Sets the ConstrainedWindowClient impl.
void SetConstrainedWindowViewsClient(
    std::unique_ptr<ConstrainedWindowViewsClient> client);

// Update the position of dialog |widget| against |dialog_host|. This is used to
// reposition widgets e.g. when the host dimensions change.
void UpdateWebContentsModalDialogPosition(
    views::Widget* widget,
    web_modal::WebContentsModalDialogHost* dialog_host);

void UpdateWidgetModalDialogPosition(
    views::Widget* widget,
    web_modal::ModalDialogHost* dialog_host);

// Calls CreateWebModalDialogViews, shows the dialog, and returns its widget.
views::Widget* ShowWebModalDialogViews(
    views::WidgetDelegate* dialog,
    content::WebContents* initiator_web_contents);

// Create a widget for |dialog| that is modal to |web_contents|.
// The modal type of |dialog->GetModalType()| must be ui::MODAL_TYPE_CHILD.
views::Widget* CreateWebModalDialogViews(views::WidgetDelegate* dialog,
                                         content::WebContents* web_contents);

// Create a widget for |dialog| that has a modality given by
// |dialog->GetModalType()|.  The modal type must be either
// ui::MODAL_TYPE_SYSTEM or ui::MODAL_TYPE_WINDOW.  This places the
// dialog appropriately if |parent| is a valid browser window.
views::Widget* CreateBrowserModalDialogViews(views::DialogDelegate* dialog,
                                             gfx::NativeWindow parent);

}  // namespace constrained window

#endif  // COMPONENTS_CONSTRAINED_WINDOW_CONSTRAINED_WINDOW_VIEWS_H_

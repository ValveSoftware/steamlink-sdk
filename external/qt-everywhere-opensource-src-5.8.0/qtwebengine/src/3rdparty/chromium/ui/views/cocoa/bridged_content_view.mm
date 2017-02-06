// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/views/cocoa/bridged_content_view.h"

#include "base/logging.h"
#import "base/mac/mac_util.h"
#import "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "skia/ext/skia_utils_mac.h"
#include "ui/base/cocoa/cocoa_base_utils.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/os_exchange_data_provider_mac.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_edit_commands.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/compositor/canvas_painter.h"
#import "ui/events/cocoa/cocoa_event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#import "ui/events/keycodes/keyboard_code_conversion_mac.h"
#include "ui/gfx/canvas_paint_mac.h"
#include "ui/gfx/geometry/rect.h"
#import "ui/gfx/mac/coordinate_conversion.h"
#include "ui/gfx/path.h"
#import "ui/gfx/path_mac.h"
#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"
#import "ui/views/cocoa/bridged_native_widget.h"
#import "ui/views/cocoa/drag_drop_client_mac.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/view.h"
#include "ui/views/widget/native_widget_mac.h"
#include "ui/views/widget/widget.h"

using views::MenuController;

namespace {

NSString* const kFullKeyboardAccessChangedNotification =
    @"com.apple.KeyboardUIModeDidChange";

// Returns true if all four corners of |rect| are contained inside |path|.
bool IsRectInsidePath(NSRect rect, NSBezierPath* path) {
  return [path containsPoint:rect.origin] &&
         [path containsPoint:NSMakePoint(rect.origin.x + rect.size.width,
                                         rect.origin.y)] &&
         [path containsPoint:NSMakePoint(rect.origin.x,
                                         rect.origin.y + rect.size.height)] &&
         [path containsPoint:NSMakePoint(rect.origin.x + rect.size.width,
                                         rect.origin.y + rect.size.height)];
}

// Convert a |point| in |source_window|'s AppKit coordinate system (origin at
// the bottom left of the window) to |target_window|'s content rect, with the
// origin at the top left of the content area.
// If |source_window| is nil, |point| will be treated as screen coordinates.
gfx::Point MovePointToWindow(const NSPoint& point,
                             NSWindow* source_window,
                             NSWindow* target_window) {
  NSPoint point_in_screen = source_window
      ? ui::ConvertPointFromWindowToScreen(source_window, point)
      : point;

  NSPoint point_in_window =
      ui::ConvertPointFromScreenToWindow(target_window, point_in_screen);
  NSRect content_rect =
      [target_window contentRectForFrameRect:[target_window frame]];
  return gfx::Point(point_in_window.x,
                    NSHeight(content_rect) - point_in_window.y);
}

// Dispatch |event| to |menu_controller| and return true if |event| is
// swallowed.
bool DispatchEventToMenu(MenuController* menu_controller, ui::KeyEvent* event) {
  return menu_controller &&
         menu_controller->OnWillDispatchKeyEvent(event) ==
             ui::POST_DISPATCH_NONE;
}

// Returns true if |client| has RTL text.
bool IsTextRTL(const ui::TextInputClient* client) {
  return client && client->GetTextDirection() == base::i18n::RIGHT_TO_LEFT;
}

// Returns the boundary rectangle for composition characters in the
// |requested_range|. Sets |actual_range| corresponding to the returned
// rectangle. For cases, where there is no composition text or the
// |requested_range| lies outside the composition range, a zero width rectangle
// corresponding to the caret bounds is returned. Logic used is similar to
// RenderWidgetHostViewMac::GetCachedFirstRectForCharacterRange(...).
gfx::Rect GetFirstRectForRangeHelper(const ui::TextInputClient* client,
                                     const gfx::Range& requested_range,
                                     gfx::Range* actual_range) {
  // NSRange doesn't support reversed ranges.
  DCHECK(!requested_range.is_reversed());
  DCHECK(actual_range);

  // Set up default return values, to be returned in case of unusual cases.
  gfx::Rect default_rect;
  *actual_range = gfx::Range::InvalidRange();
  if (!client)
    return default_rect;

  default_rect = client->GetCaretBounds();
  default_rect.set_width(0);

  // If possible, modify actual_range to correspond to caret position.
  gfx::Range selection_range;
  if (client->GetSelectionRange(&selection_range)) {
    // Caret bounds correspond to end index of selection_range.
    *actual_range = gfx::Range(selection_range.end());
  }

  gfx::Range composition_range;
  if (!client->HasCompositionText() ||
      !client->GetCompositionTextRange(&composition_range) ||
      !composition_range.Contains(requested_range))
    return default_rect;

  DCHECK(!composition_range.is_reversed());

  const size_t from = requested_range.start() - composition_range.start();
  const size_t to = requested_range.end() - composition_range.start();

  // Pick the first character's bounds as the initial rectangle, then grow it to
  // the full |requested_range| if possible.
  const bool request_is_composition_end = from == composition_range.length();
  const size_t first_index = request_is_composition_end ? from - 1 : from;
  gfx::Rect union_rect;
  if (!client->GetCompositionCharacterBounds(first_index, &union_rect))
    return default_rect;

  // If requested_range is empty, return a zero width rectangle corresponding to
  // it.
  if (from == to) {
    if (request_is_composition_end && !IsTextRTL(client)) {
      // In case of an empty requested range at end of composition, return the
      // rectangle to the right of the last compositioned character.
      union_rect.set_origin(union_rect.top_right());
    }
    union_rect.set_width(0);
    *actual_range = requested_range;
    return union_rect;
  }

  // Toolkit-views textfields are always single-line, so no need to check for
  // line breaks.
  for (size_t i = from + 1; i < to; i++) {
    gfx::Rect current_rect;
    if (client->GetCompositionCharacterBounds(i, &current_rect)) {
      union_rect.Union(current_rect);
    } else {
      *actual_range =
          gfx::Range(requested_range.start(), i + composition_range.start());
      return union_rect;
    }
  }
  *actual_range = requested_range;
  return union_rect;
}

// Returns the string corresponding to |requested_range| for the given |client|.
// If a gfx::Range::InvalidRange() is passed, the full string stored by |client|
// is returned. Sets |actual_range| corresponding to the returned string.
base::string16 AttributedSubstringForRangeHelper(
    const ui::TextInputClient* client,
    const gfx::Range& requested_range,
    gfx::Range* actual_range) {
  // NSRange doesn't support reversed ranges.
  DCHECK(!requested_range.is_reversed());
  DCHECK(actual_range);

  base::string16 substring;
  gfx::Range text_range;
  *actual_range = gfx::Range::InvalidRange();
  if (!client || !client->GetTextRange(&text_range))
    return substring;

  // gfx::Range::Intersect() behaves a bit weirdly. If B is an empty range
  // contained inside a non-empty range A, B intersection A returns
  // gfx::Range::InvalidRange(), instead of returning B.
  *actual_range = text_range.Contains(requested_range)
                      ? requested_range
                      : text_range.Intersect(requested_range);

  // This is a special case for which the complete string should should be
  // returned. NSTextView also follows this, though the same is not mentioned in
  // NSTextInputClient documentation.
  if (!requested_range.IsValid())
    *actual_range = text_range;

  client->GetTextFromRange(*actual_range, &substring);
  return substring;
}

// Returns a character event corresponding to |event|. |event| must be a
// character event itself.
ui::KeyEvent GetCharacterEventFromNSEvent(NSEvent* event) {
  DCHECK([event type] == NSKeyDown || [event type] == NSKeyUp);
  DCHECK_EQ(1u, [[event characters] length]);

  // [NSEvent characters] already considers the pressed key modifiers. Hence
  // send ui::EF_NONE as the key modifier to the KeyEvent constructor.
  // E.g. For Alt+S, [NSEvent characters] is 'ß' and not 'S'.
  return ui::KeyEvent([[event characters] characterAtIndex:0],
                      ui::KeyboardCodeFromNSEvent(event), ui::EF_NONE);
}

}  // namespace

@interface BridgedContentView ()

// Returns the active menu controller corresponding to |hostedView_|,
// nil otherwise.
- (MenuController*)activeMenuController;

// Passes |event| to the InputMethod for dispatch.
- (void)handleKeyEvent:(ui::KeyEvent*)event;

// Handles an NSResponder Action Message by mapping it to a corresponding text
// editing command from ui_strings.grd and, when not being sent to a
// TextInputClient, the keyCode that toolkit-views expects internally.
// For example, moveToLeftEndOfLine: would pass ui::VKEY_HOME in non-RTL locales
// even though the Home key on Mac defaults to moveToBeginningOfDocument:.
// This approach also allows action messages a user
// may have remapped in ~/Library/KeyBindings/DefaultKeyBinding.dict to be
// catered for.
// Note: default key bindings in Mac can be read from StandardKeyBinding.dict
// which lives in /System/Library/Frameworks/AppKit.framework/Resources. Do
// `plutil -convert xml1 -o StandardKeyBinding.xml StandardKeyBinding.dict` to
// get something readable.
- (void)handleAction:(ui::TextEditCommand)command
             keyCode:(ui::KeyboardCode)keyCode
             domCode:(ui::DomCode)domCode
          eventFlags:(int)eventFlags;

// Notification handler invoked when the Full Keyboard Access mode is changed.
- (void)onFullKeyboardAccessModeChanged:(NSNotification*)notification;

// Helper method which forwards |text| to the active menu or |textInputClient_|.
- (void)insertTextInternal:(id)text;

// Returns the native Widget's drag drop client. Possibly null.
- (views::DragDropClientMac*)dragDropClient;

// Menu action handlers.
- (void)undo:(id)sender;
- (void)redo:(id)sender;
- (void)cut:(id)sender;
- (void)copy:(id)sender;
- (void)paste:(id)sender;
- (void)selectAll:(id)sender;

@end

@implementation BridgedContentView

@synthesize hostedView = hostedView_;
@synthesize textInputClient = textInputClient_;
@synthesize drawMenuBackgroundForBlur = drawMenuBackgroundForBlur_;
@synthesize mouseDownCanMoveWindow = mouseDownCanMoveWindow_;

- (id)initWithView:(views::View*)viewToHost {
  DCHECK(viewToHost);
  gfx::Rect bounds = viewToHost->bounds();
  // To keep things simple, assume the origin is (0, 0) until there exists a use
  // case for something other than that.
  DCHECK(bounds.origin().IsOrigin());
  NSRect initialFrame = NSMakeRect(0, 0, bounds.width(), bounds.height());
  if ((self = [super initWithFrame:initialFrame])) {
    hostedView_ = viewToHost;

    // Apple's documentation says that NSTrackingActiveAlways is incompatible
    // with NSTrackingCursorUpdate, so use NSTrackingActiveInActiveApp.
    cursorTrackingArea_.reset([[CrTrackingArea alloc]
        initWithRect:NSZeroRect
             options:NSTrackingMouseMoved | NSTrackingCursorUpdate |
                     NSTrackingActiveInActiveApp | NSTrackingInVisibleRect
               owner:self
            userInfo:nil]);
    [self addTrackingArea:cursorTrackingArea_.get()];

    // Get notified whenever Full Keyboard Access mode is changed.
    [[NSDistributedNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(onFullKeyboardAccessModeChanged:)
               name:kFullKeyboardAccessChangedNotification
             object:nil];

    // Initialize the focus manager with the correct keyboard accessibility
    // setting.
    [self updateFullKeyboardAccess];
    [self registerForDraggedTypes:ui::OSExchangeDataProviderMac::
                                      SupportedPasteboardTypes()];
  }
  return self;
}

- (void)clearView {
  textInputClient_ = nullptr;
  hostedView_ = nullptr;
  [[NSDistributedNotificationCenter defaultCenter] removeObserver:self];
  [cursorTrackingArea_.get() clearOwner];
  [self removeTrackingArea:cursorTrackingArea_.get()];
}

- (void)processCapturedMouseEvent:(NSEvent*)theEvent {
  if (!hostedView_)
    return;

  NSWindow* source = [theEvent window];
  NSWindow* target = [self window];
  DCHECK(target);

  // If it's the view's window, process normally.
  if ([target isEqual:source]) {
    [self mouseEvent:theEvent];
    return;
  }

  ui::MouseEvent event(theEvent);
  event.set_location(
      MovePointToWindow([theEvent locationInWindow], source, target));
  hostedView_->GetWidget()->OnMouseEvent(&event);
}

- (void)updateTooltipIfRequiredAt:(const gfx::Point&)locationInContent {
  DCHECK(hostedView_);
  base::string16 newTooltipText;

  views::View* view = hostedView_->GetTooltipHandlerForPoint(locationInContent);
  if (view) {
    gfx::Point viewPoint = locationInContent;
    views::View::ConvertPointToTarget(hostedView_, view, &viewPoint);
    if (!view->GetTooltipText(viewPoint, &newTooltipText))
      DCHECK(newTooltipText.empty());
  }
  if (newTooltipText != lastTooltipText_) {
    std::swap(newTooltipText, lastTooltipText_);
    [self setToolTipAtMousePoint:base::SysUTF16ToNSString(lastTooltipText_)];
  }
}

- (void)updateWindowMask {
  DCHECK(![self inLiveResize]);
  DCHECK(base::mac::IsOSMavericks());
  DCHECK(hostedView_);

  views::Widget* widget = hostedView_->GetWidget();
  if (!widget->non_client_view())
    return;

  const NSRect frameRect = [self bounds];
  gfx::Path mask;
  widget->non_client_view()->GetWindowMask(gfx::Size(frameRect.size), &mask);
  if (mask.isEmpty())
    return;

  windowMask_.reset([gfx::CreateNSBezierPathFromSkPath(mask) retain]);

  // Convert to AppKit coordinate system.
  NSAffineTransform* flipTransform = [NSAffineTransform transform];
  [flipTransform translateXBy:0.0 yBy:frameRect.size.height];
  [flipTransform scaleXBy:1.0 yBy:-1.0];
  [windowMask_ transformUsingAffineTransform:flipTransform];
}

- (void)updateFullKeyboardAccess {
  if (!hostedView_)
    return;

  views::FocusManager* focusManager =
      hostedView_->GetWidget()->GetFocusManager();
  if (focusManager)
    focusManager->SetKeyboardAccessible([NSApp isFullKeyboardAccessEnabled]);
}

// BridgedContentView private implementation.

- (MenuController*)activeMenuController {
  MenuController* menuController = MenuController::GetActiveInstance();
  return menuController && menuController->owner() == hostedView_->GetWidget()
             ? menuController
             : nullptr;
}

- (void)handleKeyEvent:(ui::KeyEvent*)event {
  if (!hostedView_)
    return;

  DCHECK(event);
  if (DispatchEventToMenu([self activeMenuController], event))
    return;

  hostedView_->GetWidget()->GetInputMethod()->DispatchKeyEvent(event);
}

- (void)handleAction:(ui::TextEditCommand)command
             keyCode:(ui::KeyboardCode)keyCode
             domCode:(ui::DomCode)domCode
          eventFlags:(int)eventFlags {
  if (!hostedView_)
    return;

  // Generate a synthetic event with the keycode toolkit-views expects.
  ui::KeyEvent event(ui::ET_KEY_PRESSED, keyCode, domCode, eventFlags);

  if (DispatchEventToMenu([self activeMenuController], &event))
    return;

  // If there's an active TextInputClient, schedule the editing command to be
  // performed.
  if (textInputClient_ && textInputClient_->IsTextEditCommandEnabled(command))
    textInputClient_->SetTextEditCommandForNextKeyEvent(command);

  hostedView_->GetWidget()->GetInputMethod()->DispatchKeyEvent(&event);
}

- (void)onFullKeyboardAccessModeChanged:(NSNotification*)notification {
  DCHECK([[notification name]
      isEqualToString:kFullKeyboardAccessChangedNotification]);
  [self updateFullKeyboardAccess];
}

- (void)insertTextInternal:(id)text {
  if (!hostedView_)
    return;

  if ([text isKindOfClass:[NSAttributedString class]])
    text = [text string];

  bool isCharacterEvent = keyDownEvent_ && [text length] == 1;

  // Forward the |text| to |textInputClient_| if no menu is active.
  if (textInputClient_ && ![self activeMenuController]) {
    // If a single character is inserted by keyDown's call to
    // interpretKeyEvents: then use InsertChar() to allow editing events to be
    // merged.
    if (isCharacterEvent)
      textInputClient_->InsertChar(GetCharacterEventFromNSEvent(keyDownEvent_));
    else
      textInputClient_->InsertText(base::SysNSStringToUTF16(text));
    return;
  }

  // Only handle the case where no. of characters is 1. Cases not handled (not
  // an exhaustive list):
  // - |text| contains a unicode surrogate pair, i.e. a single grapheme which
  //   requires two 16 bit characters. Currently Views menu only supports
  //   mnemonics using a single 16 bit character, so it is ok to ignore this
  //   case.
  // - Programmatically created events.
  // - Input from IME. But this case should not occur since inputContext is
  //   nil.
  if (isCharacterEvent) {
    ui::KeyEvent charEvent = GetCharacterEventFromNSEvent(keyDownEvent_);
    [self handleKeyEvent:&charEvent];
  }
}

- (views::DragDropClientMac*)dragDropClient {
  views::BridgedNativeWidget* bridge =
      views::NativeWidgetMac::GetBridgeForNativeWindow([self window]);
  return bridge ? bridge->drag_drop_client() : nullptr;
}

- (void)undo:(id)sender {
  // This DCHECK is more strict than a similar check in handleAction:. It can be
  // done here because the actors sending these actions should be calling
  // validateUserInterfaceItem: before enabling UI that allows these messages to
  // be sent. Checking it here would be too late to provide correct UI feedback
  // (e.g. there will be no "beep").
  DCHECK(textInputClient_->IsTextEditCommandEnabled(ui::TextEditCommand::UNDO));
  [self handleAction:ui::TextEditCommand::UNDO
             keyCode:ui::VKEY_Z
             domCode:ui::DomCode::US_Z
          eventFlags:ui::EF_CONTROL_DOWN];
}

- (void)redo:(id)sender {
  DCHECK(textInputClient_->IsTextEditCommandEnabled(ui::TextEditCommand::REDO));
  [self handleAction:ui::TextEditCommand::REDO
             keyCode:ui::VKEY_Z
             domCode:ui::DomCode::US_Z
          eventFlags:ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN];
}

- (void)cut:(id)sender {
  DCHECK(textInputClient_->IsTextEditCommandEnabled(ui::TextEditCommand::CUT));
  [self handleAction:ui::TextEditCommand::CUT
             keyCode:ui::VKEY_X
             domCode:ui::DomCode::US_X
          eventFlags:ui::EF_CONTROL_DOWN];
}

- (void)copy:(id)sender {
  DCHECK(textInputClient_->IsTextEditCommandEnabled(ui::TextEditCommand::COPY));
  [self handleAction:ui::TextEditCommand::COPY
             keyCode:ui::VKEY_C
             domCode:ui::DomCode::US_C
          eventFlags:ui::EF_CONTROL_DOWN];
}

- (void)paste:(id)sender {
  DCHECK(
      textInputClient_->IsTextEditCommandEnabled(ui::TextEditCommand::PASTE));
  [self handleAction:ui::TextEditCommand::PASTE
             keyCode:ui::VKEY_V
             domCode:ui::DomCode::US_V
          eventFlags:ui::EF_CONTROL_DOWN];
}

- (void)selectAll:(id)sender {
  DCHECK(textInputClient_->IsTextEditCommandEnabled(
      ui::TextEditCommand::SELECT_ALL));
  [self handleAction:ui::TextEditCommand::SELECT_ALL
             keyCode:ui::VKEY_A
             domCode:ui::DomCode::US_A
          eventFlags:ui::EF_CONTROL_DOWN];
}

// BaseView implementation.

// Don't use tracking areas from BaseView. BridgedContentView's tracks
// NSTrackingCursorUpdate and Apple's documentation suggests it's incompatible.
- (void)enableTracking {
}

// Translates the location of |theEvent| to toolkit-views coordinates and passes
// the event to NativeWidgetMac for handling.
- (void)mouseEvent:(NSEvent*)theEvent {
  if (!hostedView_)
    return;

  ui::MouseEvent event(theEvent);

  // Aura updates tooltips with the help of aura::Window::AddPreTargetHandler().
  // Mac hooks in here.
  [self updateTooltipIfRequiredAt:event.location()];

  hostedView_->GetWidget()->OnMouseEvent(&event);
}

// NSView implementation.

- (BOOL)acceptsFirstResponder {
  return YES;
}

- (BOOL)becomeFirstResponder {
  BOOL result = [super becomeFirstResponder];
  if (result && hostedView_)
    hostedView_->GetWidget()->GetFocusManager()->RestoreFocusedView();
  return result;
}

- (BOOL)resignFirstResponder {
  BOOL result = [super resignFirstResponder];
  if (result && hostedView_)
    hostedView_->GetWidget()->GetFocusManager()->StoreFocusedView(true);
  return result;
}

- (void)viewDidMoveToWindow {
  // When this view is added to a window, AppKit calls setFrameSize before it is
  // added to the window, so the behavior in setFrameSize is not triggered.
  NSWindow* window = [self window];
  if (window)
    [self setFrameSize:NSZeroSize];
}

- (void)setFrameSize:(NSSize)newSize {
  // The size passed in here does not always use
  // -[NSWindow contentRectForFrameRect]. The following ensures that the
  // contentView for a frameless window can extend over the titlebar of the new
  // window containing it, since AppKit requires a titlebar to give frameless
  // windows correct shadows and rounded corners.
  NSWindow* window = [self window];
  if (window)
    newSize = [window contentRectForFrameRect:[window frame]].size;

  [super setFrameSize:newSize];
  if (!hostedView_)
    return;

  hostedView_->SetSize(gfx::Size(newSize.width, newSize.height));
}

- (void)viewDidEndLiveResize {
  [super viewDidEndLiveResize];

  // We prevent updating the window mask and clipping the border around the
  // view, during a live resize. Hence update the window mask and redraw the
  // view after resize has completed.
  if (base::mac::IsOSMavericks()) {
    [self updateWindowMask];
    [self setNeedsDisplay:YES];
  }
}

- (void)drawRect:(NSRect)dirtyRect {
  // Note that BridgedNativeWidget uses -[NSWindow setAutodisplay:NO] to
  // suppress calls to this when the window is known to be hidden.
  if (!hostedView_)
    return;

  if (drawMenuBackgroundForBlur_) {
    const CGFloat radius = views::MenuConfig::instance().corner_radius;
    [skia::SkColorToSRGBNSColor(0x01000000) set];
    [[NSBezierPath bezierPathWithRoundedRect:[self bounds]
                                     xRadius:radius
                                     yRadius:radius] fill];
  }

  // On OS versions earlier than Yosemite, to generate a drop shadow, we set an
  // opaque background. This causes windows with non rectangular shapes to have
  // square corners. To get around this, fill the path outside the window
  // boundary with clearColor and tell Cococa to regenerate drop shadow. See
  // crbug.com/543671.
  if (windowMask_ && ![self inLiveResize] &&
      !IsRectInsidePath(dirtyRect, windowMask_)) {
    DCHECK(base::mac::IsOSMavericks());
    gfx::ScopedNSGraphicsContextSaveGState state;

    // The outer rectangular path corresponding to the window.
    NSBezierPath* outerPath = [NSBezierPath bezierPathWithRect:[self bounds]];

    [outerPath appendBezierPath:windowMask_];
    [outerPath setWindingRule:NSEvenOddWindingRule];
    [[NSGraphicsContext currentContext]
        setCompositingOperation:NSCompositeCopy];
    [[NSColor clearColor] set];

    // Fill the region between windowMask_ and its outer rectangular path
    // with clear color. This causes the window to have the shape described
    // by windowMask_.
    [outerPath fill];
    // Regerate drop shadow around the window boundary.
    [[self window] invalidateShadow];
  }

  // If there's a layer, painting occurs in BridgedNativeWidget::OnPaintLayer().
  if (hostedView_->GetWidget()->GetLayer())
    return;

  gfx::CanvasSkiaPaint canvas(dirtyRect, false /* opaque */);
  hostedView_->GetWidget()->OnNativeWidgetPaint(
      ui::CanvasPainter(&canvas, 1.f).context());
}

// To maximize consistency with the Cocoa browser (mac_views_browser=0), accept
// mouse clicks immediately so that clicking on Chrome from an inactive window
// will allow the event to be processed, rather than merely activate the window.
- (BOOL)acceptsFirstMouse:(NSEvent*)theEvent {
  return YES;
}

// NSDraggingDestination protocol overrides.

- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender {
  return [self draggingUpdated:sender];
}

- (NSDragOperation)draggingUpdated:(id<NSDraggingInfo>)sender {
  views::DragDropClientMac* client = [self dragDropClient];
  return client ? client->DragUpdate(sender) : ui::DragDropTypes::DRAG_NONE;
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender {
  views::DragDropClientMac* client = [self dragDropClient];
  return client && client->Drop(sender) != NSDragOperationNone;
}

- (NSTextInputContext*)inputContext {
  // If the textInputClient_ does not exist, return nil since this view does not
  // conform to NSTextInputClient protocol.
  if (!textInputClient_)
    return nil;

  // If a menu is active, and -[NSView interpretKeyEvents:] asks for the
  // input context, return nil. This ensures the action message is sent to
  // the view, rather than any NSTextInputClient a subview has installed.
  if ([self activeMenuController])
    return nil;

  // When not in an editable mode, or while entering passwords
  // (http://crbug.com/23219), we don't want to show IME candidate windows.
  // Returning nil prevents this view from getting messages defined as part of
  // the NSTextInputClient protocol.
  switch (textInputClient_->GetTextInputType()) {
    case ui::TEXT_INPUT_TYPE_NONE:
    case ui::TEXT_INPUT_TYPE_PASSWORD:
      return nil;
    default:
      return [super inputContext];
  }
}

// NSResponder implementation.

- (void)keyDown:(NSEvent*)theEvent {
  // Convert the event into an action message, according to OSX key mappings.
  keyDownEvent_ = theEvent;
  [self interpretKeyEvents:@[ theEvent ]];
  keyDownEvent_ = nil;
}

- (void)keyUp:(NSEvent*)theEvent {
  ui::KeyEvent event(theEvent);
  [self handleKeyEvent:&event];
}

- (void)scrollWheel:(NSEvent*)theEvent {
  if (!hostedView_)
    return;

  ui::MouseWheelEvent event(theEvent);
  hostedView_->GetWidget()->OnMouseEvent(&event);
}

////////////////////////////////////////////////////////////////////////////////
// NSResponder Action Messages. Keep sorted according NSResponder.h (from the
// 10.9 SDK). The list should eventually be complete. Anything not defined will
// beep when interpretKeyEvents: would otherwise call it.
// TODO(tapted): Make this list complete, except for insert* methods which are
// dispatched as regular key events in doCommandBySelector:.

// views::Textfields are single-line only, map Paragraph and Document commands
// to Line. Also, Up/Down commands correspond to beginning/end of line.

// The insertText action message forwards to the TextInputClient unless a menu
// is active. Note that NSResponder's interpretKeyEvents: implementation doesn't
// direct insertText: through doCommandBySelector:, so this is still needed to
// handle the case when inputContext: is nil. When inputContext: returns non-nil
// text goes directly to insertText:replacementRange:.
- (void)insertText:(id)text {
  DCHECK_EQ(nil, [self inputContext]);
  [self insertTextInternal:text];
}

// Selection movement and scrolling.

- (void)moveForward:(id)sender {
  IsTextRTL(textInputClient_) ? [self moveLeft:sender]
                              : [self moveRight:sender];
}

- (void)moveRight:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_RIGHT
             keyCode:ui::VKEY_RIGHT
             domCode:ui::DomCode::ARROW_RIGHT
          eventFlags:0];
}

- (void)moveBackward:(id)sender {
  IsTextRTL(textInputClient_) ? [self moveRight:sender]
                              : [self moveLeft:sender];
}

- (void)moveLeft:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_LEFT
             keyCode:ui::VKEY_LEFT
             domCode:ui::DomCode::ARROW_LEFT
          eventFlags:0];
}

- (void)moveUp:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_TO_BEGINNING_OF_LINE
             keyCode:ui::VKEY_UP
             domCode:ui::DomCode::ARROW_UP
          eventFlags:0];
}

- (void)moveDown:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_TO_END_OF_LINE
             keyCode:ui::VKEY_DOWN
             domCode:ui::DomCode::ARROW_DOWN
          eventFlags:0];
}

- (void)moveWordForward:(id)sender {
  IsTextRTL(textInputClient_) ? [self moveWordLeft:sender]
                              : [self moveWordRight:sender];
}

- (void)moveWordBackward:(id)sender {
  IsTextRTL(textInputClient_) ? [self moveWordRight:sender]
                              : [self moveWordLeft:sender];
}

- (void)moveToBeginningOfLine:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_TO_BEGINNING_OF_LINE
             keyCode:ui::VKEY_HOME
             domCode:ui::DomCode::HOME
          eventFlags:0];
}

- (void)moveToEndOfLine:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_TO_END_OF_LINE
             keyCode:ui::VKEY_END
             domCode:ui::DomCode::END
          eventFlags:0];
}

- (void)moveToBeginningOfParagraph:(id)sender {
  [self moveToBeginningOfLine:sender];
}

- (void)moveToEndOfParagraph:(id)sender {
  [self moveToEndOfLine:sender];
}

- (void)moveToEndOfDocument:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_TO_END_OF_LINE
             keyCode:ui::VKEY_END
             domCode:ui::DomCode::END
          eventFlags:ui::EF_CONTROL_DOWN];
}

- (void)moveToBeginningOfDocument:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_TO_BEGINNING_OF_LINE
             keyCode:ui::VKEY_HOME
             domCode:ui::DomCode::HOME
          eventFlags:ui::EF_CONTROL_DOWN];
}

- (void)pageDown:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_TO_END_OF_LINE
             keyCode:ui::VKEY_NEXT
             domCode:ui::DomCode::PAGE_DOWN
          eventFlags:0];
}

- (void)pageUp:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_TO_BEGINNING_OF_LINE
             keyCode:ui::VKEY_PRIOR
             domCode:ui::DomCode::PAGE_UP
          eventFlags:0];
}

- (void)moveBackwardAndModifySelection:(id)sender {
  IsTextRTL(textInputClient_) ? [self moveRightAndModifySelection:sender]
                              : [self moveLeftAndModifySelection:sender];
}

- (void)moveForwardAndModifySelection:(id)sender {
  IsTextRTL(textInputClient_) ? [self moveLeftAndModifySelection:sender]
                              : [self moveRightAndModifySelection:sender];
}

- (void)moveWordForwardAndModifySelection:(id)sender {
  IsTextRTL(textInputClient_) ? [self moveWordLeftAndModifySelection:sender]
                              : [self moveWordRightAndModifySelection:sender];
}

- (void)moveWordBackwardAndModifySelection:(id)sender {
  IsTextRTL(textInputClient_) ? [self moveWordRightAndModifySelection:sender]
                              : [self moveWordLeftAndModifySelection:sender];
}

- (void)moveUpAndModifySelection:(id)sender {
  [self handleAction:ui::TextEditCommand::
                         MOVE_TO_BEGINNING_OF_LINE_AND_MODIFY_SELECTION
             keyCode:ui::VKEY_UP
             domCode:ui::DomCode::ARROW_UP
          eventFlags:ui::EF_SHIFT_DOWN];
}

- (void)moveDownAndModifySelection:(id)sender {
  [self
      handleAction:ui::TextEditCommand::MOVE_TO_END_OF_LINE_AND_MODIFY_SELECTION
           keyCode:ui::VKEY_DOWN
           domCode:ui::DomCode::ARROW_DOWN
        eventFlags:ui::EF_SHIFT_DOWN];
}

- (void)moveToBeginningOfLineAndModifySelection:(id)sender {
  [self handleAction:ui::TextEditCommand::
                         MOVE_TO_BEGINNING_OF_LINE_AND_MODIFY_SELECTION
             keyCode:ui::VKEY_HOME
             domCode:ui::DomCode::HOME
          eventFlags:ui::EF_SHIFT_DOWN];
}

- (void)moveToEndOfLineAndModifySelection:(id)sender {
  [self
      handleAction:ui::TextEditCommand::MOVE_TO_END_OF_LINE_AND_MODIFY_SELECTION
           keyCode:ui::VKEY_END
           domCode:ui::DomCode::END
        eventFlags:ui::EF_SHIFT_DOWN];
}

- (void)moveToBeginningOfParagraphAndModifySelection:(id)sender {
  [self moveToBeginningOfLineAndModifySelection:sender];
}

- (void)moveToEndOfParagraphAndModifySelection:(id)sender {
  [self moveToEndOfLineAndModifySelection:sender];
}

- (void)moveToEndOfDocumentAndModifySelection:(id)sender {
  [self
      handleAction:ui::TextEditCommand::MOVE_TO_END_OF_LINE_AND_MODIFY_SELECTION
           keyCode:ui::VKEY_END
           domCode:ui::DomCode::END
        eventFlags:ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN];
}

- (void)moveToBeginningOfDocumentAndModifySelection:(id)sender {
  [self handleAction:ui::TextEditCommand::
                         MOVE_TO_BEGINNING_OF_LINE_AND_MODIFY_SELECTION
             keyCode:ui::VKEY_HOME
             domCode:ui::DomCode::HOME
          eventFlags:ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN];
}

- (void)pageDownAndModifySelection:(id)sender {
  [self
      handleAction:ui::TextEditCommand::MOVE_TO_END_OF_LINE_AND_MODIFY_SELECTION
           keyCode:ui::VKEY_NEXT
           domCode:ui::DomCode::PAGE_DOWN
        eventFlags:ui::EF_SHIFT_DOWN];
}

- (void)pageUpAndModifySelection:(id)sender {
  [self handleAction:ui::TextEditCommand::
                         MOVE_TO_BEGINNING_OF_LINE_AND_MODIFY_SELECTION
             keyCode:ui::VKEY_PRIOR
             domCode:ui::DomCode::PAGE_UP
          eventFlags:ui::EF_SHIFT_DOWN];
}

- (void)moveParagraphForwardAndModifySelection:(id)sender {
  [self
      handleAction:ui::TextEditCommand::MOVE_TO_END_OF_LINE_AND_MODIFY_SELECTION
           keyCode:ui::VKEY_DOWN
           domCode:ui::DomCode::ARROW_DOWN
        eventFlags:ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN];
}

- (void)moveParagraphBackwardAndModifySelection:(id)sender {
  [self handleAction:ui::TextEditCommand::
                         MOVE_TO_BEGINNING_OF_LINE_AND_MODIFY_SELECTION
             keyCode:ui::VKEY_UP
             domCode:ui::DomCode::ARROW_UP
          eventFlags:ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN];
}

- (void)moveWordRight:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_WORD_RIGHT
             keyCode:ui::VKEY_RIGHT
             domCode:ui::DomCode::ARROW_RIGHT
          eventFlags:ui::EF_CONTROL_DOWN];
}

- (void)moveWordLeft:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_WORD_LEFT
             keyCode:ui::VKEY_LEFT
             domCode:ui::DomCode::ARROW_LEFT
          eventFlags:ui::EF_CONTROL_DOWN];
}

- (void)moveRightAndModifySelection:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_RIGHT_AND_MODIFY_SELECTION
             keyCode:ui::VKEY_RIGHT
             domCode:ui::DomCode::ARROW_RIGHT
          eventFlags:ui::EF_SHIFT_DOWN];
}

- (void)moveLeftAndModifySelection:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_LEFT_AND_MODIFY_SELECTION
             keyCode:ui::VKEY_LEFT
             domCode:ui::DomCode::ARROW_LEFT
          eventFlags:ui::EF_SHIFT_DOWN];
}

- (void)moveWordRightAndModifySelection:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_WORD_RIGHT_AND_MODIFY_SELECTION
             keyCode:ui::VKEY_RIGHT
             domCode:ui::DomCode::ARROW_RIGHT
          eventFlags:ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN];
}

- (void)moveWordLeftAndModifySelection:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_WORD_LEFT_AND_MODIFY_SELECTION
             keyCode:ui::VKEY_LEFT
             domCode:ui::DomCode::ARROW_LEFT
          eventFlags:ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN];
}

- (void)moveToLeftEndOfLine:(id)sender {
  IsTextRTL(textInputClient_) ? [self moveToEndOfLine:sender]
                              : [self moveToBeginningOfLine:sender];
}

- (void)moveToRightEndOfLine:(id)sender {
  IsTextRTL(textInputClient_) ? [self moveToBeginningOfLine:sender]
                              : [self moveToEndOfLine:sender];
}

- (void)moveToLeftEndOfLineAndModifySelection:(id)sender {
  IsTextRTL(textInputClient_)
      ? [self moveToEndOfLineAndModifySelection:sender]
      : [self moveToBeginningOfLineAndModifySelection:sender];
}

- (void)moveToRightEndOfLineAndModifySelection:(id)sender {
  IsTextRTL(textInputClient_)
      ? [self moveToBeginningOfLineAndModifySelection:sender]
      : [self moveToEndOfLineAndModifySelection:sender];
}

// Graphical Element transposition

- (void)transpose:(id)sender {
  [self handleAction:ui::TextEditCommand::TRANSPOSE
             keyCode:ui::VKEY_T
             domCode:ui::DomCode::US_T
          eventFlags:ui::EF_CONTROL_DOWN];
}

// Deletions.

- (void)deleteForward:(id)sender {
  [self handleAction:ui::TextEditCommand::DELETE_FORWARD
             keyCode:ui::VKEY_DELETE
             domCode:ui::DomCode::DEL
          eventFlags:0];
}

- (void)deleteBackward:(id)sender {
  [self handleAction:ui::TextEditCommand::DELETE_BACKWARD
             keyCode:ui::VKEY_BACK
             domCode:ui::DomCode::BACKSPACE
          eventFlags:0];
}

- (void)deleteWordForward:(id)sender {
  [self handleAction:ui::TextEditCommand::DELETE_WORD_FORWARD
             keyCode:ui::VKEY_DELETE
             domCode:ui::DomCode::DEL
          eventFlags:ui::EF_CONTROL_DOWN];
}

- (void)deleteWordBackward:(id)sender {
  [self handleAction:ui::TextEditCommand::DELETE_WORD_BACKWARD
             keyCode:ui::VKEY_BACK
             domCode:ui::DomCode::BACKSPACE
          eventFlags:ui::EF_CONTROL_DOWN];
}

- (void)deleteToBeginningOfLine:(id)sender {
  [self handleAction:ui::TextEditCommand::DELETE_TO_BEGINNING_OF_LINE
             keyCode:ui::VKEY_BACK
             domCode:ui::DomCode::BACKSPACE
          eventFlags:ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN];
}

- (void)deleteToEndOfLine:(id)sender {
  [self handleAction:ui::TextEditCommand::DELETE_TO_END_OF_LINE
             keyCode:ui::VKEY_DELETE
             domCode:ui::DomCode::DEL
          eventFlags:ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN];
}

- (void)deleteToBeginningOfParagraph:(id)sender {
  [self deleteToBeginningOfLine:sender];
}

- (void)deleteToEndOfParagraph:(id)sender {
  [self deleteToEndOfLine:sender];
}

// Cancellation.

- (void)cancelOperation:(id)sender {
  [self handleAction:ui::TextEditCommand::INVALID_COMMAND
             keyCode:ui::VKEY_ESCAPE
             domCode:ui::DomCode::ESCAPE
          eventFlags:0];
}

// Support for Services in context menus.
// Currently we only support reading and writing plain strings.
- (id)validRequestorForSendType:(NSString*)sendType
                     returnType:(NSString*)returnType {
  BOOL canWrite = [sendType isEqualToString:NSStringPboardType] &&
                  [self selectedRange].length > 0;
  BOOL canRead = [returnType isEqualToString:NSStringPboardType];
  // Valid if (sendType, returnType) is either (string, nil), (nil, string),
  // or (string, string).
  BOOL valid = textInputClient_ && ((canWrite && (canRead || !returnType)) ||
                                    (canRead && (canWrite || !sendType)));
  return valid ? self : [super validRequestorForSendType:sendType
                                              returnType:returnType];
}

// NSServicesRequests informal protocol.

- (BOOL)writeSelectionToPasteboard:(NSPasteboard*)pboard types:(NSArray*)types {
  DCHECK([types containsObject:NSStringPboardType]);
  if (!textInputClient_)
    return NO;

  gfx::Range selectionRange;
  if (!textInputClient_->GetSelectionRange(&selectionRange))
    return NO;

  base::string16 text;
  textInputClient_->GetTextFromRange(selectionRange, &text);
  return [pboard writeObjects:@[ base::SysUTF16ToNSString(text) ]];
}

- (BOOL)readSelectionFromPasteboard:(NSPasteboard*)pboard {
  NSArray* objects =
      [pboard readObjectsForClasses:@[ [NSString class] ] options:0];
  DCHECK([objects count] == 1);
  [self insertText:[objects lastObject]];
  return YES;
}

// NSTextInputClient protocol implementation.

- (NSAttributedString*)
    attributedSubstringForProposedRange:(NSRange)range
                            actualRange:(NSRangePointer)actualRange {
  gfx::Range actual_range;
  base::string16 substring = AttributedSubstringForRangeHelper(
      textInputClient_, gfx::Range(range), &actual_range);
  if (actualRange) {
    // To maintain consistency with NSTextView, return range {0,0} for an out of
    // bounds requested range.
    *actualRange =
        actual_range.IsValid() ? actual_range.ToNSRange() : NSMakeRange(0, 0);
  }
  return substring.empty()
             ? nil
             : [[[NSAttributedString alloc]
                   initWithString:base::SysUTF16ToNSString(substring)]
                   autorelease];
}

- (NSUInteger)characterIndexForPoint:(NSPoint)aPoint {
  NOTIMPLEMENTED();
  return 0;
}

- (void)doCommandBySelector:(SEL)selector {
  // Like the renderer, handle insert action messages as a regular key dispatch.
  // This ensures, e.g., insertTab correctly changes focus between fields.
  if (keyDownEvent_ && [NSStringFromSelector(selector) hasPrefix:@"insert"]) {
    ui::KeyEvent event(keyDownEvent_);
    [self handleKeyEvent:&event];
    return;
  }

  if ([self respondsToSelector:selector])
    [self performSelector:selector withObject:nil];
  else
    [[self nextResponder] doCommandBySelector:selector];
}

- (NSRect)firstRectForCharacterRange:(NSRange)range
                         actualRange:(NSRangePointer)actualNSRange {
  gfx::Range actualRange;
  gfx::Rect rect = GetFirstRectForRangeHelper(textInputClient_,
                                              gfx::Range(range), &actualRange);
  if (actualNSRange)
    *actualNSRange = actualRange.ToNSRange();
  return gfx::ScreenRectToNSRect(rect);
}

- (BOOL)hasMarkedText {
  return textInputClient_ && textInputClient_->HasCompositionText();
}

- (void)insertText:(id)text replacementRange:(NSRange)replacementRange {
  if (!hostedView_)
    return;

  // Verify inputContext is not nil, i.e. |textInputClient_| is valid and no
  // menu is active.
  DCHECK([self inputContext]);

  textInputClient_->DeleteRange(gfx::Range(replacementRange));
  [self insertTextInternal:text];
}

- (NSRange)markedRange {
  if (!textInputClient_)
    return NSMakeRange(NSNotFound, 0);

  gfx::Range range;
  textInputClient_->GetCompositionTextRange(&range);
  return range.ToNSRange();
}

- (NSRange)selectedRange {
  if (!textInputClient_)
    return NSMakeRange(NSNotFound, 0);

  gfx::Range range;
  textInputClient_->GetSelectionRange(&range);
  return range.ToNSRange();
}

- (void)setMarkedText:(id)text
        selectedRange:(NSRange)selectedRange
     replacementRange:(NSRange)replacementRange {
  if (!textInputClient_)
    return;

  if ([text isKindOfClass:[NSAttributedString class]])
    text = [text string];
  ui::CompositionText composition;
  composition.text = base::SysNSStringToUTF16(text);
  composition.selection = gfx::Range(selectedRange);
  textInputClient_->SetCompositionText(composition);
}

- (void)unmarkText {
  if (textInputClient_)
    textInputClient_->ConfirmCompositionText();
}

- (NSArray*)validAttributesForMarkedText {
  return @[];
}

// NSUserInterfaceValidations protocol implementation.

- (BOOL)validateUserInterfaceItem:(id<NSValidatedUserInterfaceItem>)item {
  if (!textInputClient_)
    return NO;

  SEL action = [item action];

  if (action == @selector(undo:))
    return textInputClient_->IsTextEditCommandEnabled(
        ui::TextEditCommand::UNDO);
  if (action == @selector(redo:))
    return textInputClient_->IsTextEditCommandEnabled(
        ui::TextEditCommand::REDO);
  if (action == @selector(cut:))
    return textInputClient_->IsTextEditCommandEnabled(ui::TextEditCommand::CUT);
  if (action == @selector(copy:))
    return textInputClient_->IsTextEditCommandEnabled(
        ui::TextEditCommand::COPY);
  if (action == @selector(paste:))
    return textInputClient_->IsTextEditCommandEnabled(
        ui::TextEditCommand::PASTE);
  if (action == @selector(selectAll:))
    return textInputClient_->IsTextEditCommandEnabled(
        ui::TextEditCommand::SELECT_ALL);

  return NO;
}

// NSDraggingSource protocol implementation.

- (NSDragOperation)draggingSession:(NSDraggingSession*)session
    sourceOperationMaskForDraggingContext:(NSDraggingContext)context {
  return NSDragOperationEvery;
}

- (void)draggingSession:(NSDraggingSession*)session
           endedAtPoint:(NSPoint)screenPoint
              operation:(NSDragOperation)operation {
  views::DragDropClientMac* client = [self dragDropClient];
  if (client)
    client->EndDrag();
}

// NSAccessibility informal protocol implementation.

- (id)accessibilityAttributeValue:(NSString*)attribute {
  if ([attribute isEqualToString:NSAccessibilityChildrenAttribute]) {
    return @[ hostedView_->GetNativeViewAccessible() ];
  }

  return [super accessibilityAttributeValue:attribute];
}

- (id)accessibilityHitTest:(NSPoint)point {
  return [hostedView_->GetNativeViewAccessible() accessibilityHitTest:point];
}

@end

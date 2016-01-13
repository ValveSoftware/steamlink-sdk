// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/message_center/cocoa/notification_controller.h"

#include <algorithm>

#include "base/mac/foundation_util.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "grit/ui_resources.h"
#include "grit/ui_strings.h"
#include "skia/ext/skia_utils_mac.h"
#import "ui/base/cocoa/hover_image_button.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/text_utils.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_style.h"
#include "ui/message_center/notification.h"


@interface MCNotificationProgressBar : NSProgressIndicator
@end

@implementation MCNotificationProgressBar
- (void)drawRect:(NSRect)dirtyRect {
  NSRect sliceRect, remainderRect;
  double progressFraction = ([self doubleValue] - [self minValue]) /
      ([self maxValue] - [self minValue]);
  NSDivideRect(dirtyRect, &sliceRect, &remainderRect,
               NSWidth(dirtyRect) * progressFraction, NSMinXEdge);

  NSBezierPath* path = [NSBezierPath bezierPathWithRoundedRect:dirtyRect
      xRadius:message_center::kProgressBarCornerRadius
      yRadius:message_center::kProgressBarCornerRadius];
  [gfx::SkColorToCalibratedNSColor(message_center::kProgressBarBackgroundColor)
      set];
  [path fill];

  if (progressFraction == 0.0)
    return;

  path = [NSBezierPath bezierPathWithRoundedRect:sliceRect
      xRadius:message_center::kProgressBarCornerRadius
      yRadius:message_center::kProgressBarCornerRadius];
  [gfx::SkColorToCalibratedNSColor(message_center::kProgressBarSliceColor) set];
  [path fill];
}

- (id)accessibilityAttributeValue:(NSString*)attribute {
  double progressValue = 0.0;
  if ([attribute isEqualToString:NSAccessibilityDescriptionAttribute]) {
    progressValue = [self doubleValue];
  } else if ([attribute isEqualToString:NSAccessibilityMinValueAttribute]) {
    progressValue = [self minValue];
  } else if ([attribute isEqualToString:NSAccessibilityMaxValueAttribute]) {
    progressValue = [self maxValue];
  } else {
    return [super accessibilityAttributeValue:attribute];
  }

  return [NSString stringWithFormat:@"%lf", progressValue];
}
@end

////////////////////////////////////////////////////////////////////////////////
@interface MCNotificationButton : NSButton
@end

@implementation MCNotificationButton
// drawRect: needs to fill the button with a background, otherwise we don't get
// subpixel antialiasing.
- (void)drawRect:(NSRect)dirtyRect {
  NSColor* color = gfx::SkColorToCalibratedNSColor(
      message_center::kNotificationBackgroundColor);
  [color set];
  NSRectFill(dirtyRect);
  [super drawRect:dirtyRect];
}
@end

@interface MCNotificationButtonCell : NSButtonCell {
  BOOL hovered_;
}
@end

////////////////////////////////////////////////////////////////////////////////
@implementation MCNotificationButtonCell
- (BOOL)isOpaque {
  return YES;
}

- (void)drawBezelWithFrame:(NSRect)frame inView:(NSView*)controlView {
  // Else mouseEntered: and mouseExited: won't be called and hovered_ won't be
  // valid.
  DCHECK([self showsBorderOnlyWhileMouseInside]);

  if (!hovered_)
    return;
  [gfx::SkColorToCalibratedNSColor(
      message_center::kHoveredButtonBackgroundColor) set];
  NSRectFill(frame);
}

- (void)drawImage:(NSImage*)image
        withFrame:(NSRect)frame
           inView:(NSView*)controlView {
  if (!image)
    return;
  NSRect rect = NSMakeRect(message_center::kButtonHorizontalPadding,
                           message_center::kButtonIconTopPadding,
                           message_center::kNotificationButtonIconSize,
                           message_center::kNotificationButtonIconSize);
  [image drawInRect:rect
            fromRect:NSZeroRect
           operation:NSCompositeSourceOver
            fraction:1.0
      respectFlipped:YES
               hints:nil];
}

- (NSRect)drawTitle:(NSAttributedString*)title
          withFrame:(NSRect)frame
             inView:(NSView*)controlView {
  CGFloat offsetX = message_center::kButtonHorizontalPadding;
  if ([base::mac::ObjCCastStrict<NSButton>(controlView) image]) {
    offsetX += message_center::kNotificationButtonIconSize +
               message_center::kButtonIconToTitlePadding;
  }
  frame.origin.x = offsetX;
  frame.size.width -= offsetX;

  NSDictionary* attributes = @{
    NSFontAttributeName :
        [title attribute:NSFontAttributeName atIndex:0 effectiveRange:NULL],
    NSForegroundColorAttributeName :
        gfx::SkColorToCalibratedNSColor(message_center::kRegularTextColor),
  };
  [[title string] drawWithRect:frame
                       options:(NSStringDrawingUsesLineFragmentOrigin |
                                NSStringDrawingTruncatesLastVisibleLine)
                    attributes:attributes];
  return frame;
}

- (void)mouseEntered:(NSEvent*)event {
  hovered_ = YES;

  // Else the cell won't be repainted on hover.
  [super mouseEntered:event];
}

- (void)mouseExited:(NSEvent*)event {
  hovered_ = NO;
  [super mouseExited:event];
}
@end

////////////////////////////////////////////////////////////////////////////////

@interface MCNotificationView : NSBox {
 @private
  MCNotificationController* controller_;
}

- (id)initWithController:(MCNotificationController*)controller
                   frame:(NSRect)frame;
@end

@implementation MCNotificationView
- (id)initWithController:(MCNotificationController*)controller
                   frame:(NSRect)frame {
  if ((self = [super initWithFrame:frame]))
    controller_ = controller;
  return self;
}

- (void)mouseDown:(NSEvent*)event {
  if ([event type] != NSLeftMouseDown) {
    [super mouseDown:event];
    return;
  }
  [controller_ notificationClicked];
}

- (NSView*)hitTest:(NSPoint)point {
  // Route the mouse click events on NSTextView to the container view.
  NSView* hitView = [super hitTest:point];
  if (hitView)
    return [hitView isKindOfClass:[NSTextView class]] ? self : hitView;
  return nil;
}

- (BOOL)accessibilityIsIgnored {
  return NO;
}

- (NSArray*)accessibilityActionNames {
  return @[ NSAccessibilityPressAction ];
}

- (void)accessibilityPerformAction:(NSString*)action {
  if ([action isEqualToString:NSAccessibilityPressAction]) {
    [controller_ notificationClicked];
    return;
  }
  [super accessibilityPerformAction:action];
}
@end

////////////////////////////////////////////////////////////////////////////////

@interface AccessibilityIgnoredBox : NSBox
@end

// Ignore this element, but expose its children to accessibility.
@implementation AccessibilityIgnoredBox
- (BOOL)accessibilityIsIgnored {
  return YES;
}

// Pretend this element has no children.
// TODO(petewil): Until we have alt text available, we will hide the children of
//  the box also.  Remove this override once alt text is set (by using
// NSAccessibilityDescriptionAttribute).
- (id)accessibilityAttributeValue:(NSString*)attribute {
  // If we get a request for NSAccessibilityChildrenAttribute, return an empty
  // array to pretend we have no children.
  if ([attribute isEqualToString:NSAccessibilityChildrenAttribute])
    return @[];
  else
    return [super accessibilityAttributeValue:attribute];
}
@end

////////////////////////////////////////////////////////////////////////////////

@interface MCNotificationController (Private)
// Configures a NSBox to be borderless, titleless, and otherwise appearance-
// free.
- (void)configureCustomBox:(NSBox*)box;

// Initializes the icon_ ivar and returns the view to insert into the hierarchy.
- (NSView*)createIconView;

// Creates a box that shows a border when the icon is not big enough to fill the
// space.
- (NSBox*)createImageBox:(const gfx::Image&)notificationImage;

// Initializes the closeButton_ ivar with the configured button.
- (void)configureCloseButtonInFrame:(NSRect)rootFrame;

// Initializes the smallImage_ ivar with the appropriate frame.
- (void)configureSmallImageInFrame:(NSRect)rootFrame;

// Initializes title_ in the given frame.
- (void)configureTitleInFrame:(NSRect)rootFrame;

// Initializes message_ in the given frame.
- (void)configureBodyInFrame:(NSRect)rootFrame;

// Initializes contextMessage_ in the given frame.
- (void)configureContextMessageInFrame:(NSRect)rootFrame;

// Creates a NSTextView that the caller owns configured as a label in a
// notification.
- (NSTextView*)newLabelWithFrame:(NSRect)frame;

// Gets the rectangle in which notification content should be placed. This
// rectangle is to the right of the icon and left of the control buttons.
// This depends on the icon_ and closeButton_ being initialized.
- (NSRect)currentContentRect;

// Returns the wrapped text that could fit within the content rect with not
// more than the given number of lines. The wrapped text would be painted using
// the given font. The Ellipsis could be added at the end of the last line if
// it is too long. Outputs the number of lines computed in the actualLines
// parameter.
- (base::string16)wrapText:(const base::string16&)text
                   forFont:(NSFont*)font
          maxNumberOfLines:(size_t)lines
               actualLines:(size_t*)actualLines;

// Same as above without outputting the lines formatted.
- (base::string16)wrapText:(const base::string16&)text
                   forFont:(NSFont*)font
          maxNumberOfLines:(size_t)lines;

@end

////////////////////////////////////////////////////////////////////////////////

@implementation MCNotificationController

- (id)initWithNotification:(const message_center::Notification*)notification
    messageCenter:(message_center::MessageCenter*)messageCenter {
  if ((self = [super initWithNibName:nil bundle:nil])) {
    notification_ = notification;
    notificationID_ = notification_->id();
    messageCenter_ = messageCenter;
  }
  return self;
}

- (void)loadView {
  // Create the root view of the notification.
  NSRect rootFrame = NSMakeRect(0, 0,
      message_center::kNotificationPreferredImageWidth,
      message_center::kNotificationIconSize);
  base::scoped_nsobject<MCNotificationView> rootView(
      [[MCNotificationView alloc] initWithController:self frame:rootFrame]);
  [self configureCustomBox:rootView];
  [rootView setFillColor:gfx::SkColorToCalibratedNSColor(
      message_center::kNotificationBackgroundColor)];
  [self setView:rootView];

  [rootView addSubview:[self createIconView]];

  // Create the close button.
  [self configureCloseButtonInFrame:rootFrame];
  [rootView addSubview:closeButton_];

  // Create the small image.
  [rootView addSubview:[self createSmallImageInFrame:rootFrame]];

  NSRect contentFrame = [self currentContentRect];

  // Create the title.
  [self configureTitleInFrame:contentFrame];
  [rootView addSubview:title_];

  // Create the message body.
  [self configureBodyInFrame:contentFrame];
  [rootView addSubview:message_];

  // Create the context message body.
  [self configureContextMessageInFrame:contentFrame];
  [rootView addSubview:contextMessage_];

  // Populate the data.
  [self updateNotification:notification_];
}

- (NSRect)updateNotification:(const message_center::Notification*)notification {
  DCHECK_EQ(notification->id(), notificationID_);
  notification_ = notification;

  NSRect rootFrame = NSMakeRect(0, 0,
      message_center::kNotificationPreferredImageWidth,
      message_center::kNotificationIconSize);

  [smallImage_ setImage:notification_->small_image().AsNSImage()];

  // Update the icon.
  [icon_ setImage:notification_->icon().AsNSImage()];

  // The message_center:: constants are relative to capHeight at the top and
  // relative to the baseline at the bottom, but NSTextField uses the full line
  // height for its height.
  CGFloat titleTopGap =
      roundf([[title_ font] ascender] - [[title_ font] capHeight]);
  CGFloat titleBottomGap = roundf(fabs([[title_ font] descender]));
  CGFloat titlePadding = message_center::kTextTopPadding - titleTopGap;

  CGFloat messageTopGap =
      roundf([[message_ font] ascender] - [[message_ font] capHeight]);
  CGFloat messageBottomGap = roundf(fabs([[message_ font] descender]));
  CGFloat messagePadding =
      message_center::kTextTopPadding - titleBottomGap - messageTopGap;

  CGFloat contextMessageTopGap = roundf(
      [[contextMessage_ font] ascender] - [[contextMessage_ font] capHeight]);
  CGFloat contextMessagePadding =
      message_center::kTextTopPadding - messageBottomGap - contextMessageTopGap;

  // Set the title and recalculate the frame.
  size_t actualTitleLines = 0;
  [title_ setString:base::SysUTF16ToNSString(
      [self wrapText:notification_->title()
                forFont:[title_ font]
       maxNumberOfLines:message_center::kMaxTitleLines
            actualLines:&actualTitleLines])];
  [title_ sizeToFit];
  NSRect titleFrame = [title_ frame];
  titleFrame.origin.y = NSMaxY(rootFrame) - titlePadding - NSHeight(titleFrame);

  // The number of message lines depends on the number of context message lines
  // and the lines within the title, and whether an image exists.
  int messageLineLimit = message_center::kMessageExpandedLineLimit;
  if (actualTitleLines > 1)
    messageLineLimit -= (actualTitleLines - 1) * 2;
  if (!notification_->image().IsEmpty()) {
    messageLineLimit /= 2;
    if (!notification_->context_message().empty())
      messageLineLimit -= message_center::kContextMessageLineLimit;
  }
  if (messageLineLimit < 0)
    messageLineLimit = 0;

  // Set the message and recalculate the frame.
  [message_ setString:base::SysUTF16ToNSString(
      [self wrapText:notification_->message()
             forFont:[message_ font]
      maxNumberOfLines:messageLineLimit])];
  [message_ sizeToFit];
  NSRect messageFrame = [message_ frame];

  // If there are list items, then the message_ view should not be displayed.
  const std::vector<message_center::NotificationItem>& items =
      notification->items();
  // If there are list items, don't show the main message.  Also if the message
  // is empty, mark it as hidden and set 0 height, so it doesn't take up any
  // space (size to fit leaves it 15 px tall.
  if (items.size() > 0 || notification_->message().empty()) {
    [message_ setHidden:YES];
    messageFrame.origin.y = titleFrame.origin.y;
    messageFrame.size.height = 0;
  } else {
    [message_ setHidden:NO];
    messageFrame.origin.y =
        NSMinY(titleFrame) - messagePadding - NSHeight(messageFrame);
    messageFrame.size.height = NSHeight([message_ frame]);
  }

  // Set the context message and recalculate the frame.
  [contextMessage_ setString:base::SysUTF16ToNSString(
      [self wrapText:notification_->context_message()
             forFont:[contextMessage_ font]
       maxNumberOfLines:message_center::kContextMessageLineLimit])];
  [contextMessage_ sizeToFit];
  NSRect contextMessageFrame = [contextMessage_ frame];

  if (notification_->context_message().empty()) {
    [contextMessage_ setHidden:YES];
    contextMessageFrame.origin.y = messageFrame.origin.y;
    contextMessageFrame.size.height = 0;
  } else {
    [contextMessage_ setHidden:NO];
    contextMessageFrame.origin.y =
        NSMinY(messageFrame) -
        contextMessagePadding -
        NSHeight(contextMessageFrame);
    contextMessageFrame.size.height = NSHeight([contextMessage_ frame]);
  }

  // Create the list item views (up to a maximum).
  [listView_ removeFromSuperview];
  NSRect listFrame = NSZeroRect;
  if (items.size() > 0) {
    listFrame = [self currentContentRect];
    listFrame.origin.y = 0;
    listFrame.size.height = 0;
    listView_.reset([[NSView alloc] initWithFrame:listFrame]);
    [listView_ accessibilitySetOverrideValue:NSAccessibilityListRole
                                    forAttribute:NSAccessibilityRoleAttribute];
    [listView_
        accessibilitySetOverrideValue:NSAccessibilityContentListSubrole
                         forAttribute:NSAccessibilitySubroleAttribute];
    CGFloat y = 0;

    NSFont* font = [NSFont systemFontOfSize:message_center::kMessageFontSize];
    CGFloat lineHeight = roundf(NSHeight([font boundingRectForFont]));

    const int kNumNotifications =
        std::min(items.size(), message_center::kNotificationMaximumItems);
    for (int i = kNumNotifications - 1; i >= 0; --i) {
      NSTextView* itemView = [self newLabelWithFrame:
          NSMakeRect(0, y, NSWidth(listFrame), lineHeight)];
      [itemView setFont:font];

      // Disable the word-wrap in order to show the text in single line.
      [[itemView textContainer] setContainerSize:NSMakeSize(FLT_MAX, FLT_MAX)];
      [[itemView textContainer] setWidthTracksTextView:NO];

      // Construct the text from the title and message.
      base::string16 text =
          items[i].title + base::UTF8ToUTF16(" ") + items[i].message;
      base::string16 ellidedText =
          [self wrapText:text forFont:font maxNumberOfLines:1];
      [itemView setString:base::SysUTF16ToNSString(ellidedText)];

      // Use dim color for the title part.
      NSColor* titleColor =
          gfx::SkColorToCalibratedNSColor(message_center::kRegularTextColor);
      NSRange titleRange = NSMakeRange(
          0,
          std::min(ellidedText.size(), items[i].title.size()));
      [itemView setTextColor:titleColor range:titleRange];

      // Use dim color for the message part if it has not been truncated.
      if (ellidedText.size() > items[i].title.size() + 1) {
        NSColor* messageColor =
            gfx::SkColorToCalibratedNSColor(message_center::kDimTextColor);
        NSRange messageRange = NSMakeRange(
            items[i].title.size() + 1,
            ellidedText.size() - items[i].title.size() - 1);
        [itemView setTextColor:messageColor range:messageRange];
      }

      [listView_ addSubview:itemView];
      y += lineHeight;
    }
    // TODO(thakis): The spacing is not completely right.
    CGFloat listTopPadding =
        message_center::kTextTopPadding - contextMessageTopGap;
    listFrame.size.height = y;
    listFrame.origin.y =
        NSMinY(contextMessageFrame) - listTopPadding - NSHeight(listFrame);
    [listView_ setFrame:listFrame];
    [[self view] addSubview:listView_];
  }

  // Create the progress bar view if needed.
  [progressBarView_ removeFromSuperview];
  NSRect progressBarFrame = NSZeroRect;
  if (notification->type() == message_center::NOTIFICATION_TYPE_PROGRESS) {
    progressBarFrame = [self currentContentRect];
    progressBarFrame.origin.y = NSMinY(contextMessageFrame) -
        message_center::kProgressBarTopPadding -
        message_center::kProgressBarThickness;
    progressBarFrame.size.height = message_center::kProgressBarThickness;
    progressBarView_.reset(
        [[MCNotificationProgressBar alloc] initWithFrame:progressBarFrame]);
    // Setting indeterminate to NO does not work with custom drawRect.
    [progressBarView_ setIndeterminate:YES];
    [progressBarView_ setStyle:NSProgressIndicatorBarStyle];
    [progressBarView_ setDoubleValue:notification->progress()];
    [[self view] addSubview:progressBarView_];
  }

  // If the bottom-most element so far is out of the rootView's bounds, resize
  // the view.
  CGFloat minY = NSMinY(contextMessageFrame);
  if (listView_ && NSMinY(listFrame) < minY)
    minY = NSMinY(listFrame);
  if (progressBarView_ && NSMinY(progressBarFrame) < minY)
    minY = NSMinY(progressBarFrame);
  if (minY < messagePadding) {
    CGFloat delta = messagePadding - minY;
    rootFrame.size.height += delta;
    titleFrame.origin.y += delta;
    messageFrame.origin.y += delta;
    contextMessageFrame.origin.y += delta;
    listFrame.origin.y += delta;
    progressBarFrame.origin.y += delta;
  }

  // Add the bottom container view.
  NSRect frame = rootFrame;
  frame.size.height = 0;
  [bottomView_ removeFromSuperview];
  bottomView_.reset([[NSView alloc] initWithFrame:frame]);
  CGFloat y = 0;

  // Create action buttons if appropriate, bottom-up.
  std::vector<message_center::ButtonInfo> buttons = notification->buttons();
  for (int i = buttons.size() - 1; i >= 0; --i) {
    message_center::ButtonInfo buttonInfo = buttons[i];
    NSRect buttonFrame = frame;
    buttonFrame.origin = NSMakePoint(0, y);
    buttonFrame.size.height = message_center::kButtonHeight;
    base::scoped_nsobject<MCNotificationButton> button(
        [[MCNotificationButton alloc] initWithFrame:buttonFrame]);
    base::scoped_nsobject<MCNotificationButtonCell> cell(
        [[MCNotificationButtonCell alloc]
            initTextCell:base::SysUTF16ToNSString(buttonInfo.title)]);
    [cell setShowsBorderOnlyWhileMouseInside:YES];
    [button setCell:cell];
    [button setImage:buttonInfo.icon.AsNSImage()];
    [button setBezelStyle:NSSmallSquareBezelStyle];
    [button setImagePosition:NSImageLeft];
    [button setTag:i];
    [button setTarget:self];
    [button setAction:@selector(buttonClicked:)];
    y += NSHeight(buttonFrame);
    frame.size.height += NSHeight(buttonFrame);
    [bottomView_ addSubview:button];

    NSRect separatorFrame = frame;
    separatorFrame.origin = NSMakePoint(0, y);
    separatorFrame.size.height = 1;
    base::scoped_nsobject<NSBox> separator(
        [[AccessibilityIgnoredBox alloc] initWithFrame:separatorFrame]);
    [self configureCustomBox:separator];
    [separator setFillColor:gfx::SkColorToCalibratedNSColor(
        message_center::kButtonSeparatorColor)];
    y += NSHeight(separatorFrame);
    frame.size.height += NSHeight(separatorFrame);
    [bottomView_ addSubview:separator];
  }

  // Create the image view if appropriate.
  gfx::Image notificationImage = notification->image();
  if (!notificationImage.IsEmpty()) {
    NSBox* imageBox = [self createImageBox:notificationImage];
    NSRect outerFrame = frame;
    outerFrame.origin = NSMakePoint(0, y);
    outerFrame.size = [imageBox frame].size;
    [imageBox setFrame:outerFrame];

    y += NSHeight(outerFrame);
    frame.size.height += NSHeight(outerFrame);

    [bottomView_ addSubview:imageBox];
  }

  [bottomView_ setFrame:frame];
  [[self view] addSubview:bottomView_];

  rootFrame.size.height += NSHeight(frame);
  titleFrame.origin.y += NSHeight(frame);
  messageFrame.origin.y += NSHeight(frame);
  contextMessageFrame.origin.y += NSHeight(frame);
  listFrame.origin.y += NSHeight(frame);
  progressBarFrame.origin.y += NSHeight(frame);

  // Make sure that there is a minimum amount of spacing below the icon and
  // the edge of the frame.
  CGFloat bottomDelta = NSHeight(rootFrame) - NSHeight([icon_ frame]);
  if (bottomDelta > 0 && bottomDelta < message_center::kIconBottomPadding) {
    CGFloat bottomAdjust = message_center::kIconBottomPadding - bottomDelta;
    rootFrame.size.height += bottomAdjust;
    titleFrame.origin.y += bottomAdjust;
    messageFrame.origin.y += bottomAdjust;
    contextMessageFrame.origin.y += bottomAdjust;
    listFrame.origin.y += bottomAdjust;
    progressBarFrame.origin.y += bottomAdjust;
  }

  [[self view] setFrame:rootFrame];
  [title_ setFrame:titleFrame];
  [message_ setFrame:messageFrame];
  [contextMessage_ setFrame:contextMessageFrame];
  [listView_ setFrame:listFrame];
  [progressBarView_ setFrame:progressBarFrame];

  return rootFrame;
}

- (void)close:(id)sender {
  [closeButton_ setTarget:nil];
  messageCenter_->RemoveNotification([self notificationID], /*by_user=*/true);
}

- (void)buttonClicked:(id)button {
  messageCenter_->ClickOnNotificationButton([self notificationID],
                                            [button tag]);
}

- (const message_center::Notification*)notification {
  return notification_;
}

- (const std::string&)notificationID {
  return notificationID_;
}

- (void)notificationClicked {
  messageCenter_->ClickOnNotification([self notificationID]);
}

// Private /////////////////////////////////////////////////////////////////////

- (void)configureCustomBox:(NSBox*)box {
  [box setBoxType:NSBoxCustom];
  [box setBorderType:NSNoBorder];
  [box setTitlePosition:NSNoTitle];
  [box setContentViewMargins:NSZeroSize];
}

- (NSView*)createIconView {
  // Create another box that shows a background color when the icon is not
  // big enough to fill the space.
  NSRect imageFrame = NSMakeRect(0, 0,
       message_center::kNotificationIconSize,
       message_center::kNotificationIconSize);
  base::scoped_nsobject<NSBox> imageBox(
      [[AccessibilityIgnoredBox alloc] initWithFrame:imageFrame]);
  [self configureCustomBox:imageBox];
  [imageBox setFillColor:gfx::SkColorToCalibratedNSColor(
      message_center::kIconBackgroundColor)];
  [imageBox setAutoresizingMask:NSViewMinYMargin];

  // Inside the image box put the actual icon view.
  icon_.reset([[NSImageView alloc] initWithFrame:imageFrame]);
  [imageBox setContentView:icon_];

  return imageBox.autorelease();
}

- (NSBox*)createImageBox:(const gfx::Image&)notificationImage {
  using message_center::kNotificationImageBorderSize;
  using message_center::kNotificationPreferredImageWidth;
  using message_center::kNotificationPreferredImageHeight;

  NSRect imageFrame = NSMakeRect(0, 0,
       kNotificationPreferredImageWidth,
       kNotificationPreferredImageHeight);
  base::scoped_nsobject<NSBox> imageBox(
      [[AccessibilityIgnoredBox alloc] initWithFrame:imageFrame]);
  [self configureCustomBox:imageBox];
  [imageBox setFillColor:gfx::SkColorToCalibratedNSColor(
      message_center::kImageBackgroundColor)];

  // Images with non-preferred aspect ratios get a border on all sides.
  gfx::Size idealSize = gfx::Size(
      kNotificationPreferredImageWidth, kNotificationPreferredImageHeight);
  gfx::Size scaledSize = message_center::GetImageSizeForContainerSize(
      idealSize, notificationImage.Size());
  if (scaledSize != idealSize) {
    NSSize borderSize =
        NSMakeSize(kNotificationImageBorderSize, kNotificationImageBorderSize);
    [imageBox setContentViewMargins:borderSize];
  }

  NSImage* image = notificationImage.AsNSImage();
  base::scoped_nsobject<NSImageView> imageView(
      [[NSImageView alloc] initWithFrame:imageFrame]);
  [imageView setImage:image];
  [imageView setImageScaling:NSImageScaleProportionallyUpOrDown];
  [imageBox setContentView:imageView];

  return imageBox.autorelease();
}

- (void)configureCloseButtonInFrame:(NSRect)rootFrame {
  // The close button is configured to be the same size as the small image.
  int closeButtonOriginOffset =
      message_center::kSmallImageSize + message_center::kSmallImagePadding;
  NSRect closeButtonFrame =
      NSMakeRect(NSMaxX(rootFrame) - closeButtonOriginOffset,
                 NSMaxY(rootFrame) - closeButtonOriginOffset,
                 message_center::kSmallImageSize,
                 message_center::kSmallImageSize);
  closeButton_.reset([[HoverImageButton alloc] initWithFrame:closeButtonFrame]);
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  [closeButton_ setDefaultImage:
      rb.GetNativeImageNamed(IDR_NOTIFICATION_CLOSE).ToNSImage()];
  [closeButton_ setHoverImage:
      rb.GetNativeImageNamed(IDR_NOTIFICATION_CLOSE_HOVER).ToNSImage()];
  [closeButton_ setPressedImage:
      rb.GetNativeImageNamed(IDR_NOTIFICATION_CLOSE_PRESSED).ToNSImage()];
  [[closeButton_ cell] setHighlightsBy:NSOnState];
  [closeButton_ setTrackingEnabled:YES];
  [closeButton_ setBordered:NO];
  [closeButton_ setAutoresizingMask:NSViewMinYMargin];
  [closeButton_ setTarget:self];
  [closeButton_ setAction:@selector(close:)];
  [[closeButton_ cell]
      accessibilitySetOverrideValue:NSAccessibilityCloseButtonSubrole
                       forAttribute:NSAccessibilitySubroleAttribute];
  [[closeButton_ cell]
      accessibilitySetOverrideValue:
          l10n_util::GetNSString(IDS_APP_ACCNAME_CLOSE)
                       forAttribute:NSAccessibilityTitleAttribute];
}

- (NSView*)createSmallImageInFrame:(NSRect)rootFrame {
  int smallImageXOffset =
      message_center::kSmallImagePadding + message_center::kSmallImageSize;
  NSRect boxFrame =
      NSMakeRect(NSMaxX(rootFrame) - smallImageXOffset,
                 NSMinY(rootFrame) + message_center::kSmallImagePadding,
                 message_center::kSmallImageSize,
                 message_center::kSmallImageSize);

  // Put the smallImage inside another box which can hide it from accessibility
  // until we have some alt text to go with it.  Once we have alt text, remove
  // the box, and set NSAccessibilityDescriptionAttribute with it.
  base::scoped_nsobject<NSBox> imageBox(
      [[AccessibilityIgnoredBox alloc] initWithFrame:boxFrame]);
  [self configureCustomBox:imageBox];
  [imageBox setAutoresizingMask:NSViewMinYMargin];

  NSRect smallImageFrame =
      NSMakeRect(0,0,
                 message_center::kSmallImageSize,
                 message_center::kSmallImageSize);

  smallImage_.reset([[NSImageView alloc] initWithFrame:smallImageFrame]);
  [smallImage_ setImageScaling:NSImageScaleProportionallyUpOrDown];
  [imageBox setContentView:smallImage_];

  return imageBox.autorelease();
}

- (void)configureTitleInFrame:(NSRect)contentFrame {
  contentFrame.size.height = 0;
  title_.reset([self newLabelWithFrame:contentFrame]);
  [title_ setAutoresizingMask:NSViewMinYMargin];
  [title_ setTextColor:gfx::SkColorToCalibratedNSColor(
      message_center::kRegularTextColor)];
  [title_ setFont:[NSFont messageFontOfSize:message_center::kTitleFontSize]];
}

- (void)configureBodyInFrame:(NSRect)contentFrame {
  contentFrame.size.height = 0;
  message_.reset([self newLabelWithFrame:contentFrame]);
  [message_ setAutoresizingMask:NSViewMinYMargin];
  [message_ setTextColor:gfx::SkColorToCalibratedNSColor(
      message_center::kRegularTextColor)];
  [message_ setFont:
      [NSFont messageFontOfSize:message_center::kMessageFontSize]];
}

- (void)configureContextMessageInFrame:(NSRect)contentFrame {
  contentFrame.size.height = 0;
  contextMessage_.reset([self newLabelWithFrame:contentFrame]);
  [contextMessage_ setAutoresizingMask:NSViewMinYMargin];
  [contextMessage_ setTextColor:gfx::SkColorToCalibratedNSColor(
      message_center::kDimTextColor)];
  [contextMessage_ setFont:
      [NSFont messageFontOfSize:message_center::kMessageFontSize]];
}

- (NSTextView*)newLabelWithFrame:(NSRect)frame {
  NSTextView* label = [[NSTextView alloc] initWithFrame:frame];

  // The labels MUST draw their background so that subpixel antialiasing can
  // happen on the text.
  [label setDrawsBackground:YES];
  [label setBackgroundColor:gfx::SkColorToCalibratedNSColor(
      message_center::kNotificationBackgroundColor)];

  [label setEditable:NO];
  [label setSelectable:NO];
  [label setTextContainerInset:NSMakeSize(0.0f, 0.0f)];
  [[label textContainer] setLineFragmentPadding:0.0f];
  return label;
}

- (NSRect)currentContentRect {
  DCHECK(icon_);
  DCHECK(closeButton_);
  DCHECK(smallImage_);

  NSRect iconFrame, contentFrame;
  NSDivideRect([[self view] bounds], &iconFrame, &contentFrame,
      NSWidth([icon_ frame]) + message_center::kIconToTextPadding,
      NSMinXEdge);
  // The content area is between the icon on the left and the control area
  // on the right.
  int controlAreaWidth =
      std::max(NSWidth([closeButton_ frame]), NSWidth([smallImage_ frame]));
  contentFrame.size.width -=
      2 * message_center::kSmallImagePadding + controlAreaWidth;
  return contentFrame;
}

- (base::string16)wrapText:(const base::string16&)text
                   forFont:(NSFont*)nsfont
          maxNumberOfLines:(size_t)lines
               actualLines:(size_t*)actualLines {
  *actualLines = 0;
  if (text.empty() || lines == 0)
    return base::string16();
  gfx::FontList font_list((gfx::Font(nsfont)));
  int width = NSWidth([self currentContentRect]);
  int height = (lines + 1) * font_list.GetHeight();

  std::vector<base::string16> wrapped;
  gfx::ElideRectangleText(text, font_list, width, height,
                          gfx::WRAP_LONG_WORDS, &wrapped);

  // This could be possible when the input text contains only spaces.
  if (wrapped.empty())
    return base::string16();

  if (wrapped.size() > lines) {
    // Add an ellipsis to the last line. If this ellipsis makes the last line
    // too wide, that line will be further elided by the gfx::ElideText below.
    base::string16 last =
        wrapped[lines - 1] + base::UTF8ToUTF16(gfx::kEllipsis);
    if (gfx::GetStringWidth(last, font_list) > width)
      last = gfx::ElideText(last, font_list, width, gfx::ELIDE_TAIL);
    wrapped.resize(lines - 1);
    wrapped.push_back(last);
  }

  *actualLines = wrapped.size();
  return lines == 1 ? wrapped[0] : JoinString(wrapped, '\n');
}

- (base::string16)wrapText:(const base::string16&)text
                   forFont:(NSFont*)nsfont
          maxNumberOfLines:(size_t)lines {
  size_t unused;
  return [self wrapText:text
                forFont:nsfont
       maxNumberOfLines:lines
            actualLines:&unused];
}

@end

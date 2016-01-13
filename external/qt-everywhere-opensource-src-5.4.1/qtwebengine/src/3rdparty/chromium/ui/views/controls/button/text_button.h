// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_BUTTON_TEXT_BUTTON_H_
#define UI_VIEWS_CONTROLS_BUTTON_TEXT_BUTTON_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/custom_button.h"
#include "ui/views/native_theme_delegate.h"
#include "ui/views/painter.h"

namespace views {

// A Border subclass for TextButtons that allows configurable insets for the
// button.
class VIEWS_EXPORT TextButtonBorder : public Border {
 public:
  TextButtonBorder();
  virtual ~TextButtonBorder();

  void SetInsets(const gfx::Insets& insets);

  // Border:
  virtual void Paint(const View& view, gfx::Canvas* canvas) OVERRIDE;
  virtual gfx::Insets GetInsets() const OVERRIDE;
  virtual gfx::Size GetMinimumSize() const OVERRIDE;

 private:
  gfx::Insets insets_;

  DISALLOW_COPY_AND_ASSIGN(TextButtonBorder);
};


// A Border subclass that paints a TextButton's background layer -- basically
// the button frame in the hot/pushed states.
//
// Note that this type of button is not focusable by default and will not be
// part of the focus chain.  Call SetFocusable(true) to make it part of the
// focus chain.
class VIEWS_EXPORT TextButtonDefaultBorder : public TextButtonBorder {
 public:
  TextButtonDefaultBorder();
  virtual ~TextButtonDefaultBorder();

  // TextButtonDefaultBorder takes and retains ownership of these |painter|s.
  void set_normal_painter(Painter* painter) { normal_painter_.reset(painter); }
  void set_hot_painter(Painter* painter) { hot_painter_.reset(painter); }
  void set_pushed_painter(Painter* painter) { pushed_painter_.reset(painter); }

 private:
  // TextButtonBorder:
  virtual void Paint(const View& view, gfx::Canvas* canvas) OVERRIDE;
  virtual gfx::Size GetMinimumSize() const OVERRIDE;

  scoped_ptr<Painter> normal_painter_;
  scoped_ptr<Painter> hot_painter_;
  scoped_ptr<Painter> pushed_painter_;

  int vertical_padding_;

  DISALLOW_COPY_AND_ASSIGN(TextButtonDefaultBorder);
};


// A Border subclass that paints a TextButton's background layer using the
// platform's native theme look.  This handles normal/disabled/hot/pressed
// states, with possible animation between states.
class VIEWS_EXPORT TextButtonNativeThemeBorder : public TextButtonBorder {
 public:
  explicit TextButtonNativeThemeBorder(NativeThemeDelegate* delegate);
  virtual ~TextButtonNativeThemeBorder();

  // TextButtonBorder:
  virtual void Paint(const View& view, gfx::Canvas* canvas) OVERRIDE;
  // We don't override GetMinimumSize(), since there's no easy way to calculate
  // the minimum size required by the various theme components.

 private:
  // The delegate the controls the appearance of this border.
  NativeThemeDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(TextButtonNativeThemeBorder);
};


// A base class for different types of buttons, like push buttons, radio
// buttons, and checkboxes, that do not depend on native components for look and
// feel. TextButton reserves space for the largest string passed to SetText. To
// reset the cached max size invoke ClearMaxTextSize.
class VIEWS_EXPORT TextButtonBase : public CustomButton,
                                    public NativeThemeDelegate {
 public:
  // The menu button's class name.
  static const char kViewClassName[];

  virtual ~TextButtonBase();

  // Call SetText once per string in your set of possible values at button
  // creation time, so that it can contain the largest of them and avoid
  // resizing the button when the text changes.
  virtual void SetText(const base::string16& text);
  const base::string16& text() const { return text_; }

  enum TextAlignment {
    ALIGN_LEFT,
    ALIGN_CENTER,
    ALIGN_RIGHT
  };

  void set_alignment(TextAlignment alignment) { alignment_ = alignment; }

  const gfx::Animation* GetAnimation() const;

  void SetIsDefault(bool is_default);
  bool is_default() const { return is_default_; }

  // Set whether the button text can wrap on multiple lines.
  // Default is false.
  void SetMultiLine(bool multi_line);

  // Return whether the button text can wrap on multiple lines.
  bool multi_line() const { return multi_line_; }

  // TextButton remembers the maximum display size of the text passed to
  // SetText. This method resets the cached maximum display size to the
  // current size.
  void ClearMaxTextSize();

  void set_min_width(int min_width) { min_width_ = min_width; }
  void set_min_height(int min_height) { min_height_ = min_height; }
  void set_max_width(int max_width) { max_width_ = max_width; }
  const gfx::FontList& font_list() const { return font_list_; }
  void SetFontList(const gfx::FontList& font_list);

  void SetEnabledColor(SkColor color);
  void SetDisabledColor(SkColor color);
  void SetHighlightColor(SkColor color);
  void SetHoverColor(SkColor color);

  // Sets whether or not to show the hot and pushed states for the button icon
  // (if present) in addition to the normal state.  Defaults to true.
  bool show_multiple_icon_states() const { return show_multiple_icon_states_; }
  void SetShowMultipleIconStates(bool show_multiple_icon_states);

  void SetFocusPainter(scoped_ptr<Painter> focus_painter);
  Painter* focus_painter() { return focus_painter_.get(); }

  // Paint the button into the specified canvas. If |mode| is |PB_FOR_DRAG|, the
  // function paints a drag image representation into the canvas.
  enum PaintButtonMode { PB_NORMAL, PB_FOR_DRAG };
  virtual void PaintButton(gfx::Canvas* canvas, PaintButtonMode mode);

  // Overridden from View:
  virtual gfx::Size GetPreferredSize() const OVERRIDE;
  virtual gfx::Size GetMinimumSize() const OVERRIDE;
  virtual int GetHeightForWidth(int w) const OVERRIDE;
  virtual void OnEnabledChanged() OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual void OnBoundsChanged(const gfx::Rect& previous_bounds) OVERRIDE;
  virtual const char* GetClassName() const OVERRIDE;
  virtual void OnNativeThemeChanged(const ui::NativeTheme* theme) OVERRIDE;

 protected:
  TextButtonBase(ButtonListener* listener, const base::string16& text);

  // Called when enabled or disabled state changes, or the colors for those
  // states change.
  virtual void UpdateColor();

  // Updates text_size_ and max_text_size_ from the current text/font. This is
  // invoked when the font list or text changes.
  void UpdateTextSize();

  // Calculate the size of the text size without setting any of the members.
  void CalculateTextSize(gfx::Size* text_size, int max_width) const;

  // Paint the button's text into the specified canvas. If |mode| is
  // |PB_FOR_DRAG|, the function paints a drag image representation. Derived
  // can override this function to change only the text rendering.
  virtual void OnPaintText(gfx::Canvas* canvas, PaintButtonMode mode);

  void set_color_enabled(SkColor color) { color_enabled_ = color; }
  void set_color_disabled(SkColor color) { color_disabled_ = color; }
  void set_color_hover(SkColor color) { color_hover_ = color; }

  bool use_enabled_color_from_theme() const {
    return use_enabled_color_from_theme_;
  }

  bool use_disabled_color_from_theme() const {
    return use_disabled_color_from_theme_;
  }

  bool use_hover_color_from_theme() const {
    return use_hover_color_from_theme_;
  }

  // Overridden from NativeThemeDelegate:
  virtual gfx::Rect GetThemePaintRect() const OVERRIDE;
  virtual ui::NativeTheme::State GetThemeState(
      ui::NativeTheme::ExtraParams* params) const OVERRIDE;
  virtual const gfx::Animation* GetThemeAnimation() const OVERRIDE;
  virtual ui::NativeTheme::State GetBackgroundThemeState(
      ui::NativeTheme::ExtraParams* params) const OVERRIDE;
  virtual ui::NativeTheme::State GetForegroundThemeState(
      ui::NativeTheme::ExtraParams* params) const OVERRIDE;

  // Overridden from View:
  virtual void OnFocus() OVERRIDE;
  virtual void OnBlur() OVERRIDE;

  virtual void GetExtraParams(ui::NativeTheme::ExtraParams* params) const;

  virtual gfx::Rect GetTextBounds() const;

  int ComputeCanvasStringFlags() const;

  // Calculate the bounds of the content of this button, including any extra
  // width needed on top of the text width.
  gfx::Rect GetContentBounds(int extra_width) const;

  // The text string that is displayed in the button.
  base::string16 text_;

  // The size of the text string.
  mutable gfx::Size text_size_;

  // Track the size of the largest text string seen so far, so that
  // changing text_ will not resize the button boundary.
  gfx::Size max_text_size_;

  // The alignment of the text string within the button.
  TextAlignment alignment_;

  // The font list used to paint the text.
  gfx::FontList font_list_;

  // The dimensions of the button will be at least these values.
  int min_width_;
  int min_height_;

  // The width of the button will never be larger than this value. A value <= 0
  // indicates the width is not constrained.
  int max_width_;

  // Whether or not to show the hot and pushed icon states.
  bool show_multiple_icon_states_;

  // Whether or not the button appears and behaves as the default button in its
  // current context.
  bool is_default_;

  // Whether the text button should handle its text string as multi-line.
  bool multi_line_;

 private:
  // Text color.
  SkColor color_;

  // State colors.
  SkColor color_enabled_;
  SkColor color_disabled_;
  SkColor color_highlight_;
  SkColor color_hover_;

  // True if the specified color should be used from the theme.
  bool use_enabled_color_from_theme_;
  bool use_disabled_color_from_theme_;
  bool use_highlight_color_from_theme_;
  bool use_hover_color_from_theme_;

  scoped_ptr<Painter> focus_painter_;

  DISALLOW_COPY_AND_ASSIGN(TextButtonBase);
};


// A button which displays text and/or and icon that can be changed in response
// to actions. TextButton reserves space for the largest string passed to
// SetText. To reset the cached max size invoke ClearMaxTextSize.
class VIEWS_EXPORT TextButton : public TextButtonBase {
 public:
  // The button's class name.
  static const char kViewClassName[];

  TextButton(ButtonListener* listener, const base::string16& text);
  virtual ~TextButton();

  void set_icon_text_spacing(int icon_text_spacing) {
    icon_text_spacing_ = icon_text_spacing;
  }

  // Sets the icon.
  virtual void SetIcon(const gfx::ImageSkia& icon);
  virtual void SetHoverIcon(const gfx::ImageSkia& icon);
  virtual void SetPushedIcon(const gfx::ImageSkia& icon);

  bool HasIcon() const { return !icon_.isNull(); }

  // Meanings are reversed for right-to-left layouts.
  enum IconPlacement {
    ICON_ON_LEFT,
    ICON_ON_RIGHT,
    ICON_CENTERED  // Centered is valid only when text is empty.
  };

  IconPlacement icon_placement() { return icon_placement_; }
  void set_icon_placement(IconPlacement icon_placement) {
    // ICON_CENTERED works only when |text_| is empty.
    DCHECK((icon_placement != ICON_CENTERED) || text_.empty());
    icon_placement_ = icon_placement;
  }

  void set_ignore_minimum_size(bool ignore_minimum_size);

  void set_full_justification(bool full_justification);

  // Overridden from View:
  virtual gfx::Size GetPreferredSize() const OVERRIDE;
  virtual const char* GetClassName() const OVERRIDE;

  // Overridden from TextButtonBase:
  virtual void PaintButton(gfx::Canvas* canvas, PaintButtonMode mode) OVERRIDE;

 protected:
  // Paint the button's icon into the specified canvas. If |mode| is
  // |PB_FOR_DRAG|, the function paints a drag image representation. Derived
  // can override this function to change only the icon rendering.
  virtual void OnPaintIcon(gfx::Canvas* canvas, PaintButtonMode mode);

  gfx::ImageSkia icon() const { return icon_; }

  virtual const gfx::ImageSkia& GetImageToPaint() const;

  // Overridden from NativeThemeDelegate:
  virtual ui::NativeTheme::Part GetThemePart() const OVERRIDE;

  // Overridden from TextButtonBase:
  virtual void GetExtraParams(
      ui::NativeTheme::ExtraParams* params) const OVERRIDE;
  virtual gfx::Rect GetTextBounds() const OVERRIDE;

 private:
  // The position of the icon.
  IconPlacement icon_placement_;

  // An icon displayed with the text.
  gfx::ImageSkia icon_;

  // An optional different version of the icon for hover state.
  gfx::ImageSkia icon_hover_;
  bool has_hover_icon_;

  // An optional different version of the icon for pushed state.
  gfx::ImageSkia icon_pushed_;
  bool has_pushed_icon_;

  // Space between icon and text.
  int icon_text_spacing_;

  // True if the button should ignore the minimum size for the platform. Default
  // is true. Set to false to prevent narrower buttons.
  bool ignore_minimum_size_;

  // True if the icon and the text are aligned along both the left and right
  // margins of the button.
  bool full_justification_;

  DISALLOW_COPY_AND_ASSIGN(TextButton);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_BUTTON_TEXT_BUTTON_H_

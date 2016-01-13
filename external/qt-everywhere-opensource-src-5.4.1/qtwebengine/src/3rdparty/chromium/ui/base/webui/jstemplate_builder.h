// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This provides some helper methods for building and rendering an
// internal html page.  The flow is as follows:
// - instantiate a builder given a webframe that we're going to render content
//   into
// - load the template html and load the jstemplate javascript into the frame
// - given a json data object, run the jstemplate javascript which fills in
//   template values

#ifndef UI_BASE_WEBUI_JSTEMPLATE_BUILDER_H_
#define UI_BASE_WEBUI_JSTEMPLATE_BUILDER_H_

#include <string>

#include "base/strings/string_piece.h"
#include "ui/base/ui_base_export.h"

namespace base {
class DictionaryValue;
}

namespace webui {

// While an object of this class is in scope, the template builder will output
// version 2 html. Version 2 uses load_time_data.js and i18n_template2.js, and
// should soon become the default.
class UI_BASE_EXPORT UseVersion2 {
 public:
  UseVersion2();
  ~UseVersion2();

 private:
  DISALLOW_COPY_AND_ASSIGN(UseVersion2);
};

// A helper function that generates a string of HTML to be loaded.  The
// string includes the HTML and the javascript code necessary to generate the
// full page with support for JsTemplates.
UI_BASE_EXPORT std::string GetTemplateHtml(
    const base::StringPiece& html_template,
    const base::DictionaryValue* json,
    const base::StringPiece& template_id);

// A helper function that generates a string of HTML to be loaded.  The
// string includes the HTML and the javascript code necessary to generate the
// full page with support for i18n Templates.
UI_BASE_EXPORT std::string GetI18nTemplateHtml(
    const base::StringPiece& html_template,
    const base::DictionaryValue* json);

// A helper function that generates a string of HTML to be loaded.  The
// string includes the HTML and the javascript code necessary to generate the
// full page with support for both i18n Templates and JsTemplates.
UI_BASE_EXPORT std::string GetTemplatesHtml(
    const base::StringPiece& html_template,
    const base::DictionaryValue* json,
    const base::StringPiece& template_id);

// The following functions build up the different parts that the above
// templates use.

// Appends a script tag with a variable name |templateData| that has the JSON
// assigned to it.
UI_BASE_EXPORT void AppendJsonHtml(const base::DictionaryValue* json,
                                   std::string* output);

// Same as AppendJsonHtml(), except does not include the <script></script>
// tag wrappers.
UI_BASE_EXPORT void AppendJsonJS(const base::DictionaryValue* json,
                                 std::string* output);

// Appends the source for JsTemplates in a script tag.
UI_BASE_EXPORT void AppendJsTemplateSourceHtml(std::string* output);

// Appends the code that processes the JsTemplate with the JSON. You should
// call AppendJsTemplateSourceHtml and AppendJsonHtml before calling this.
UI_BASE_EXPORT void AppendJsTemplateProcessHtml(
    const base::StringPiece& template_id,
    std::string* output);

// Appends the source for i18n Templates in a script tag.
UI_BASE_EXPORT void AppendI18nTemplateSourceHtml(std::string* output);

// Appends the code that processes the i18n Template with the JSON. You
// should call AppendJsTemplateSourceHtml and AppendJsonHtml before calling
// this.
UI_BASE_EXPORT void AppendI18nTemplateProcessHtml(std::string* output);

}  // namespace webui

#endif  // UI_BASE_WEBUI_JSTEMPLATE_BUILDER_H_

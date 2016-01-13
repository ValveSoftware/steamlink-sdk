// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A helper function for using JsTemplate. See jstemplate_builder.h for more
// info.

#include "ui/base/webui/jstemplate_builder.h"

#include "base/json/json_file_value_serializer.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "grit/webui_resources.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

// Non-zero when building version 2 templates. See UseVersion2 class.
int g_version2 = 0;

}  // namespace

namespace webui {

UseVersion2::UseVersion2() {
  g_version2++;
}

UseVersion2::~UseVersion2() {
  g_version2--;
}

std::string GetTemplateHtml(const base::StringPiece& html_template,
                            const base::DictionaryValue* json,
                            const base::StringPiece& template_id) {
  std::string output(html_template.data(), html_template.size());
  AppendJsonHtml(json, &output);
  AppendJsTemplateSourceHtml(&output);
  AppendJsTemplateProcessHtml(template_id, &output);
  return output;
}

std::string GetI18nTemplateHtml(const base::StringPiece& html_template,
                                const base::DictionaryValue* json) {
  std::string output(html_template.data(), html_template.size());
  AppendJsonHtml(json, &output);
  AppendI18nTemplateSourceHtml(&output);
  AppendI18nTemplateProcessHtml(&output);
  return output;
}

std::string GetTemplatesHtml(const base::StringPiece& html_template,
                             const base::DictionaryValue* json,
                             const base::StringPiece& template_id) {
  std::string output(html_template.data(), html_template.size());
  AppendI18nTemplateSourceHtml(&output);
  AppendJsTemplateSourceHtml(&output);
  AppendJsonHtml(json, &output);
  AppendI18nTemplateProcessHtml(&output);
  AppendJsTemplateProcessHtml(template_id, &output);
  return output;
}

void AppendJsonHtml(const base::DictionaryValue* json, std::string* output) {
  std::string javascript_string;
  AppendJsonJS(json, &javascript_string);

  // </ confuses the HTML parser because it could be a </script> tag.  So we
  // replace </ with <\/.  The extra \ will be ignored by the JS engine.
  ReplaceSubstringsAfterOffset(&javascript_string, 0, "</", "<\\/");

  output->append("<script>");
  output->append(javascript_string);
  output->append("</script>");
}

void AppendJsonJS(const base::DictionaryValue* json, std::string* output) {
  // Convert the template data to a json string.
  DCHECK(json) << "must include json data structure";

  std::string jstext;
  JSONStringValueSerializer serializer(&jstext);
  serializer.Serialize(*json);
  output->append(g_version2 ? "loadTimeData.data = " : "var templateData = ");
  output->append(jstext);
  output->append(";");
}

void AppendJsTemplateSourceHtml(std::string* output) {
  // fetch and cache the pointer of the jstemplate resource source text.
  static const base::StringPiece jstemplate_src(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_WEBUI_JSTEMPLATE_JS));

  if (jstemplate_src.empty()) {
    NOTREACHED() << "Unable to get jstemplate src";
    return;
  }

  output->append("<script>");
  output->append(jstemplate_src.data(), jstemplate_src.size());
  output->append("</script>");
}

void AppendJsTemplateProcessHtml(const base::StringPiece& template_id,
                                 std::string* output) {
  output->append("<script>");
  output->append("var tp = document.getElementById('");
  output->append(template_id.data(), template_id.size());
  output->append("');");
  output->append("jstProcess(new JsEvalContext(templateData), tp);");
  output->append("</script>");
}

void AppendI18nTemplateSourceHtml(std::string* output) {
  // fetch and cache the pointer of the jstemplate resource source text.
  static const base::StringPiece i18n_template_src(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_WEBUI_I18N_TEMPLATE_JS));
  static const base::StringPiece i18n_template2_src(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_WEBUI_I18N_TEMPLATE2_JS));
  const base::StringPiece* template_src = g_version2 ?
      &i18n_template2_src : &i18n_template_src;

  if (template_src->empty()) {
    NOTREACHED() << "Unable to get i18n template src";
    return;
  }

  output->append("<script>");
  output->append(template_src->data(), template_src->size());
  output->append("</script>");
}

void AppendI18nTemplateProcessHtml(std::string* output) {
  if (g_version2)
    return;

  static const base::StringPiece i18n_process_src(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_WEBUI_I18N_PROCESS_JS));

  if (i18n_process_src.empty()) {
    NOTREACHED() << "Unable to get i18n process src";
    return;
  }

  output->append("<script>");
  output->append(i18n_process_src.data(), i18n_process_src.size());
  output->append("</script>");
}

}  // namespace webui

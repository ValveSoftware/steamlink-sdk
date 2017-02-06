// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_auralinux.h"

#include <stdint.h>
#include <string.h>

#include "base/strings/utf_string_conversions.h"
#include "content/browser/accessibility/browser_accessibility_manager_auralinux.h"
#include "content/common/accessibility_messages.h"

namespace content {

static gpointer browser_accessibility_parent_class = NULL;

static BrowserAccessibilityAuraLinux* ToBrowserAccessibilityAuraLinux(
    BrowserAccessibilityAtk* atk_object) {
  if (!atk_object)
    return NULL;

  return atk_object->m_object;
}

const BrowserAccessibilityAuraLinux* ToBrowserAccessibilityAuraLinux(
    const BrowserAccessibility* obj) {
  DCHECK(!obj || obj->IsNative());
  return static_cast<const BrowserAccessibilityAuraLinux*>(obj);
}

BrowserAccessibilityAuraLinux* ToBrowserAccessibilityAuraLinux(
    BrowserAccessibility* obj) {
  DCHECK(!obj || obj->IsNative());
  return static_cast<BrowserAccessibilityAuraLinux*>(obj);
}

//
// AtkAction interface.
//

static BrowserAccessibilityAuraLinux* ToBrowserAccessibilityAuraLinux(
    AtkAction* atk_action) {
  if (!IS_BROWSER_ACCESSIBILITY(atk_action))
    return NULL;

  return ToBrowserAccessibilityAuraLinux(BROWSER_ACCESSIBILITY(atk_action));
}

static gboolean browser_accessibility_do_action(AtkAction* atk_action,
                                                gint index) {
  g_return_val_if_fail(ATK_IS_ACTION(atk_action), FALSE);
  g_return_val_if_fail(!index, FALSE);

  BrowserAccessibilityAuraLinux* obj =
      ToBrowserAccessibilityAuraLinux(atk_action);
  if (!obj)
    return FALSE;

  obj->manager()->DoDefaultAction(*obj);

  return TRUE;
}

static gint browser_accessibility_get_n_actions(AtkAction* atk_action) {
  g_return_val_if_fail(ATK_IS_ACTION(atk_action), 0);

  BrowserAccessibilityAuraLinux* obj =
      ToBrowserAccessibilityAuraLinux(atk_action);
  if (!obj)
    return 0;

  return 1;
}

static const gchar* browser_accessibility_get_description(AtkAction* atk_action,
                                                          gint) {
  g_return_val_if_fail(ATK_IS_ACTION(atk_action), 0);
  BrowserAccessibilityAuraLinux* obj =
      ToBrowserAccessibilityAuraLinux(atk_action);
  if (!obj)
    return 0;

  return 0;
}

static const gchar* browser_accessibility_get_name(AtkAction* atk_action,
                                                   gint index) {
  g_return_val_if_fail(ATK_IS_ACTION(atk_action), 0);
  g_return_val_if_fail(!index, 0);
  BrowserAccessibilityAuraLinux* obj =
      ToBrowserAccessibilityAuraLinux(atk_action);
  if (!obj)
    return 0;

  return obj->GetStringAttribute(ui::AX_ATTR_ACTION).c_str();
}

static const gchar* browser_accessibility_get_keybinding(AtkAction* atk_action,
                                                         gint index) {
  g_return_val_if_fail(ATK_IS_ACTION(atk_action), 0);
  g_return_val_if_fail(!index, 0);
  BrowserAccessibilityAuraLinux* obj =
      ToBrowserAccessibilityAuraLinux(atk_action);
  if (!obj)
    return 0;

  return obj->GetStringAttribute(ui::AX_ATTR_ACCESS_KEY).c_str();
}

static void ActionInterfaceInit(AtkActionIface* iface) {
  iface->do_action = browser_accessibility_do_action;
  iface->get_n_actions = browser_accessibility_get_n_actions;
  iface->get_description = browser_accessibility_get_description;
  iface->get_name = browser_accessibility_get_name;
  iface->get_keybinding = browser_accessibility_get_keybinding;
}

static const GInterfaceInfo ActionInfo = {
    reinterpret_cast<GInterfaceInitFunc>(ActionInterfaceInit), 0, 0};

//
// AtkComponent interface.
//

static BrowserAccessibilityAuraLinux* ToBrowserAccessibilityAuraLinux(
    AtkComponent* atk_component) {
  if (!IS_BROWSER_ACCESSIBILITY(atk_component))
    return NULL;

  return ToBrowserAccessibilityAuraLinux(BROWSER_ACCESSIBILITY(atk_component));
}

static AtkObject* browser_accessibility_accessible_at_point(
    AtkComponent* atk_component,
    gint x,
    gint y,
    AtkCoordType coord_type) {
  g_return_val_if_fail(ATK_IS_COMPONENT(atk_component), 0);

  BrowserAccessibilityAuraLinux* obj =
      ToBrowserAccessibilityAuraLinux(atk_component);
  if (!obj)
    return NULL;

  gfx::Point point(x, y);
  if (!obj->GetGlobalBoundsRect().Contains(point))
    return NULL;

  BrowserAccessibility* result = obj->BrowserAccessibilityForPoint(point);
  if (!result)
    return NULL;

  AtkObject* atk_result =
      ToBrowserAccessibilityAuraLinux(result)->GetAtkObject();
  g_object_ref(atk_result);
  return atk_result;
}

static void browser_accessibility_get_extents(AtkComponent* atk_component,
                                              gint* x,
                                              gint* y,
                                              gint* width,
                                              gint* height,
                                              AtkCoordType coord_type) {
  g_return_if_fail(ATK_IS_COMPONENT(atk_component));

  BrowserAccessibilityAuraLinux* obj =
      ToBrowserAccessibilityAuraLinux(atk_component);
  if (!obj)
    return;

  gfx::Rect bounds = obj->GetGlobalBoundsRect();
  if (x)
    *x = bounds.x();
  if (y)
    *y = bounds.y();
  if (width)
    *width = bounds.width();
  if (height)
    *height = bounds.height();
}

static gboolean browser_accessibility_grab_focus(AtkComponent* atk_component) {
  g_return_val_if_fail(ATK_IS_COMPONENT(atk_component), FALSE);

  BrowserAccessibilityAuraLinux* obj =
      ToBrowserAccessibilityAuraLinux(atk_component);
  if (!obj)
    return false;

  obj->manager()->SetFocus(*obj);
  return true;
}

static void ComponentInterfaceInit(AtkComponentIface* iface) {
  iface->ref_accessible_at_point = browser_accessibility_accessible_at_point;
  iface->get_extents = browser_accessibility_get_extents;
  iface->grab_focus = browser_accessibility_grab_focus;
}

static const GInterfaceInfo ComponentInfo = {
    reinterpret_cast<GInterfaceInitFunc>(ComponentInterfaceInit),
    0,
    0};

//
// AtkDocument interface.
//

static BrowserAccessibilityAuraLinux* ToBrowserAccessibilityAuraLinux(
    AtkDocument* atk_doc) {
  if (!IS_BROWSER_ACCESSIBILITY(atk_doc))
    return NULL;

  return ToBrowserAccessibilityAuraLinux(BROWSER_ACCESSIBILITY(atk_doc));
}

static const gchar* GetDocumentAttributeValue(
    BrowserAccessibilityAuraLinux* obj,
    const gchar* attribute) {
  if (!g_ascii_strcasecmp(attribute, "DocType"))
    return obj->manager()->GetTreeData().doctype.c_str();
  else if (!g_ascii_strcasecmp(attribute, "MimeType"))
    return obj->manager()->GetTreeData().mimetype.c_str();
  else if (!g_ascii_strcasecmp(attribute, "Title"))
    return obj->manager()->GetTreeData().title.c_str();
  else if (!g_ascii_strcasecmp(attribute, "URI"))
    return obj->manager()->GetTreeData().url.c_str();

  return 0;
}

AtkAttributeSet* SetAtkAttributeSet(AtkAttributeSet* attribute_set,
                                    const char* name,
                                    const char* value) {
  AtkAttribute* attribute =
      static_cast<AtkAttribute*>(g_malloc(sizeof(AtkAttribute)));
  attribute->name = g_strdup(name);
  attribute->value = g_strdup(value);
  attribute_set = g_slist_prepend(attribute_set, attribute);
  return attribute_set;
}

static const gchar* browser_accessibility_get_attribute_value(
    AtkDocument* atk_doc,
    const gchar* attribute) {
  g_return_val_if_fail(ATK_IS_DOCUMENT(atk_doc), 0);
  BrowserAccessibilityAuraLinux* obj = ToBrowserAccessibilityAuraLinux(atk_doc);
  if (!obj)
    return 0;

  return GetDocumentAttributeValue(obj, attribute);
}

static AtkAttributeSet* browser_accessibility_get_attributes(
    AtkDocument* atk_doc) {
  g_return_val_if_fail(ATK_IS_DOCUMENT(atk_doc), 0);
  BrowserAccessibilityAuraLinux* obj = ToBrowserAccessibilityAuraLinux(atk_doc);
  if (!obj)
    return 0;

  AtkAttributeSet* attribute_set = 0;
  const gchar* doc_attributes[] = {"DocType", "MimeType", "Title", "URI"};
  const gchar* value = 0;

  for (unsigned i = 0; i < G_N_ELEMENTS(doc_attributes); i++) {
    value = GetDocumentAttributeValue(obj, doc_attributes[i]);
    if (value)
      attribute_set =
          SetAtkAttributeSet(attribute_set, doc_attributes[i], value);
  }

  return attribute_set;
}

static void DocumentInterfaceInit(AtkDocumentIface* iface) {
  iface->get_document_attribute_value =
      browser_accessibility_get_attribute_value;
  iface->get_document_attributes = browser_accessibility_get_attributes;
}

static const GInterfaceInfo DocumentInfo = {
    reinterpret_cast<GInterfaceInitFunc>(DocumentInterfaceInit), 0, 0};

//
// AtkImage interface.
//

static BrowserAccessibilityAuraLinux* ToBrowserAccessibilityAuraLinux(
    AtkImage* atk_img) {
  if (!IS_BROWSER_ACCESSIBILITY(atk_img))
    return NULL;

  return ToBrowserAccessibilityAuraLinux(BROWSER_ACCESSIBILITY(atk_img));
}

void GetImagePositionSize(BrowserAccessibilityAuraLinux* obj,
                          gint* x,
                          gint* y,
                          gint* width,
                          gint* height) {
  gfx::Rect img_pos_size = obj->GetGlobalBoundsRect();

  if (x)
    *x = img_pos_size.x();
  if (y)
    *y = img_pos_size.y();
  if (width)
    *width = img_pos_size.width();
  if (height)
    *height = img_pos_size.height();
}

static void browser_accessibility_get_image_position(AtkImage* atk_img,
                                                     gint* x,
                                                     gint* y,
                                                     AtkCoordType coordType) {
  g_return_if_fail(ATK_IMAGE(atk_img));
  BrowserAccessibilityAuraLinux* obj = ToBrowserAccessibilityAuraLinux(atk_img);
  if (!obj)
    return;

  GetImagePositionSize(obj, x, y, nullptr, nullptr);
}

static const gchar* browser_accessibility_get_image_description(
    AtkImage* atk_img) {
  g_return_val_if_fail(ATK_IMAGE(atk_img), 0);
  BrowserAccessibilityAuraLinux* obj = ToBrowserAccessibilityAuraLinux(atk_img);
  if (!obj)
    return 0;

  return obj->GetStringAttribute(ui::AX_ATTR_DESCRIPTION).c_str();
}

static void browser_accessibility_get_image_size(AtkImage* atk_img,
                                                 gint* width,
                                                 gint* height) {
  g_return_if_fail(ATK_IMAGE(atk_img));
  BrowserAccessibilityAuraLinux* obj = ToBrowserAccessibilityAuraLinux(atk_img);
  if (!obj)
    return;

  GetImagePositionSize(obj, nullptr, nullptr, width, height);
}

void ImageInterfaceInit(AtkImageIface* iface) {
  iface->get_image_position = browser_accessibility_get_image_position;
  iface->get_image_description = browser_accessibility_get_image_description;
  iface->get_image_size = browser_accessibility_get_image_size;
}

static const GInterfaceInfo ImageInfo = {
    reinterpret_cast<GInterfaceInitFunc>(ImageInterfaceInit), 0, 0};

//
// AtkValue interface.
//

static BrowserAccessibilityAuraLinux* ToBrowserAccessibilityAuraLinux(
    AtkValue* atk_object) {
  if (!IS_BROWSER_ACCESSIBILITY(atk_object))
    return NULL;

  return ToBrowserAccessibilityAuraLinux(BROWSER_ACCESSIBILITY(atk_object));
}

static float GetStepAttribute(BrowserAccessibilityAuraLinux* obj) {
  // TODO(shreeram.k): Get Correct value of Step attribute.
  return 1.0;
}

static void browser_accessibility_get_current_value(AtkValue* atk_value,
                                                    GValue* value) {
  g_return_if_fail(ATK_VALUE(atk_value));

  BrowserAccessibilityAuraLinux* obj =
      ToBrowserAccessibilityAuraLinux(atk_value);
  if (!obj)
    return;

  float float_val;
  if (obj->GetFloatAttribute(ui::AX_ATTR_VALUE_FOR_RANGE, &float_val)) {
    memset(value, 0, sizeof(*value));
    g_value_init(value, G_TYPE_FLOAT);
    g_value_set_float(value, float_val);
  }
}

static void browser_accessibility_get_minimum_value(AtkValue* atk_value,
                                                    GValue* value) {
  g_return_if_fail(ATK_VALUE(atk_value));

  BrowserAccessibilityAuraLinux* obj =
      ToBrowserAccessibilityAuraLinux(atk_value);
  if (!obj)
    return;

  float float_val;
  if (obj->GetFloatAttribute(ui::AX_ATTR_MIN_VALUE_FOR_RANGE, &float_val)) {
    memset(value, 0, sizeof(*value));
    g_value_init(value, G_TYPE_FLOAT);
    g_value_set_float(value, float_val);
  }
}

static void browser_accessibility_get_maximum_value(AtkValue* atk_value,
                                                    GValue* value) {
  g_return_if_fail(ATK_VALUE(atk_value));

  BrowserAccessibilityAuraLinux* obj =
      ToBrowserAccessibilityAuraLinux(atk_value);
  if (!obj)
    return;

  float float_val;
  if (obj->GetFloatAttribute(ui::AX_ATTR_MAX_VALUE_FOR_RANGE, &float_val)) {
    memset(value, 0, sizeof(*value));
    g_value_init(value, G_TYPE_FLOAT);
    g_value_set_float(value, float_val);
  }
}

static void browser_accessibility_get_minimum_increment(AtkValue* atk_value,
                                                        GValue* gValue) {
  g_return_if_fail(ATK_VALUE(atk_value));

  memset(gValue, 0, sizeof(GValue));
  g_value_init(gValue, G_TYPE_FLOAT);

  BrowserAccessibilityAuraLinux* obj =
      ToBrowserAccessibilityAuraLinux(atk_value);
  if (!obj)
    return;

  g_value_set_float(gValue, GetStepAttribute(obj));
}

static void ValueInterfaceInit(AtkValueIface* iface) {
  iface->get_current_value = browser_accessibility_get_current_value;
  iface->get_minimum_value = browser_accessibility_get_minimum_value;
  iface->get_maximum_value = browser_accessibility_get_maximum_value;
  iface->get_minimum_increment = browser_accessibility_get_minimum_increment;
}

static const GInterfaceInfo ValueInfo = {
    reinterpret_cast<GInterfaceInitFunc>(ValueInterfaceInit),
    0,
    0};

//
// AtkObject interface
//

static BrowserAccessibilityAuraLinux* ToBrowserAccessibilityAuraLinux(
    AtkObject* atk_object) {
  if (!IS_BROWSER_ACCESSIBILITY(atk_object))
    return NULL;

  return ToBrowserAccessibilityAuraLinux(BROWSER_ACCESSIBILITY(atk_object));
}

static const gchar* browser_accessibility_get_name(AtkObject* atk_object) {
  BrowserAccessibilityAuraLinux* obj =
      ToBrowserAccessibilityAuraLinux(atk_object);
  if (!obj)
    return NULL;

  return obj->GetStringAttribute(ui::AX_ATTR_NAME).c_str();
}

static const gchar* browser_accessibility_get_description(
    AtkObject* atk_object) {
  BrowserAccessibilityAuraLinux* obj =
      ToBrowserAccessibilityAuraLinux(atk_object);
  if (!obj)
    return NULL;

  return obj->GetStringAttribute(ui::AX_ATTR_DESCRIPTION).c_str();
}

static AtkObject* browser_accessibility_get_parent(AtkObject* atk_object) {
  BrowserAccessibilityAuraLinux* obj =
      ToBrowserAccessibilityAuraLinux(atk_object);
  if (!obj)
    return NULL;
  if (obj->GetParent())
    return ToBrowserAccessibilityAuraLinux(obj->GetParent())->GetAtkObject();

  BrowserAccessibilityManagerAuraLinux* manager =
      static_cast<BrowserAccessibilityManagerAuraLinux*>(obj->manager());
  return manager->parent_object();
}

static gint browser_accessibility_get_n_children(AtkObject* atk_object) {
  BrowserAccessibilityAuraLinux* obj =
      ToBrowserAccessibilityAuraLinux(atk_object);
  if (!obj)
    return 0;

  return obj->PlatformChildCount();
}

static AtkObject* browser_accessibility_ref_child(AtkObject* atk_object,
                                                  gint index) {
  BrowserAccessibilityAuraLinux* obj =
      ToBrowserAccessibilityAuraLinux(atk_object);
  if (!obj)
    return NULL;

  if (index < 0 || index >= static_cast<gint>(obj->PlatformChildCount()))
    return NULL;

  AtkObject* result = ToBrowserAccessibilityAuraLinux(
      obj->InternalGetChild(index))->GetAtkObject();
  g_object_ref(result);
  return result;
}

static gint browser_accessibility_get_index_in_parent(AtkObject* atk_object) {
  BrowserAccessibilityAuraLinux* obj =
      ToBrowserAccessibilityAuraLinux(atk_object);
  if (!obj)
    return 0;
  return obj->GetIndexInParent();
}

static AtkAttributeSet* browser_accessibility_get_attributes(
    AtkObject* atk_object) {
  return NULL;
}

static AtkRole browser_accessibility_get_role(AtkObject* atk_object) {
  BrowserAccessibilityAuraLinux* obj =
      ToBrowserAccessibilityAuraLinux(atk_object);
  if (!obj)
    return ATK_ROLE_INVALID;
  return obj->atk_role();
}

static AtkStateSet* browser_accessibility_ref_state_set(AtkObject* atk_object) {
  BrowserAccessibilityAuraLinux* obj =
      ToBrowserAccessibilityAuraLinux(atk_object);
  if (!obj)
    return NULL;
  AtkStateSet* state_set = ATK_OBJECT_CLASS(browser_accessibility_parent_class)
                               ->ref_state_set(atk_object);
  int32_t state = obj->GetState();

  if (state & (1 << ui::AX_STATE_FOCUSABLE))
    atk_state_set_add_state(state_set, ATK_STATE_FOCUSABLE);
  if (obj->manager()->GetFocus() == obj)
    atk_state_set_add_state(state_set, ATK_STATE_FOCUSED);
  if (state & (1 << ui::AX_STATE_ENABLED))
    atk_state_set_add_state(state_set, ATK_STATE_ENABLED);

  return state_set;
}

static AtkRelationSet* browser_accessibility_ref_relation_set(
    AtkObject* atk_object) {
  AtkRelationSet* relation_set =
      ATK_OBJECT_CLASS(browser_accessibility_parent_class)
          ->ref_relation_set(atk_object);
  return relation_set;
}

//
// The rest of the BrowserAccessibilityAuraLinux code, not specific to one
// of the Atk* interfaces.
//

static void browser_accessibility_init(AtkObject* atk_object, gpointer data) {
  if (ATK_OBJECT_CLASS(browser_accessibility_parent_class)->initialize) {
    ATK_OBJECT_CLASS(browser_accessibility_parent_class)
        ->initialize(atk_object, data);
  }

  BROWSER_ACCESSIBILITY(atk_object)->m_object =
      reinterpret_cast<BrowserAccessibilityAuraLinux*>(data);
}

static void browser_accessibility_finalize(GObject* atk_object) {
  G_OBJECT_CLASS(browser_accessibility_parent_class)->finalize(atk_object);
}

static void browser_accessibility_class_init(AtkObjectClass* klass) {
  GObjectClass* gobject_class = G_OBJECT_CLASS(klass);
  browser_accessibility_parent_class = g_type_class_peek_parent(klass);

  gobject_class->finalize = browser_accessibility_finalize;
  klass->initialize = browser_accessibility_init;
  klass->get_name = browser_accessibility_get_name;
  klass->get_description = browser_accessibility_get_description;
  klass->get_parent = browser_accessibility_get_parent;
  klass->get_n_children = browser_accessibility_get_n_children;
  klass->ref_child = browser_accessibility_ref_child;
  klass->get_role = browser_accessibility_get_role;
  klass->ref_state_set = browser_accessibility_ref_state_set;
  klass->get_index_in_parent = browser_accessibility_get_index_in_parent;
  klass->get_attributes = browser_accessibility_get_attributes;
  klass->ref_relation_set = browser_accessibility_ref_relation_set;
}

GType browser_accessibility_get_type() {
  static volatile gsize type_volatile = 0;

#if !GLIB_CHECK_VERSION(2, 36, 0)
  g_type_init();
#endif

  if (g_once_init_enter(&type_volatile)) {
    static const GTypeInfo tinfo = {
        sizeof(BrowserAccessibilityAtkClass),
        (GBaseInitFunc)0,
        (GBaseFinalizeFunc)0,
        (GClassInitFunc)browser_accessibility_class_init,
        (GClassFinalizeFunc)0,
        0,                               /* class data */
        sizeof(BrowserAccessibilityAtk), /* instance size */
        0,                               /* nb preallocs */
        (GInstanceInitFunc)0,
        0 /* value table */
    };

    GType type = g_type_register_static(ATK_TYPE_OBJECT, "BrowserAccessibility",
                                        &tinfo, GTypeFlags(0));
    g_once_init_leave(&type_volatile, type);
  }

  return type_volatile;
}

static const char* GetUniqueAccessibilityTypeName(int interface_mask) {
  // 20 characters is enough for "Chrome%x" with any integer value.
  static char name[20];
  snprintf(name, sizeof(name), "Chrome%x", interface_mask);
  return name;
}

enum AtkInterfaces {
  ATK_ACTION_INTERFACE,
  ATK_COMPONENT_INTERFACE,
  ATK_DOCUMENT_INTERFACE,
  ATK_EDITABLE_TEXT_INTERFACE,
  ATK_HYPERLINK_INTERFACE,
  ATK_HYPERTEXT_INTERFACE,
  ATK_IMAGE_INTERFACE,
  ATK_SELECTION_INTERFACE,
  ATK_TABLE_INTERFACE,
  ATK_TEXT_INTERFACE,
  ATK_VALUE_INTERFACE,
};

static int GetInterfaceMaskFromObject(BrowserAccessibilityAuraLinux* obj) {
  int interface_mask = 0;

  // Component interface is always supported.
  interface_mask |= 1 << ATK_COMPONENT_INTERFACE;

  // Action interface is basic one. It just relays on executing default action
  // for each object.
  interface_mask |= 1 << ATK_ACTION_INTERFACE;

  // Value Interface
  int role = obj->GetRole();
  if (role == ui::AX_ROLE_PROGRESS_INDICATOR ||
      role == ui::AX_ROLE_SCROLL_BAR || role == ui::AX_ROLE_SLIDER) {
    interface_mask |= 1 << ATK_VALUE_INTERFACE;
  }

  // Document Interface
  if (role == ui::AX_ROLE_DOCUMENT || role == ui::AX_ROLE_ROOT_WEB_AREA ||
      role == ui::AX_ROLE_WEB_AREA)
    interface_mask |= 1 << ATK_DOCUMENT_INTERFACE;

  // Image Interface
  if (role == ui::AX_ROLE_IMAGE || role == ui::AX_ROLE_IMAGE_MAP)
    interface_mask |= 1 << ATK_IMAGE_INTERFACE;

  return interface_mask;
}

static GType GetAccessibilityTypeFromObject(
    BrowserAccessibilityAuraLinux* obj) {
  static const GTypeInfo type_info = {
      sizeof(BrowserAccessibilityAtkClass),
      (GBaseInitFunc)0,
      (GBaseFinalizeFunc)0,
      (GClassInitFunc)0,
      (GClassFinalizeFunc)0,
      0,                               /* class data */
      sizeof(BrowserAccessibilityAtk), /* instance size */
      0,                               /* nb preallocs */
      (GInstanceInitFunc)0,
      0 /* value table */
  };

  int interface_mask = GetInterfaceMaskFromObject(obj);
  const char* atk_type_name = GetUniqueAccessibilityTypeName(interface_mask);
  GType type = g_type_from_name(atk_type_name);
  if (type)
    return type;

  type = g_type_register_static(BROWSER_ACCESSIBILITY_TYPE, atk_type_name,
                                &type_info, GTypeFlags(0));
  if (interface_mask & (1 << ATK_ACTION_INTERFACE))
    g_type_add_interface_static(type, ATK_TYPE_ACTION, &ActionInfo);
  if (interface_mask & (1 << ATK_COMPONENT_INTERFACE))
    g_type_add_interface_static(type, ATK_TYPE_COMPONENT, &ComponentInfo);
  if (interface_mask & (1 << ATK_DOCUMENT_INTERFACE))
    g_type_add_interface_static(type, ATK_TYPE_DOCUMENT, &DocumentInfo);
  if (interface_mask & (1 << ATK_IMAGE_INTERFACE))
    g_type_add_interface_static(type, ATK_TYPE_IMAGE, &ImageInfo);
  if (interface_mask & (1 << ATK_VALUE_INTERFACE))
    g_type_add_interface_static(type, ATK_TYPE_VALUE, &ValueInfo);

  return type;
}

BrowserAccessibilityAtk* browser_accessibility_new(
    BrowserAccessibilityAuraLinux* obj) {
  #if !GLIB_CHECK_VERSION(2, 36, 0)
  static bool first_time = true;
  if (first_time) {
    g_type_init();
    first_time = false;
  }
  #endif

  GType type = GetAccessibilityTypeFromObject(obj);
  AtkObject* atk_object = static_cast<AtkObject*>(g_object_new(type, 0));

  atk_object_initialize(atk_object, obj);

  return BROWSER_ACCESSIBILITY(atk_object);
}

void browser_accessibility_detach(BrowserAccessibilityAtk* atk_object) {
  atk_object->m_object = NULL;
}

// static
BrowserAccessibility* BrowserAccessibility::Create() {
  return new BrowserAccessibilityAuraLinux();
}

BrowserAccessibilityAuraLinux::BrowserAccessibilityAuraLinux()
    : atk_object_(NULL) {
}

BrowserAccessibilityAuraLinux::~BrowserAccessibilityAuraLinux() {
  browser_accessibility_detach(BROWSER_ACCESSIBILITY(atk_object_));
  if (atk_object_)
    g_object_unref(atk_object_);
}

AtkObject* BrowserAccessibilityAuraLinux::GetAtkObject() const {
  if (!G_IS_OBJECT(atk_object_))
    return NULL;
  return atk_object_;
}

void BrowserAccessibilityAuraLinux::OnDataChanged() {
  BrowserAccessibility::OnDataChanged();
  InitRoleAndState();

  if (atk_object_) {
    // If the object's role changes and that causes its
    // interface mask to change, we need to create a new
    // AtkObject for it.
    int interface_mask = GetInterfaceMaskFromObject(this);
    if (interface_mask != interface_mask_) {
      g_object_unref(atk_object_);
      atk_object_ = NULL;
    }
  }

  if (!atk_object_) {
    interface_mask_ = GetInterfaceMaskFromObject(this);
    atk_object_ = ATK_OBJECT(browser_accessibility_new(this));
    if (this->GetParent()) {
      atk_object_set_parent(
          atk_object_,
          ToBrowserAccessibilityAuraLinux(this->GetParent())->GetAtkObject());
    }
  }
}

bool BrowserAccessibilityAuraLinux::IsNative() const {
  return true;
}

void BrowserAccessibilityAuraLinux::InitRoleAndState() {
  switch (GetRole()) {
    case ui::AX_ROLE_DOCUMENT:
    case ui::AX_ROLE_ROOT_WEB_AREA:
    case ui::AX_ROLE_WEB_AREA:
      atk_role_ = ATK_ROLE_DOCUMENT_WEB;
      break;
    case ui::AX_ROLE_ALERT:
    case ui::AX_ROLE_ALERT_DIALOG:
      atk_role_ = ATK_ROLE_ALERT;
      break;
    case ui::AX_ROLE_APPLICATION:
      atk_role_ = ATK_ROLE_APPLICATION;
      break;
    case ui::AX_ROLE_BUTTON:
      atk_role_ = ATK_ROLE_PUSH_BUTTON;
      break;
    case ui::AX_ROLE_CANVAS:
      atk_role_ = ATK_ROLE_CANVAS;
      break;
    case ui::AX_ROLE_CAPTION:
      atk_role_ = ATK_ROLE_CAPTION;
      break;
    case ui::AX_ROLE_CHECK_BOX:
      atk_role_ = ATK_ROLE_CHECK_BOX;
      break;
    case ui::AX_ROLE_COLOR_WELL:
      atk_role_ = ATK_ROLE_COLOR_CHOOSER;
      break;
    case ui::AX_ROLE_COLUMN_HEADER:
      atk_role_ = ATK_ROLE_COLUMN_HEADER;
      break;
    case ui::AX_ROLE_COMBO_BOX:
      atk_role_ = ATK_ROLE_COMBO_BOX;
      break;
    case ui::AX_ROLE_DATE:
    case ui::AX_ROLE_DATE_TIME:
      atk_role_ = ATK_ROLE_DATE_EDITOR;
      break;
    case ui::AX_ROLE_DIALOG:
      atk_role_ = ATK_ROLE_DIALOG;
      break;
    case ui::AX_ROLE_DIV:
    case ui::AX_ROLE_GROUP:
      atk_role_ = ATK_ROLE_SECTION;
      break;
    case ui::AX_ROLE_FORM:
      atk_role_ = ATK_ROLE_FORM;
      break;
    case ui::AX_ROLE_IMAGE:
      atk_role_ = ATK_ROLE_IMAGE;
      break;
    case ui::AX_ROLE_IMAGE_MAP:
      atk_role_ = ATK_ROLE_IMAGE_MAP;
      break;
    case ui::AX_ROLE_LABEL_TEXT:
      atk_role_ = ATK_ROLE_LABEL;
      break;
    case ui::AX_ROLE_LINK:
      atk_role_ = ATK_ROLE_LINK;
      break;
    case ui::AX_ROLE_LIST:
      atk_role_ = ATK_ROLE_LIST;
      break;
    case ui::AX_ROLE_LIST_BOX:
      atk_role_ = ATK_ROLE_LIST_BOX;
      break;
    case ui::AX_ROLE_LIST_ITEM:
      atk_role_ = ATK_ROLE_LIST_ITEM;
      break;
    case ui::AX_ROLE_MENU:
      atk_role_ = ATK_ROLE_MENU;
      break;
    case ui::AX_ROLE_MENU_BAR:
      atk_role_ = ATK_ROLE_MENU_BAR;
      break;
    case ui::AX_ROLE_MENU_ITEM:
      atk_role_ = ATK_ROLE_MENU_ITEM;
      break;
    case ui::AX_ROLE_MENU_ITEM_CHECK_BOX:
      atk_role_ = ATK_ROLE_CHECK_MENU_ITEM;
      break;
    case ui::AX_ROLE_MENU_ITEM_RADIO:
      atk_role_ = ATK_ROLE_RADIO_MENU_ITEM;
      break;
    case ui::AX_ROLE_METER:
      atk_role_ = ATK_ROLE_PROGRESS_BAR;
      break;
    case ui::AX_ROLE_PARAGRAPH:
      atk_role_ = ATK_ROLE_PARAGRAPH;
      break;
    case ui::AX_ROLE_RADIO_BUTTON:
      atk_role_ = ATK_ROLE_RADIO_BUTTON;
      break;
    case ui::AX_ROLE_ROW_HEADER:
      atk_role_ = ATK_ROLE_ROW_HEADER;
      break;
    case ui::AX_ROLE_SCROLL_BAR:
      atk_role_ = ATK_ROLE_SCROLL_BAR;
      break;
    case ui::AX_ROLE_SLIDER:
      atk_role_ = ATK_ROLE_SLIDER;
      break;
    case ui::AX_ROLE_SPIN_BUTTON:
      atk_role_ = ATK_ROLE_SPIN_BUTTON;
      break;
    case ui::AX_ROLE_SPLITTER:
      atk_role_ = ATK_ROLE_SEPARATOR;
      break;
    case ui::AX_ROLE_STATIC_TEXT:
      atk_role_ = ATK_ROLE_TEXT;
      break;
    case ui::AX_ROLE_STATUS:
      atk_role_ = ATK_ROLE_STATUSBAR;
      break;
    case ui::AX_ROLE_TAB:
      atk_role_ = ATK_ROLE_PAGE_TAB;
      break;
    case ui::AX_ROLE_TABLE:
      atk_role_ = ATK_ROLE_TABLE;
      break;
    case ui::AX_ROLE_TAB_LIST:
      atk_role_ = ATK_ROLE_PAGE_TAB_LIST;
      break;
    case ui::AX_ROLE_TOGGLE_BUTTON:
      atk_role_ = ATK_ROLE_TOGGLE_BUTTON;
      break;
    case ui::AX_ROLE_TOOLBAR:
      atk_role_ = ATK_ROLE_TOOL_BAR;
      break;
    case ui::AX_ROLE_TOOLTIP:
      atk_role_ = ATK_ROLE_TOOL_TIP;
      break;
    case ui::AX_ROLE_TEXT_FIELD:
      atk_role_ = ATK_ROLE_ENTRY;
      break;
    case ui::AX_ROLE_TREE:
      atk_role_ = ATK_ROLE_TREE;
      break;
    case ui::AX_ROLE_TREE_ITEM:
      atk_role_ = ATK_ROLE_TREE_ITEM;
      break;
    default:
      atk_role_ = ATK_ROLE_UNKNOWN;
      break;
  }
}

}  // namespace content

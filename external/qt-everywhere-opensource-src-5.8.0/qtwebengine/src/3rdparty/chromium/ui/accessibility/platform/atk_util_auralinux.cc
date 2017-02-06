// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atk/atk.h>
#if defined(USE_GCONF)
#include <gconf/gconf-client.h>
#endif
#include <glib-2.0/gmodule.h>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "ui/accessibility/platform/atk_util_auralinux.h"
#include "ui/accessibility/platform/ax_platform_node_auralinux.h"

namespace {

typedef void (*gnome_accessibility_module_init)();

const char kAtkBridgePath[] = "gtk-2.0/modules/libatk-bridge.so";
const char kAtkBridgeSymbolName[] = "gnome_accessibility_module_init";

gnome_accessibility_module_init g_accessibility_module_init = nullptr;

bool AccessibilityModuleInitOnFileThread() {
  // Try to load libatk-bridge.so.
  base::FilePath atk_bridge_path(ATK_LIB_DIR);
  atk_bridge_path = atk_bridge_path.Append(kAtkBridgePath);
  GModule* bridge = g_module_open(atk_bridge_path.value().c_str(),
                                  static_cast<GModuleFlags>(0));
  if (!bridge) {
    VLOG(1) << "Unable to open module " << atk_bridge_path.value();
    return false;
  }

  if (!g_module_symbol(bridge, kAtkBridgeSymbolName,
                      (gpointer *)&g_accessibility_module_init)) {
    VLOG(1) << "Unable to get symbol pointer from " << atk_bridge_path.value();
    // Just to make sure it's null;
    g_accessibility_module_init = nullptr;
    return false;
  }

  return true;
}

#if defined(USE_GCONF)

const char kAccessibilityEnabled[] = "ACCESSIBILITY_ENABLED";
const char kGnomeAccessibilityEnabledKey[] =
    "/desktop/gnome/interface/accessibility";

bool PlatformShouldEnableAccessibility() {
  GConfClient* client = gconf_client_get_default();
  if (!client) {
    LOG(ERROR) << "gconf_client_get_default failed";
    return false;
  }

  GError* error = nullptr;
  gboolean value = gconf_client_get_bool(client,
                                         kGnomeAccessibilityEnabledKey,
                                         &error);
  g_object_unref(client);

  if (error) {
    VLOG(1) << "gconf_client_get_bool failed";
    g_error_free(error);
    return false;
  }

  return value;
}

#else  // !defined(USE_GCONF)

bool PlatformShouldEnableAccessibility() {
  // TODO(iceman): implement this for non-GNOME desktops.
  return false;
}

#endif  // defined(USE_GCONF)

bool ShouldEnableAccessibility() {
#if defined(USE_GCONF)
  char* enable_accessibility = getenv(kAccessibilityEnabled);
  if ((enable_accessibility && atoi(enable_accessibility) == 1) ||
      PlatformShouldEnableAccessibility())
    return true;
#endif  // defined(USE_GCONF)

  return false;
}

}  // namespace

G_BEGIN_DECLS

//
// atk_util_auralinux AtkObject definition and implementation.
//

#define ATK_UTIL_AURALINUX_TYPE (atk_util_auralinux_get_type())
#define ATK_UTIL_AURALINUX(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), \
     ATK_UTIL_AURALINUX_TYPE, \
     AtkUtilAuraLinux))
#define ATK_UTIL_AURALINUX_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), \
                             ATK_UTIL_AURALINUX_TYPE, \
                             AtkUtilAuraLinuxClass))
#define IS_ATK_UTIL_AURALINUX(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), ATK_UTIL_AURALINUX_TYPE))
#define IS_ATK_UTIL_AURALINUX_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), ATK_UTIL_AURALINUX_TYPE))
#define ATK_UTIL_AURALINUX_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), \
                               ATK_UTIL_AURALINUX_TYPE, \
                               AtkUtilAuraLinuxClass))

typedef struct _AtkUtilAuraLinux        AtkUtilAuraLinux;
typedef struct _AtkUtilAuraLinuxClass   AtkUtilAuraLinuxClass;

struct _AtkUtilAuraLinux
{
  AtkUtil parent;
};

struct _AtkUtilAuraLinuxClass
{
  AtkUtilClass parent_class;
};

GType atk_util_auralinux_get_type();

G_DEFINE_TYPE(AtkUtilAuraLinux, atk_util_auralinux, ATK_TYPE_UTIL);

static void atk_util_auralinux_init(AtkUtilAuraLinux *ax_util) {
}

static AtkObject* atk_util_auralinux_get_root() {
  ui::AXPlatformNode* application = ui::AXPlatformNodeAuraLinux::application();
  if (application) {
    return application->GetNativeViewAccessible();
  }
  return nullptr;
}

static G_CONST_RETURN gchar* atk_util_auralinux_get_toolkit_name(void) {
  return "Chromium";
}

static G_CONST_RETURN gchar* atk_util_auralinux_get_toolkit_version(void) {
  return "1.0";
}

static void atk_util_auralinux_class_init(AtkUtilAuraLinuxClass *klass) {
  AtkUtilClass *atk_class;
  gpointer data;

  data = g_type_class_peek(ATK_TYPE_UTIL);
  atk_class = ATK_UTIL_CLASS(data);

  atk_class->get_root = atk_util_auralinux_get_root;
  atk_class->get_toolkit_name = atk_util_auralinux_get_toolkit_name;
  atk_class->get_toolkit_version = atk_util_auralinux_get_toolkit_version;
}

G_END_DECLS

//
// AtkUtilAuraLinuxClass implementation.
//

namespace ui {

// static
AtkUtilAuraLinux* AtkUtilAuraLinux::GetInstance() {
  return base::Singleton<AtkUtilAuraLinux>::get();
}

#if defined(USE_GCONF)

AtkUtilAuraLinux::AtkUtilAuraLinux()
    : is_enabled_(false) {
}

#else

AtkUtilAuraLinux::AtkUtilAuraLinux() {
}

#endif // defined(USE_GCONF)

void AtkUtilAuraLinux::Initialize(
    scoped_refptr<base::TaskRunner> init_task_runner) {

  // Register our util class.
  g_type_class_unref(g_type_class_ref(ATK_UTIL_AURALINUX_TYPE));

  if (!ShouldEnableAccessibility())
    return;

  init_task_runner->PostTaskAndReply(
      FROM_HERE,
      base::Bind(
          &AtkUtilAuraLinux::CheckIfAccessibilityIsEnabledOnFileThread,
          base::Unretained(this)),
      base::Bind(
          &AtkUtilAuraLinux::FinishAccessibilityInitOnUIThread,
          base::Unretained(this)));
}

AtkUtilAuraLinux::~AtkUtilAuraLinux() {
}

#if defined(USE_GCONF)

void AtkUtilAuraLinux::CheckIfAccessibilityIsEnabledOnFileThread() {
  is_enabled_ = AccessibilityModuleInitOnFileThread();
}

void AtkUtilAuraLinux::FinishAccessibilityInitOnUIThread() {
  if (!is_enabled_) {
    VLOG(1) << "Will not enable ATK accessibility support.";
    return;
  }

  DCHECK(g_accessibility_module_init);
  g_accessibility_module_init();
}

#else

void AtkUtilAuraLinux::CheckIfAccessibilityIsEnabledOnFileThread() {
}

void AtkUtilAuraLinux::FinishAccessibilityInitOnUIThread() {
}

#endif // defined(USE_GCONF)

}  // namespace ui

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/app/blimp_engine_crash_keys.h"

#include "base/debug/crash_logging.h"
#include "base/macros.h"
#include "components/crash/core/common/crash_keys.h"

namespace blimp {
namespace engine {

size_t RegisterEngineCrashKeys() {
  // For now these need to be kept relatively up to date with those in
  // //chrome/common/crash_keys.cc::RegisterChromeCrashKeys()
  // All of the keys used in //content and //components/crash must show up
  // here.
  // TODO(marcinjb): Change the approach when http://crbug.com/598854 is
  // resolved.
  const base::debug::CrashKey engine_keys[] = {
      { crash_keys::kClientId, crash_keys::kSmallSize },
      { crash_keys::kChannel, crash_keys::kSmallSize },
      { crash_keys::kNumVariations, crash_keys::kSmallSize },
      { crash_keys::kVariations, crash_keys::kLargeSize },

      // browser/:
      { "third-party-modules-loaded", crash_keys::kSmallSize },
      { "third-party-modules-not-loaded", crash_keys::kSmallSize },

      // //content crash keys
      { "bad_message_reason", crash_keys::kSmallSize },
      { "channel_error_bt", crash_keys::kMediumSize },
      { "discardable-memory-allocated", crash_keys::kSmallSize },
      { "discardable-memory-free", crash_keys::kSmallSize },
      { "ppapi_path", crash_keys::kMediumSize },
      { "remove_route_bt", crash_keys::kMediumSize },
      { "rwhvm_window", crash_keys::kMediumSize },
      { "subresource_url", crash_keys::kLargeSize },
      { "total-discardable-memory-allocated", crash_keys::kSmallSize },

      // gin/:
      { "v8-ignition", crash_keys::kSmallSize },

      // Temporary for http://crbug.com/575245.
      { "commit_frame_id", crash_keys::kSmallSize },
      { "commit_proxy_id", crash_keys::kSmallSize },
      { "commit_view_id", crash_keys::kSmallSize },
      { "commit_main_render_frame_id", crash_keys::kSmallSize },
      { "initrf_frame_id", crash_keys::kSmallSize },
      { "initrf_proxy_id", crash_keys::kSmallSize },
      { "initrf_view_id", crash_keys::kSmallSize },
      { "initrf_main_frame_id", crash_keys::kSmallSize },
      { "initrf_view_is_live", crash_keys::kSmallSize },
      { "newproxy_proxy_id", crash_keys::kSmallSize },
      { "newproxy_view_id", crash_keys::kSmallSize },
      { "newproxy_opener_id", crash_keys::kSmallSize },
      { "newproxy_parent_id", crash_keys::kSmallSize },
      { "rvinit_view_id", crash_keys::kSmallSize },
      { "rvinit_proxy_id", crash_keys::kSmallSize },
      { "rvinit_main_frame_id", crash_keys::kSmallSize },
      { "swapout_frame_id", crash_keys::kSmallSize },
      { "swapout_proxy_id", crash_keys::kSmallSize },
      { "swapout_view_id", crash_keys::kSmallSize },

      // Temporary for https://crbug.com/591478.
      { "initrf_parent_proxy_exists", crash_keys::kSmallSize },
      { "initrf_render_view_is_live", crash_keys::kSmallSize },
      { "initrf_parent_is_in_same_site_instance", crash_keys::kSmallSize},
      { "initrf_parent_process_is_live", crash_keys::kSmallSize},
      { "initrf_root_is_in_same_site_instance", crash_keys::kSmallSize},
      { "initrf_root_is_in_same_site_instance_as_parent",
        crash_keys::kSmallSize},
      { "initrf_root_process_is_live", crash_keys::kSmallSize},
      { "initrf_root_proxy_is_live", crash_keys::kSmallSize},

      // Temporary for https://crbug.com/626802.
      { "newframe_routing_id", crash_keys::kSmallSize },
      { "newframe_proxy_id", crash_keys::kSmallSize },
      { "newframe_opener_id", crash_keys::kSmallSize },
      { "newframe_parent_id", crash_keys::kSmallSize },
      { "newframe_widget_id", crash_keys::kSmallSize },
      { "newframe_widget_hidden", crash_keys::kSmallSize },
      { "newframe_replicated_origin", crash_keys::kSmallSize },
      { "newframe_oopifs_possible", crash_keys::kSmallSize },

      // Temporary for https://crbug.com/630103.
      { "origin_mismatch_url", crash_keys::kLargeSize },
      { "origin_mismatch_origin", crash_keys::kMediumSize },
      { "origin_mismatch_transition", crash_keys::kSmallSize },
      { "origin_mismatch_redirects", crash_keys::kSmallSize },
      { "origin_mismatch_same_page", crash_keys::kSmallSize },

      // Temporary for https://crbug.com/612711.
      { "aci_wrong_sp_extension_id", crash_keys::kSmallSize },

      // Temporary for https://crbug.com/616149.
      { "existing_extension_pref_value_type", crash_keys::kSmallSize },

      // Temporary for https://crbug.com/630495.
      { "swdh_register_cannot_host_url", crash_keys::kLargeSize },
      { "swdh_register_cannot_scope_url", crash_keys::kLargeSize },
      { "swdh_register_cannot_script_url", crash_keys::kLargeSize },

      // Temporary for https://crbug.com/619294.
      { "swdh_unregister_cannot_host_url", crash_keys::kLargeSize },
      { "swdh_unregister_cannot_scope_url", crash_keys::kLargeSize },

      // Temporary for https://crbug.com/630496.
      { "swdh_get_registration_cannot_host_url", crash_keys::kLargeSize },
      { "swdh_get_registration_cannot_document_url", crash_keys::kLargeSize },
    };

  return base::debug::InitCrashKeys(engine_keys, arraysize(engine_keys),
                                    crash_keys::kChunkMaxLength);
}

}  // namespace engine
}  // namespace blimp

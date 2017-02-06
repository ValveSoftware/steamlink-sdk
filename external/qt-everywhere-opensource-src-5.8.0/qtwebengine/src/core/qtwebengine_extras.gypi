{
  'variables': {
    'werror%': '',
    'qt_os%': '',
  },
  'target_defaults': {
    # patterns used to exclude chromium files from the build when we have a drop-in replacement
    'sources/': [
      ['exclude', 'clipboard/clipboard_aura.cc$'],
      ['exclude', 'clipboard/clipboard_aurax11.cc$'],
      ['exclude', 'clipboard/clipboard_gtk.cc$'],
      ['exclude', 'clipboard/clipboard_mac.mm$'],
      ['exclude', 'clipboard/clipboard_win.cc$'],
      ['exclude', 'clipboard/clipboard_util_win\\.(cc|h)$'],
      ['exclude', 'dragdrop/os_exchange_data_provider_aurax11\\.(cc|h)$'],
      ['exclude', 'dragdrop/os_exchange_data_provider_win\\.(cc|h)$'],
      ['exclude', 'dragdrop/os_exchange_data_provider_mac\\.(mm|h)$'],
      ['exclude', 'resource/resource_bundle_auralinux.cc$'],
      ['exclude', 'resource/resource_bundle_gtk.cc$'],
      ['exclude', 'resource/resource_bundle_mac.mm$'],
      ['exclude', 'resource/resource_bundle_win.cc$'],
      ['exclude', 'browser/web_contents/web_contents_view_aura\\.(cc|h)$'],
      ['exclude', 'browser/web_contents/web_contents_view_gtk\\.(cc|h)$'],
      ['exclude', 'browser/web_contents/web_contents_view_mac\\.(mm|h)$'],
      ['exclude', 'browser/web_contents/web_contents_view_win\\.(cc|h)$'],
      ['exclude', 'browser/renderer_host/gtk_im_context_wrapper\\.cc$'],
      ['exclude', 'browser/renderer_host/native_web_keyboard_event_aura.cc$'],
      ['exclude', 'browser/renderer_host/native_web_keyboard_event_mac.mm$'],
      ['exclude', 'browser/renderer_host/pepper/pepper_truetype_font_list_pango\\.cc$'],
      ['exclude', 'browser/renderer_host/render_widget_host_view_aura\\.(cc|h)$'],
      ['exclude', 'browser/renderer_host/render_widget_host_view_gtk\\.(cc|h)$'],
      ['exclude', 'browser/renderer_host/render_widget_host_view_mac\\.(mm|h)$'],
      ['exclude', 'browser/renderer_host/render_widget_host_view_win\\.(cc|h)$'],
      ['exclude', 'common/font_list_pango\\.cc$'],
      ['exclude', 'browser/accessibility/browser_accessibility_cocoa\\.(mm|h)$'],
      ['exclude', 'browser/accessibility/browser_accessibility_gtk\\.(cc|h)$'],
      ['exclude', 'browser/accessibility/browser_accessibility_mac\\.(mm|h)$'],
      ['exclude', 'browser/accessibility/browser_accessibility_win\\.(cc|h)$'],
      ['exclude', 'browser/accessibility/browser_accessibility_event_win\\.(cc|h)$'],
      ['exclude', 'browser/accessibility/browser_accessibility_manager_gtk\\.(cc|h)$'],
      ['exclude', 'browser/accessibility/browser_accessibility_manager_mac\\.(mm|h)$'],
      ['exclude', 'browser/accessibility/browser_accessibility_manager_win\\.(cc|h)$'],
      ['exclude', 'command_buffer/service/async_pixel_transfer_manager_egl\\.(cc|h)$'],
      ['exclude', 'common/gpu/image_transport_surface_linux\\.cc$'],
      ['exclude', 'common/gpu/image_transport_surface_win\\.cc$'],
      ['exclude', 'gl_surface_egl\\.cc$'],
      ['exclude', 'gl_surface_egl_ozone\\.cc$'],
      ['exclude', 'gl_surface_egl_win\\.cc$'],
      ['exclude', 'gl_surface_egl_x11\\.cc$'],
      ['exclude', 'gl_surface_glx\\.cc$'],
      ['exclude', 'gl_surface_ozone\\.cc$'],
      ['exclude', 'gl_factory_ozone\\.cc$'],
      ['exclude', 'gl_factory_win\\.cc$'],
      ['exclude', 'gl_initializer_ozone\\.cc$'],
      ['exclude', 'gl_initializer_win\\.cc$'],
      ['exclude', 'gl_initializer_x11\\.cc$'],
      # Avoid the ATL dependency to allow building with VS Express
      ['exclude', 'browser/accessibility/accessibility_tree_formatter\\.(cc|h)$',],
      ['exclude', 'browser/accessibility/accessibility_tree_formatter_mac\\.(mm|h)$',],
      ['exclude', 'browser/accessibility/accessibility_tree_formatter_utils_win\\.(cc|h)$',],
      ['exclude', 'browser/accessibility/accessibility_tree_formatter_win\\.(cc|h)$',],
      ['exclude', 'browser/accessibility/accessibility_ui\\.(cc|h)$',],
      ['exclude', 'browser/renderer_host/legacy_render_widget_host_win\\.(cc|h)$'],
      ['exclude', 'win/accessibility_ids_win\\.h$'],
      ['exclude', 'win/accessibility_misc_utils\\.(cc|h)$'],
      ['exclude', 'win/atl_module\\.h$'],
      ['exclude', 'platform/ax_platform_node_win\\.(cc|h)$'],
      ['exclude', 'audio_classifier\\.(cc|h)$'],
    ],
    'defines': [
      'TOOLKIT_QT',
    ],
    'configurations': {
      'Debug': {
        'defines': [
          'QTWEBENGINEPROCESS_NAME="<(qtwe_process_name_debug)"'
        ],
      },
      'Debug_x64': {
        'defines': [
          'QTWEBENGINEPROCESS_NAME="<(qtwe_process_name_debug)"'
        ],
      },
      'Release': {
        'defines': [
          'QTWEBENGINEPROCESS_NAME="<(qtwe_process_name_release)"'
        ],
      },
      'Release_x64': {
        'defines': [
          'QTWEBENGINEPROCESS_NAME="<(qtwe_process_name_release)"'
        ],
      },
    },
  },
  'conditions': [
    [ 'qt_os=="embedded_linux"', {
      'variables': {
        'external_ozone_platforms': [ 'eglfs', ],
      },
      'target_defaults': {
        'defines': [
            'GL_GLEXT_PROTOTYPES',
            'EGL_EGLEXT_PROTOTYPES',
            # At runtime the env variable SSL_CERT_DIR can be used to override this
            'OPENSSLDIR="/usr/lib/ssl"',
            'OPENSSL_LOAD_CONF',
            'EGL_API_FB=1',
            'LINUX=1',
        ],
        'target_conditions': [
          ['_type=="shared_library"', {
            'ldflags': [
              # Tell the linker to prefer symbols within the library before looking outside
              '-Wl,-shared,-Bsymbolic',
            ],
          }],
          ['_toolset=="target"', {
            'libraries': [
              '-licui18n',
              '-licuuc',
              '-licudata',
            ],
          }],
        ],
      },
    }],
  ],
}

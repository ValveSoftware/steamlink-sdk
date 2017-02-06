{
  'targets': [
    {
      # GN version: //components/supervused_user_error_page
      'target_name': 'supervised_user_error_page',
      'type': 'static_library',
      'dependencies': [
        'components_resources.gyp:components_resources',
        'components_strings.gyp:components_strings',
        '../base/base.gyp:base',
        '../ui/base/ui_base.gyp:ui_base'
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'supervised_user_error_page/supervised_user_error_page.cc',
        'supervised_user_error_page/supervised_user_error_page.h',
      ],
    },
  ],
}

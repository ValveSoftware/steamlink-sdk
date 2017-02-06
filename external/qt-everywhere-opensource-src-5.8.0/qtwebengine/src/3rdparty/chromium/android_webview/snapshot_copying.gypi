{
  'variables': {
    'snapshot_additional_input_paths': [],
    'snapshot_copy_files': [],
    'conditions': [
      ['target_arch=="arm" or target_arch=="ia32" or target_arch=="mipsel"', {
        'snapshot_additional_input_paths': [
          '<(asset_location)/natives_blob_32.bin',
          '<(asset_location)/snapshot_blob_32.bin',
        ],
        'snapshot_copy_files': [
          '<(PRODUCT_DIR)/natives_blob_32.bin',
          '<(PRODUCT_DIR)/snapshot_blob_32.bin',
        ],
      }],
      ['target_arch=="arm64" or target_arch=="x64" or target_arch=="mips64el"', {
        'snapshot_additional_input_paths': [
          '<(asset_location)/natives_blob_64.bin',
          '<(asset_location)/snapshot_blob_64.bin',
        ],
        'snapshot_copy_files': [
          '<(PRODUCT_DIR)/natives_blob_64.bin',
          '<(PRODUCT_DIR)/snapshot_blob_64.bin',
        ],
      }],
    ],
  },
}

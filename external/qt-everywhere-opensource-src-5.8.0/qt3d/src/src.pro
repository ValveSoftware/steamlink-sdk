TEMPLATE = subdirs

src_core.subdir = $$PWD/core
src_core.target = sub-core

src_render.subdir = $$PWD/render
src_render.target = sub-render
src_render.depends = src_core

src_logic.subdir = $$PWD/logic
src_logic.target = sub-logic
src_logic.depends = src_core

src_input.subdir = $$PWD/input
src_input.target = sub-input
src_input.depends = src_render

src_extras.subdir = $$PWD/extras
src_extras.target = src_extras
src_extras.depends = src_render src_input src_logic

# Quick3D libs
src_quick3d_core.subdir = $$PWD/quick3d/quick3d
src_quick3d_core.target = sub-quick3d-core
src_quick3d_core.depends = src_core src_input

src_quick3d_render.subdir = $$PWD/quick3d/quick3drender
src_quick3d_render.target = sub-quick3d-render
src_quick3d_render.depends = src_render src_quick3d_core

src_quick3d_input.subdir = $$PWD/quick3d/quick3dinput
src_quick3d_input.target = sub-quick3d-input
src_quick3d_input.depends = src_input src_quick3d_core

src_quick3d_extras.subdir = $$PWD/quick3d/quick3dextras
src_quick3d_extras.target = sub-quick3d-extras
src_quick3d_extras.depends = src_render src_logic src_input src_quick3d_core

# Quick3D imports
src_quick3d_core_imports.file = $$PWD/quick3d/imports/core/importscore.pro
src_quick3d_core_imports.target = sub-quick3d-imports-core
src_quick3d_core_imports.depends = src_quick3d_core

src_quick3d_imports_render.file = $$PWD/quick3d/imports/render/importsrender.pro
src_quick3d_imports_render.target = sub-quick3d-imports-render
src_quick3d_imports_render.depends = src_quick3d_render

src_quick3d_imports_scene3d.file = $$PWD/quick3d/imports/scene3d/importsscene3d.pro
src_quick3d_imports_scene3d.target = sub-quick3d-imports-scene3d
src_quick3d_imports_scene3d.depends = src_quick3d_render src_input

src_quick3d_imports_input.file = $$PWD/quick3d/imports/input/importsinput.pro
src_quick3d_imports_input.target = sub-quick3d-imports-input
src_quick3d_imports_input.depends = src_input src_quick3d_input

src_quick3d_imports_logic.file = $$PWD/quick3d/imports/logic/importslogic.pro
src_quick3d_imports_logic.target = sub-quick3d-imports-logic
src_quick3d_imports_logic.depends = src_logic

src_quick3d_imports_extras.file = $$PWD/quick3d/imports/extras/importsextras.pro
src_quick3d_imports_extras.target = sub-quick3d-imports-extras
src_quick3d_imports_extras.depends = src_extras src_quick3d_extras

# Qt3D Scene Parser plugins
src_plugins_sceneparsers.file = $$PWD/plugins/sceneparsers/sceneparsers.pro
src_plugins_sceneparsers.target = sub-plugins-sceneparsers
src_plugins_sceneparsers.depends = src_render src_extras

SUBDIRS += \
    src_core \
    src_render \
    src_logic \
    src_input \
    src_extras \
    src_quick3d_core \
    src_quick3d_core_imports \
    src_quick3d_render \
    src_quick3d_input \
    src_quick3d_extras \
    src_quick3d_imports_render \
    src_quick3d_imports_scene3d \
    src_quick3d_imports_input \
    src_quick3d_imports_logic \
    src_quick3d_imports_extras \
    src_plugins_sceneparsers \
    doc



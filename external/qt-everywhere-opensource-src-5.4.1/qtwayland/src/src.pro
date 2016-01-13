TEMPLATE=subdirs

sub_qtwaylandscanner.subdir = qtwaylandscanner
sub_qtwaylandscanner.target = sub-qtwaylandscanner
SUBDIRS += sub_qtwaylandscanner

# Don't build QtCompositor API unless explicitly enabled
contains(CONFIG, wayland-compositor) {
    sub_compositor.subdir = compositor
    sub_compositor.depends = sub-qtwaylandscanner
    sub_compositor.target = sub-compositor
    SUBDIRS += sub_compositor
}

sub_client.subdir = client
sub_client.depends = sub-qtwaylandscanner
sub_client.target = sub-client
SUBDIRS += sub_client

sub_plugins.subdir = plugins
sub_plugins.depends = sub-qtwaylandscanner sub-client
contains(CONFIG, wayland-compositor): sub_plugins.depends += sub-compositor
sub_plugins.target = sub-plugins
SUBDIRS += sub_plugins

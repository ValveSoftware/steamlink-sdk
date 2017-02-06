unstable_protocols =								\
	unstable/pointer-gestures/pointer-gestures-unstable-v1.xml		\
	unstable/fullscreen-shell/fullscreen-shell-unstable-v1.xml		\
	unstable/linux-dmabuf/linux-dmabuf-unstable-v1.xml			\
	unstable/text-input/text-input-unstable-v1.xml				\
	unstable/input-method/input-method-unstable-v1.xml			\
	unstable/xdg-shell/xdg-shell-unstable-v5.xml				\
	unstable/relative-pointer/relative-pointer-unstable-v1.xml		\
	unstable/pointer-constraints/pointer-constraints-unstable-v1.xml	\
	unstable/tablet/tablet-unstable-v1.xml			                \
	$(NULL)

stable_protocols =								\
	stable/presentation-time/presentation-time.xml				\
	stable/viewporter/viewporter.xml					\
	$(NULL)

nobase_dist_pkgdata_DATA =							\
	$(unstable_protocols)							\
	$(stable_protocols)							\
	$(NULL)

dist_noinst_DATA =								\
	$(sort $(foreach p,$(unstable_protocols),$(dir $p)README))		\
	$(sort $(foreach p,$(stable_protocols),$(dir $p)README))		\
	$(NULL)

noarch_pkgconfig_DATA = wayland-protocols.pc

dist_check_SCRIPTS = tests/scan.sh

TESTS = $(unstable_protocols) $(stable_protocols)
TEST_EXTENSIONS = .xml
AM_TESTS_ENVIRONMENT = SCANNER='$(wayland_scanner)'; export SCANNER;
XML_LOG_COMPILER = $(srcdir)/tests/scan.sh

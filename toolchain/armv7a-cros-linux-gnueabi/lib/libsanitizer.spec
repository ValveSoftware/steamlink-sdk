# This spec file is read by gcc when linking.  It is used to specify the
# standard libraries we need in order to link with various sanitizer libs.

*link_libasan: -ldl -lpthread -lm

*link_libtsan: -ldl -lpthread -lm

*link_libubsan: -ldl -lpthread -lm

*link_liblsan: -ldl -lpthread -lm


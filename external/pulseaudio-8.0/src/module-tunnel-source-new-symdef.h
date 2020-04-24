#ifndef foomoduletunnelsourcenewsymdeffoo
#define foomoduletunnelsourcenewsymdeffoo

#include <pulsecore/core.h>
#include <pulsecore/module.h>
#include <pulsecore/macro.h>

#define pa__init module_tunnel_source_new_LTX_pa__init
#define pa__done module_tunnel_source_new_LTX_pa__done
#define pa__get_author module_tunnel_source_new_LTX_pa__get_author
#define pa__get_description module_tunnel_source_new_LTX_pa__get_description
#define pa__get_usage module_tunnel_source_new_LTX_pa__get_usage
#define pa__get_version module_tunnel_source_new_LTX_pa__get_version
#define pa__get_deprecated module_tunnel_source_new_LTX_pa__get_deprecated
#define pa__load_once module_tunnel_source_new_LTX_pa__load_once
#define pa__get_n_used module_tunnel_source_new_LTX_pa__get_n_used

int pa__init(pa_module*m);
void pa__done(pa_module*m);
int pa__get_n_used(pa_module*m);

const char* pa__get_author(void);
const char* pa__get_description(void);
const char* pa__get_usage(void);
const char* pa__get_version(void);
const char* pa__get_deprecated(void);
bool pa__load_once(void);

#endif

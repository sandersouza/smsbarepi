#ifndef SMSBARE_BACKEND_CORE_REGISTRY_H
#define SMSBARE_BACKEND_CORE_REGISTRY_H

#include "module.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const BackendModuleV1 *const *modules;
    unsigned module_count;
    const BackendModuleV1 *default_module;
    const BackendModuleV1 *selected_module;
    const char *requested_backend_id;
} BackendRegistry;

void backend_registry_init(BackendRegistry *registry,
                           const BackendModuleV1 *const *modules,
                           unsigned module_count,
                           const char *default_backend_id);
const BackendModuleV1 *backend_registry_find(const BackendRegistry *registry, const char *backend_id);
const BackendModuleV1 *backend_registry_select(BackendRegistry *registry,
                                               const char *requested_backend_id,
                                               boolean *out_requested_backend_found,
                                               boolean *out_used_default_backend);
const BackendModuleV1 *backend_registry_default(const BackendRegistry *registry);

#ifdef __cplusplus
}
#endif

#endif

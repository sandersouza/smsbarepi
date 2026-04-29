#include "registry.h"

static int backend_streq(const char *a, const char *b)
{
    if (a == 0 || b == 0)
    {
        return 0;
    }

    while (*a != '\0' && *b != '\0')
    {
        if (*a != *b)
        {
            return 0;
        }
        ++a;
        ++b;
    }

    return (*a == '\0' && *b == '\0') ? 1 : 0;
}

const BackendModuleV1 *backend_registry_find(const BackendRegistry *registry, const char *backend_id)
{
    unsigned i;

    if (registry == 0 || registry->modules == 0 || backend_id == 0 || backend_id[0] == '\0')
    {
        return 0;
    }

    for (i = 0u; i < registry->module_count; ++i)
    {
        const BackendModuleV1 *module = registry->modules[i];
        if (module != 0 && backend_streq(module->backend_id, backend_id))
        {
            return module;
        }
    }

    return 0;
}

void backend_registry_init(BackendRegistry *registry,
                           const BackendModuleV1 *const *modules,
                           unsigned module_count,
                           const char *default_backend_id)
{
    if (registry == 0)
    {
        return;
    }

    registry->modules = modules;
    registry->module_count = module_count;
    registry->default_module = 0;
    registry->selected_module = 0;
    registry->requested_backend_id = 0;

    if (modules == 0 || module_count == 0u)
    {
        return;
    }

    registry->default_module = backend_registry_find(registry, default_backend_id);
    if (registry->default_module == 0)
    {
        registry->default_module = modules[0];
    }
}

const BackendModuleV1 *backend_registry_select(BackendRegistry *registry,
                                               const char *requested_backend_id,
                                               boolean *out_requested_backend_found,
                                               boolean *out_used_default_backend)
{
    const BackendModuleV1 *selected = 0;
    boolean requested_found = FALSE;
    boolean used_default = FALSE;

    if (registry == 0)
    {
        return 0;
    }

    registry->requested_backend_id = requested_backend_id;

    if (requested_backend_id != 0 && requested_backend_id[0] != '\0')
    {
        selected = backend_registry_find(registry, requested_backend_id);
        requested_found = (selected != 0) ? TRUE : FALSE;
    }

    if (selected == 0)
    {
        selected = registry->default_module;
        used_default = (selected != 0) ? TRUE : FALSE;
    }

    registry->selected_module = selected;

    if (out_requested_backend_found != 0)
    {
        *out_requested_backend_found = requested_found;
    }
    if (out_used_default_backend != 0)
    {
        *out_used_default_backend = used_default;
    }

    return selected;
}

const BackendModuleV1 *backend_registry_default(const BackendRegistry *registry)
{
    return registry != 0 ? registry->default_module : 0;
}

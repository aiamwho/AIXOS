#ifndef AIXOS_NAMESPACE_H
#define AIXOS_NAMESPACE_H

#include "aixos/types.h"
#include "aixos/microkernel.h"

/*
 * Resource-manager namespace API.
 */

void aixos_namespace_init(void);

int aixos_namespace_register(const char *name, aixos_handle_t obj,
                             aixos_obj_type_t type, uint16_t rights);

int aixos_namespace_unregister(const char *name);

int aixos_namespace_resolve(const char *name, aixos_handle_t *obj,
                            aixos_obj_type_t *type, uint16_t *rights);

int aixos_namespace_open(const char *name, uint16_t required_rights,
                         aixos_cap_t *cap);

#endif /* AIXOS_NAMESPACE_H */

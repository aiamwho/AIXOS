#include "aixos/types.h"
#include "aixos/task.h"
#include "aixos/microkernel.h"
#include "kernel/list.h"
#include "kernel/object.h"
#include "kernel/sched.h"
#include "aixos/arch/arch.h"
#include "config/aixos_cfg.h"
#include "config/string.h"

/*
 * Resource-manager namespace.
 * Provides path-based resource registration and lookup.
 */

#if AIXOS_CFG_ENABLE_NAMESPACE

#define NS_MAX_ENTRIES AIXOS_CFG_NAMESPACE_MAX

typedef struct {
    char name[32];
    aixos_handle_t object;
    aixos_obj_type_t type;
    uint16_t default_rights;
    uint8_t used;
} namespace_entry_t;

static namespace_entry_t ns_table[NS_MAX_ENTRIES];

void aixos_namespace_init(void)
{
    int i;
    for (i = 0; i < NS_MAX_ENTRIES; i++) {
        memset(&ns_table[i], 0, sizeof(ns_table[i]));
        ns_table[i].used = 0U;
    }
}

int aixos_namespace_register(const char *name, aixos_handle_t obj,
                             aixos_obj_type_t type, uint16_t rights)
{
    int i;
    size_t len;
    aixos_arch_flags_t flags;
    
    if (name == NULL || obj == AIXOS_HANDLE_INVALID || type == AIXOS_OBJ_UNUSED) {
        return AIXOS_ERR_INVAL;
    }
    
    len = strlen(name);
    if (len == 0U || len >= sizeof(ns_table[0].name)) {
        return AIXOS_ERR_INVAL;
    }
    
    flags = aixos_arch_int_disable();
    
    // 查找空闲槽位
    for (i = 0; i < NS_MAX_ENTRIES; i++) {
        if (ns_table[i].used == 0U) {
            break;
        }
        // 检查是否已注册同名条目
        if (ns_table[i].used != 0U && strcmp(ns_table[i].name, name) == 0) {
            aixos_arch_int_restore(flags);
            return AIXOS_ERR_BUSY;
        }
    }
    if (i >= NS_MAX_ENTRIES) {
        aixos_arch_int_restore(flags);
        return AIXOS_ERR_NOMEM;
    }
    
    strncpy(ns_table[i].name, name, sizeof(ns_table[i].name) - 1U);
    ns_table[i].name[sizeof(ns_table[i].name) - 1U] = '\0';
    ns_table[i].object = obj;
    ns_table[i].type = type;
    ns_table[i].default_rights = rights;
    ns_table[i].used = 1U;
    
    aixos_arch_int_restore(flags);
    return AIXOS_OK;
}

int aixos_namespace_unregister(const char *name)
{
    int i;
    aixos_arch_flags_t flags;
    
    if (name == NULL) return AIXOS_ERR_INVAL;
    
    flags = aixos_arch_int_disable();
    for (i = 0; i < NS_MAX_ENTRIES; i++) {
        if (ns_table[i].used != 0U && strcmp(ns_table[i].name, name) == 0) {
            memset(&ns_table[i], 0, sizeof(ns_table[i]));
            ns_table[i].used = 0U;
            aixos_arch_int_restore(flags);
            return AIXOS_OK;
        }
    }
    aixos_arch_int_restore(flags);
    return AIXOS_ERR_NOT_FOUND;
}

int aixos_namespace_resolve(const char *name, aixos_handle_t *obj,
                            aixos_obj_type_t *type, uint16_t *rights)
{
    int i;
    aixos_arch_flags_t flags;
    
    if (name == NULL || obj == NULL) return AIXOS_ERR_INVAL;
    
    flags = aixos_arch_int_disable();
    for (i = 0; i < NS_MAX_ENTRIES; i++) {
        if (ns_table[i].used != 0U && strcmp(ns_table[i].name, name) == 0) {
            *obj = ns_table[i].object;
            if (type != NULL) *type = ns_table[i].type;
            if (rights != NULL) *rights = ns_table[i].default_rights;
            aixos_arch_int_restore(flags);
            return AIXOS_OK;
        }
    }
    aixos_arch_int_restore(flags);
    return AIXOS_ERR_NOT_FOUND;
}

int aixos_namespace_open(const char *name, uint16_t required_rights,
                         aixos_cap_t *cap)
{
    aixos_tcb_t *caller = g_cur_task;
    aixos_handle_t obj;
    aixos_obj_type_t type;
    uint16_t rights;
    int result;
    
    if (caller == NULL || cap == NULL) return AIXOS_ERR_INVAL;
    
    result = aixos_namespace_resolve(name, &obj, &type, &rights);
    if (result != AIXOS_OK) return result;
    
    // 检查权限
    if ((rights & required_rights) != required_rights) {
        return AIXOS_ERR_PERM;
    }
    
    // 为调用者授予能力
    *cap = (aixos_cap_t)cap_grant_direct(caller, obj, type, rights);
    return *cap >= 0 ? AIXOS_OK : AIXOS_ERR_NOMEM;
}

#else /* AIXOS_CFG_ENABLE_NAMESPACE */

void aixos_namespace_init(void) {}

#endif /* AIXOS_CFG_ENABLE_NAMESPACE */

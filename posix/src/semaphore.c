#include "posix/include/semaphore.h"
#include "aixos/arch/arch.h"
#include "config/aixos_cfg.h"
#include "config/string.h"

#define POSIX_SEM_NAME_MAX 31U

typedef struct {
    char name[POSIX_SEM_NAME_MAX + 1U];
    sem_t semaphore;
    uint16_t references;
    uint8_t used;
    uint8_t unlinked;
} named_sem_t;

static named_sem_t named_semaphores[AIXOS_CFG_MAX_SEM];

static int name_equal(const char *left, const char *right)
{
    while (*left != '\0' && *left == *right) {
        left++;
        right++;
    }
    return *left == *right;
}

static size_t name_length(const char *name)
{
    size_t length = 0U;
    while (name[length] != '\0') length++;
    return length;
}

static named_sem_t *find_name(const char *name)
{
    uint32_t index;
    for (index = 0U; index < AIXOS_CFG_MAX_SEM; index++) {
        if (named_semaphores[index].used != 0U &&
            named_semaphores[index].unlinked == 0U &&
            name_equal(named_semaphores[index].name, name)) {
            return &named_semaphores[index];
        }
    }
    return NULL;
}

static named_sem_t *find_pointer(sem_t *sem)
{
    uint32_t index;
    for (index = 0U; index < AIXOS_CFG_MAX_SEM; index++) {
        if (named_semaphores[index].used != 0U &&
            &named_semaphores[index].semaphore == sem) {
            return &named_semaphores[index];
        }
    }
    return NULL;
}

static named_sem_t *find_free_entry(void)
{
    uint32_t index;
    for (index = 0U; index < AIXOS_CFG_MAX_SEM; index++) {
        if (named_semaphores[index].used == 0U) {
            return &named_semaphores[index];
        }
    }
    return NULL;
}

sem_t *aixos_posix_sem_open_named(const char *name, int flags,
                                  unsigned int value)
{
    named_sem_t *entry;
    aixos_arch_flags_t interrupt_flags;
    if (name == NULL || name[0] != '/' || name[1] == '\0' ||
        name_length(name) > POSIX_SEM_NAME_MAX ||
        value > AIXOS_CFG_SEM_MAX_COUNT) {
        errno = EINVAL;
        return SEM_FAILED;
    }
    interrupt_flags = aixos_arch_int_disable();
    entry = find_name(name);
    if (entry != NULL) {
        if ((flags & (O_CREAT | O_EXCL)) == (O_CREAT | O_EXCL)) {
            aixos_arch_int_restore(interrupt_flags);
            errno = EEXIST;
            return SEM_FAILED;
        }
        if (entry->references == UINT16_MAX) {
            aixos_arch_int_restore(interrupt_flags);
            errno = EMFILE;
            return SEM_FAILED;
        }
        entry->references++;
        aixos_arch_int_restore(interrupt_flags);
        return &entry->semaphore;
    }
    if ((flags & O_CREAT) == 0) {
        aixos_arch_int_restore(interrupt_flags);
        errno = ENOENT;
        return SEM_FAILED;
    }
    entry = find_free_entry();
    if (entry == NULL) {
        aixos_arch_int_restore(interrupt_flags);
        errno = ENOSPC;
        return SEM_FAILED;
    }
    memset(entry, 0, sizeof(*entry));
    entry->semaphore = aixos_sem_create((int)value);
    if (entry->semaphore == AIXOS_HANDLE_INVALID) {
        aixos_arch_int_restore(interrupt_flags);
        errno = EAGAIN;
        return SEM_FAILED;
    }
    strncpy(entry->name, name, sizeof(entry->name) - 1U);
    entry->references = 1U;
    entry->used = 1U;
    aixos_arch_int_restore(interrupt_flags);
    return &entry->semaphore;
}

int aixos_posix_sem_close_named(sem_t *sem)
{
    named_sem_t *entry;
    aixos_arch_flags_t flags = aixos_arch_int_disable();
    entry = find_pointer(sem);
    if (entry == NULL || entry->references == 0U) {
        aixos_arch_int_restore(flags);
        errno = EINVAL;
        return -1;
    }
    entry->references--;
    if (entry->references == 0U && entry->unlinked != 0U) {
        sem_t native = entry->semaphore;
        aixos_arch_int_restore(flags);
        if (aixos_sem_delete(native) != AIXOS_OK) {
            flags = aixos_arch_int_disable();
            entry->references = 1U;
            aixos_arch_int_restore(flags);
            errno = EBUSY;
            return -1;
        }
        flags = aixos_arch_int_disable();
        memset(entry, 0, sizeof(*entry));
        aixos_arch_int_restore(flags);
        return 0;
    }
    aixos_arch_int_restore(flags);
    return 0;
}

int aixos_posix_sem_unlink_named(const char *name)
{
    named_sem_t *entry;
    sem_t native;
    aixos_arch_flags_t flags;
    if (name == NULL) {
        errno = EINVAL;
        return -1;
    }
    flags = aixos_arch_int_disable();
    entry = find_name(name);
    if (entry == NULL) {
        aixos_arch_int_restore(flags);
        errno = ENOENT;
        return -1;
    }
    entry->unlinked = 1U;
    if (entry->references == 0U) {
        native = entry->semaphore;
        aixos_arch_int_restore(flags);
        if (aixos_sem_delete(native) != AIXOS_OK) {
            errno = EBUSY;
            return -1;
        }
        flags = aixos_arch_int_disable();
        memset(entry, 0, sizeof(*entry));
        aixos_arch_int_restore(flags);
        return 0;
    }
    aixos_arch_int_restore(flags);
    return 0;
}

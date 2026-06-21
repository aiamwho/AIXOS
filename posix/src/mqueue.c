#include "posix/include/mqueue.h"
#include "aixos/arch/arch.h"
#include "config/aixos_cfg.h"
#include "config/string.h"

#define POSIX_MQ_NAME_MAX 31U

typedef struct {
    char name[POSIX_MQ_NAME_MAX + 1U];
    mqd_t descriptor;
    uint16_t references;
    uint8_t used;
    uint8_t unlinked;
} named_mq_t;

static named_mq_t named_queues[AIXOS_CFG_MAX_MQ];

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

static named_mq_t *find_name(const char *name)
{
    uint32_t index;
    for (index = 0U; index < AIXOS_CFG_MAX_MQ; index++) {
        if (named_queues[index].used != 0U &&
            named_queues[index].unlinked == 0U &&
            name_equal(named_queues[index].name, name)) {
            return &named_queues[index];
        }
    }
    return NULL;
}

static named_mq_t *find_descriptor(mqd_t descriptor)
{
    uint32_t index;
    for (index = 0U; index < AIXOS_CFG_MAX_MQ; index++) {
        if (named_queues[index].used != 0U &&
            named_queues[index].descriptor == descriptor) {
            return &named_queues[index];
        }
    }
    return NULL;
}

static named_mq_t *find_free_entry(void)
{
    uint32_t index;
    for (index = 0U; index < AIXOS_CFG_MAX_MQ; index++) {
        if (named_queues[index].used == 0U) return &named_queues[index];
    }
    return NULL;
}

mqd_t aixos_posix_mq_open_named(const char *name, int flags,
                                const struct mq_attr *attr)
{
    named_mq_t *entry;
    aixos_arch_flags_t interrupt_flags;
    mqd_t descriptor;
    if (name == NULL || name[0] != '/' || name[1] == '\0' ||
        name_length(name) > POSIX_MQ_NAME_MAX ||
        (flags & O_NONBLOCK) != 0) {
        errno = (flags & O_NONBLOCK) != 0 ? ENOTSUP : EINVAL;
        return AIXOS_HANDLE_INVALID;
    }
    interrupt_flags = aixos_arch_int_disable();
    entry = find_name(name);
    if (entry != NULL) {
        if ((flags & (O_CREAT | O_EXCL)) == (O_CREAT | O_EXCL)) {
            aixos_arch_int_restore(interrupt_flags);
            errno = EEXIST;
            return AIXOS_HANDLE_INVALID;
        }
        if (entry->references == UINT16_MAX) {
            aixos_arch_int_restore(interrupt_flags);
            errno = EMFILE;
            return AIXOS_HANDLE_INVALID;
        }
        entry->references++;
        descriptor = entry->descriptor;
        aixos_arch_int_restore(interrupt_flags);
        return descriptor;
    }
    if ((flags & O_CREAT) == 0) {
        aixos_arch_int_restore(interrupt_flags);
        errno = ENOENT;
        return AIXOS_HANDLE_INVALID;
    }
    if (attr == NULL || attr->mq_maxmsg <= 0 || attr->mq_msgsize <= 0) {
        aixos_arch_int_restore(interrupt_flags);
        errno = EINVAL;
        return AIXOS_HANDLE_INVALID;
    }
    entry = find_free_entry();
    if (entry == NULL) {
        aixos_arch_int_restore(interrupt_flags);
        errno = ENOSPC;
        return AIXOS_HANDLE_INVALID;
    }
    descriptor = aixos_mq_create((size_t)attr->mq_maxmsg,
                                 (size_t)attr->mq_msgsize);
    if (descriptor == AIXOS_HANDLE_INVALID) {
        aixos_arch_int_restore(interrupt_flags);
        errno = EAGAIN;
        return AIXOS_HANDLE_INVALID;
    }
    memset(entry, 0, sizeof(*entry));
    strncpy(entry->name, name, sizeof(entry->name) - 1U);
    entry->descriptor = descriptor;
    entry->references = 1U;
    entry->used = 1U;
    aixos_arch_int_restore(interrupt_flags);
    return descriptor;
}

int aixos_posix_mq_close_named(mqd_t descriptor)
{
    named_mq_t *entry;
    aixos_arch_flags_t flags = aixos_arch_int_disable();
    entry = find_descriptor(descriptor);
    if (entry == NULL || entry->references == 0U) {
        aixos_arch_int_restore(flags);
        errno = EBADF;
        return -1;
    }
    entry->references--;
    if (entry->references == 0U && entry->unlinked != 0U) {
        int result;
        aixos_arch_int_restore(flags);
        result = aixos_mq_delete(descriptor);
        if (result != AIXOS_OK) {
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

int aixos_posix_mq_unlink_named(const char *name)
{
    named_mq_t *entry;
    mqd_t descriptor;
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
    descriptor = entry->descriptor;
    if (entry->references == 0U) {
        aixos_arch_int_restore(flags);
        if (aixos_mq_delete(descriptor) != AIXOS_OK) {
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

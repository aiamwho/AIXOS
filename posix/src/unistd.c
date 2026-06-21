#include "posix/include/unistd.h"
#include "aixos/aixos.h"
#include "aixos/arch/arch.h"
#include "config/aixos_cfg.h"

typedef struct {
    aixos_handle_t pipe;
    uint8_t readable;
    uint8_t writable;
    uint8_t used;
} descriptor_t;

static descriptor_t descriptors[AIXOS_CFG_POSIX_OPEN_MAX];

static int allocate_descriptor(void)
{
    uint32_t index;
    for (index = 0U; index < AIXOS_CFG_POSIX_OPEN_MAX; index++) {
        if (descriptors[index].used == 0U) return (int)index;
    }
    return -1;
}

int aixos_posix_pipe_open(int result[2])
{
    aixos_arch_flags_t flags;
    aixos_handle_t native;
    int read_descriptor;
    int write_descriptor;
    if (result == NULL) {
        errno = EINVAL;
        return -1;
    }
    flags = aixos_arch_int_disable();
    read_descriptor = allocate_descriptor();
    if (read_descriptor < 0) {
        aixos_arch_int_restore(flags);
        errno = EMFILE;
        return -1;
    }
    descriptors[read_descriptor].used = 1U;
    write_descriptor = allocate_descriptor();
    if (write_descriptor < 0) {
        descriptors[read_descriptor].used = 0U;
        aixos_arch_int_restore(flags);
        errno = EMFILE;
        return -1;
    }
    native = aixos_pipe_create(AIXOS_CFG_POSIX_PIPE_CAPACITY);
    if (native == AIXOS_HANDLE_INVALID) {
        descriptors[read_descriptor].used = 0U;
        aixos_arch_int_restore(flags);
        errno = EAGAIN;
        return -1;
    }
    descriptors[read_descriptor].pipe = native;
    descriptors[read_descriptor].readable = 1U;
    descriptors[write_descriptor].pipe = native;
    descriptors[write_descriptor].writable = 1U;
    descriptors[write_descriptor].used = 1U;
    result[0] = read_descriptor;
    result[1] = write_descriptor;
    aixos_arch_int_restore(flags);
    return 0;
}

ssize_t aixos_posix_read(int descriptor, void *buffer, size_t count)
{
    int result;
    if (descriptor < 0 ||
        (uint32_t)descriptor >= AIXOS_CFG_POSIX_OPEN_MAX ||
        descriptors[descriptor].used == 0U ||
        descriptors[descriptor].readable == 0U) {
        errno = EBADF;
        return -1;
    }
    result = aixos_pipe_read(descriptors[descriptor].pipe, buffer, count,
                             UINT32_MAX);
    if (result < 0) {
        errno = result == AIXOS_ERR_AGAIN ? EAGAIN : EINVAL;
        return -1;
    }
    return (ssize_t)result;
}

ssize_t aixos_posix_write(int descriptor, const void *buffer, size_t count)
{
    int result;
    if (descriptor < 0 ||
        (uint32_t)descriptor >= AIXOS_CFG_POSIX_OPEN_MAX ||
        descriptors[descriptor].used == 0U ||
        descriptors[descriptor].writable == 0U) {
        errno = EBADF;
        return -1;
    }
    result = aixos_pipe_write(descriptors[descriptor].pipe, buffer, count,
                              UINT32_MAX);
    if (result < 0) {
        errno = result == AIXOS_ERR_BUSY ? EAGAIN : EINVAL;
        return -1;
    }
    return (ssize_t)result;
}

int aixos_posix_close(int descriptor)
{
    aixos_handle_t native;
    uint32_t index;
    int references = 0;
    aixos_arch_flags_t flags;
    if (descriptor < 0 ||
        (uint32_t)descriptor >= AIXOS_CFG_POSIX_OPEN_MAX) {
        errno = EBADF;
        return -1;
    }
    flags = aixos_arch_int_disable();
    if (descriptors[descriptor].used == 0U) {
        aixos_arch_int_restore(flags);
        errno = EBADF;
        return -1;
    }
    native = descriptors[descriptor].pipe;
    descriptors[descriptor].used = 0U;
    descriptors[descriptor].readable = 0U;
    descriptors[descriptor].writable = 0U;
    for (index = 0U; index < AIXOS_CFG_POSIX_OPEN_MAX; index++) {
        if (descriptors[index].used != 0U &&
            descriptors[index].pipe == native) {
            references++;
        }
    }
    aixos_arch_int_restore(flags);
    if (references == 0 && aixos_pipe_delete(native) != AIXOS_OK) {
        errno = EBUSY;
        return -1;
    }
    return 0;
}

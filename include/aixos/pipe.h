#ifndef AIXOS_PIPE_H
#define AIXOS_PIPE_H
#include "aixos/types.h"
aixos_handle_t aixos_pipe_create(size_t size);
aixos_handle_t aixos_pipe_create_static(void *buffer, size_t size);
int aixos_pipe_write(aixos_handle_t pipe, const void *data, size_t len, uint32_t timeout_ms);
int aixos_pipe_read(aixos_handle_t pipe, void *buf, size_t len, uint32_t timeout_ms);
int aixos_pipe_write_from_isr(aixos_handle_t pipe, const void *data, size_t len);
int aixos_pipe_delete(aixos_handle_t pipe);
#endif

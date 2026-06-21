#ifndef AIXOS_MQ_H
#define AIXOS_MQ_H
#include "aixos/types.h"

typedef struct {
    size_t max_messages;
    size_t message_size;
    size_t current_messages;
} aixos_mq_info_t;

aixos_handle_t aixos_mq_create(size_t max_msgs, size_t msg_size);
aixos_handle_t aixos_mq_create_static(size_t max_msgs, size_t msg_size,
                                      void *buffer, size_t *lengths);
int aixos_mq_send(aixos_handle_t mq, const void *msg, size_t size, uint32_t timeout_ms);
int aixos_mq_send_priority(aixos_handle_t mq, const void *msg, size_t size,
                           uint32_t priority, uint32_t timeout_ms);
int aixos_mq_send_from_isr(aixos_handle_t mq, const void *msg, size_t size);
int aixos_mq_recv(aixos_handle_t mq, void *buf, size_t capacity, size_t *size,
                  uint32_t timeout_ms);
int aixos_mq_recv_priority(aixos_handle_t mq, void *buf, size_t capacity,
                           size_t *size, uint32_t *priority,
                           uint32_t timeout_ms);
int aixos_mq_get_info(aixos_handle_t mq, aixos_mq_info_t *info);
int aixos_mq_delete(aixos_handle_t mq);
#endif

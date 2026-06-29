#include "test.h"

int test_failures;
int test_checks;

void test_object_handles(void);
void test_heap_allocator(void);
void test_fixed_block_pool(void);
void test_kernel_primitives(void);
void test_reliability_guards(void);
void test_mutex_inheritance(void);
void test_v6_safety_features(void);
void test_heap_operation_sequence(void);
void test_tick_wrap_timeout(void);
void test_dynamic_task_capacity_and_priorities(void);
void test_task_state_transitions(void);
void test_priority_ordered_waiters(void);
void test_task_notifications(void);
void test_posix_compatibility(void);
void test_posix_public_api(void);
void test_microkernel_services(void);
void test_mpu_regions(void);
void test_user_copy_api(void);
void test_namespace_resource_manager(void);
void test_timing_wheel_paths(void);
void test_microkernel_sync_ipc_paths(void);
void test_api_parameter_matrix(void);
void test_blackbox_user_workflows(void);
void test_trace_crash_timer_scheduler_paths(void);
void test_task_signal_and_lifecycle_edges(void);
void test_posix_extended_api_edges(void);
void test_ipc_waiter_edge_paths(void);
void test_posix_named_wrapper_edges(void);
void test_microkernel_user_syscall_edges(void);
void test_kernel_start_and_return_trap_edges(void);
void test_mutex_waiter_and_priority_edges(void);
void test_ipc_isr_and_parameter_edges(void);
void test_heap_object_additional_edges(void);

int main(void)
{
    RUN_TEST(test_object_handles);
    RUN_TEST(test_heap_allocator);
    RUN_TEST(test_fixed_block_pool);
    RUN_TEST(test_kernel_primitives);
    RUN_TEST(test_reliability_guards);
    RUN_TEST(test_mutex_inheritance);
    RUN_TEST(test_v6_safety_features);
    RUN_TEST(test_heap_operation_sequence);
    RUN_TEST(test_tick_wrap_timeout);
    RUN_TEST(test_dynamic_task_capacity_and_priorities);
    RUN_TEST(test_task_state_transitions);
    RUN_TEST(test_priority_ordered_waiters);
    RUN_TEST(test_task_notifications);
    RUN_TEST(test_posix_compatibility);
    RUN_TEST(test_posix_public_api);
    RUN_TEST(test_microkernel_services);
    RUN_TEST(test_mpu_regions);
    RUN_TEST(test_user_copy_api);
    RUN_TEST(test_namespace_resource_manager);
    RUN_TEST(test_timing_wheel_paths);
    RUN_TEST(test_microkernel_sync_ipc_paths);
    RUN_TEST(test_api_parameter_matrix);
    RUN_TEST(test_blackbox_user_workflows);
    RUN_TEST(test_trace_crash_timer_scheduler_paths);
    RUN_TEST(test_task_signal_and_lifecycle_edges);
    RUN_TEST(test_posix_extended_api_edges);
    RUN_TEST(test_microkernel_user_syscall_edges);
    RUN_TEST(test_kernel_start_and_return_trap_edges);
    RUN_TEST(test_mutex_waiter_and_priority_edges);
    RUN_TEST(test_ipc_isr_and_parameter_edges);
    RUN_TEST(test_heap_object_additional_edges);
    RUN_TEST(test_ipc_waiter_edge_paths);
    RUN_TEST(test_posix_named_wrapper_edges);
    printf("%d checks, %d failures\n", test_checks, test_failures);
    return test_failures ? 1 : 0;
}

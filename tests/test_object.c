#include "test.h"
#include "kernel/object.h"

void test_object_handles(void)
{
    int object_a;
    int object_b;
    int slot;
    aixos_handle_t first;
    aixos_handle_t second;

    aixos_object_init();
    slot = aixos_slot_alloc(AIXOS_POOL_SEM, &object_a);
    CHECK(slot >= 0);
    first = aixos_slot_handle(AIXOS_POOL_SEM, slot);
    CHECK(first != AIXOS_HANDLE_INVALID);
    CHECK(aixos_obj_from_handle(first, AIXOS_OBJ_SEM) == &object_a);
    CHECK(aixos_obj_from_handle(first, AIXOS_OBJ_MUTEX) == NULL);

    aixos_slot_free(AIXOS_POOL_SEM, slot);
    CHECK(!aixos_handle_is_valid(first, AIXOS_OBJ_SEM));

    CHECK(aixos_slot_alloc(AIXOS_POOL_SEM, &object_b) == slot);
    second = aixos_slot_handle(AIXOS_POOL_SEM, slot);
    CHECK(second != first);
    CHECK(aixos_obj_from_handle(second, AIXOS_OBJ_SEM) == &object_b);
    CHECK(aixos_obj_from_handle(first, AIXOS_OBJ_SEM) == NULL);

    aixos_slot_free(AIXOS_POOL_SEM, slot);
    CHECK(aixos_test_set_slot_generation(AIXOS_POOL_SEM, slot,
                                         0x00FFFFFFU) == AIXOS_OK);
    CHECK(aixos_slot_alloc(AIXOS_POOL_SEM, &object_a) == slot);
    CHECK(AIXOS_HANDLE_GEN(aixos_slot_handle(AIXOS_POOL_SEM, slot)) == 1U);
}

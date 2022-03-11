#define _GNU_SOURCE

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <inttypes.h>
#include <unistd.h>
#include <stdbool.h>
#include <setjmp.h>
#include <cmocka.h>

#include "uci.h"

#include "utils/log.h"
#include "utils/uci_wrt.h"
#include "utils/utarray.h"
#include "utils/iface_mapper.h"

static void test_uwrt_init_context(void **state)
{
  (void) state;

  struct uctx *context = uwrt_init_context(NULL);
  assert_non_null(context);
  uwrt_free_context(context);
#ifdef TEST_UCI_CONFIG_DIR
  context = uwrt_init_context(TEST_UCI_CONFIG_DIR);
  assert_non_null(context);
  uwrt_free_context(context);
#endif
}

#ifdef TEST_UCI_CONFIG_DIR
static void test_uwrt_get_interfaces(void **state)
{
  (void) state;

  struct uctx *context = uwrt_init_context(TEST_UCI_CONFIG_DIR);
  netif_info_t *ptr = NULL;
  UT_array *interfaces = uwrt_get_interfaces(context, NULL);
  assert_non_null(interfaces);

  ptr = (netif_info_t *) utarray_next(interfaces, ptr);
  assert_string_equal(ptr->ifname, "lo");
  assert_string_equal(ptr->ip_addr, "127.0.0.1");

  ptr = (netif_info_t *) utarray_next(interfaces, ptr);
  assert_string_equal(ptr->ifname, "lan0 lan1 lan2 lan3 lan4");
  assert_string_equal(ptr->ip_addr, "192.168.1.1");

  ptr = (netif_info_t *) utarray_next(interfaces, ptr);
  assert_string_equal(ptr->ifname, "eth2");
  assert_string_equal(ptr->ip_addr, "");

  ptr = (netif_info_t *) utarray_next(interfaces, ptr);
  assert_string_equal(ptr->ifname, "@wan");
  assert_string_equal(ptr->ip_addr, "");

  utarray_free(interfaces);
  ptr = NULL;

  interfaces = uwrt_get_interfaces(context, "loopback");
  assert_non_null(interfaces);
  assert_int_equal(utarray_len(interfaces), 1);

  ptr = (netif_info_t *) utarray_next(interfaces, ptr);
  assert_string_equal(ptr->ifname, "lo");
  assert_string_equal(ptr->ip_addr, "127.0.0.1");

  utarray_free(interfaces);
  ptr = NULL;

  interfaces = uwrt_get_interfaces(context, "lan1");
  assert_non_null(interfaces);
  ptr = (netif_info_t *) utarray_next(interfaces, ptr);
  assert_null(ptr);

  uwrt_free_context(context);
}

static void test_uwrt_create_interface(void **state)
{
  (void) state;

  struct uci_ptr p;
  netif_info_t *ptr = NULL;
  struct uctx *context = uwrt_init_context(TEST_UCI_CONFIG_DIR);
  assert_int_equal(uwrt_create_interface(context, "br0", "bridge", "10.0.0.1", "10.0.0.255", "255.255.255.0"), 0);
  UT_array *interfaces = uwrt_get_interfaces(context, "br0");
  assert_non_null(interfaces);

  ptr = (netif_info_t *) utarray_next(interfaces, ptr);
  assert_string_equal(ptr->ifname, "br0");
  assert_string_equal(ptr->ip_addr, "10.0.0.1");
  utarray_free(interfaces);

  char *property = os_strdup("network.br0");

  assert_int_equal(uci_lookup_ptr(context->uctx, &p, property, true), UCI_OK);
  assert_int_equal(uci_delete(context->uctx, &p), UCI_OK);
  assert_int_equal(uci_save(context->uctx, p.p), UCI_OK);

  uwrt_free_context(context);
}
#endif

int main(int argc, char *argv[])
{  
  (void) argc;
  (void) argv;

  log_set_quiet(false);

  const struct CMUnitTest tests[] = {
    cmocka_unit_test(test_uwrt_init_context),
#ifdef TEST_UCI_CONFIG_DIR
    cmocka_unit_test(test_uwrt_get_interfaces),
    cmocka_unit_test(test_uwrt_create_interface),
#endif
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
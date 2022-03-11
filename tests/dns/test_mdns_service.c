#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <ctype.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>
#include <errno.h>
#include <net/if.h>
#include <libgen.h>
#include <setjmp.h>
#include <cmocka.h>

#include "utils/log.h"
#include "utils/os.h"
#include "dns/mdns_service.h"
#include "capture/pcap_service.h"
#include "utils/eloop.h"

int __wrap_run_pcap(char *interface, bool immediate, bool promiscuous,
             int timeout, char *filter, bool nonblock, capture_callback_fn pcap_fn,
             void *fn_ctx, struct pcap_context** pctx)
{
  (void) interface;
  (void) immediate;
  (void) promiscuous;
  (void) timeout;
  (void) filter;
  (void) nonblock;
  (void) pcap_fn;
  (void) fn_ctx;

  *pctx = (struct pcap_context*) os_zalloc(sizeof(struct pcap_context));

  return 0;
}

int __wrap_eloop_register_read_sock(int sock, eloop_sock_handler handler,
			     void *eloop_data, void *user_data)
{
  (void) sock;
  (void) handler;
  (void) eloop_data;
  (void) user_data;

  return 0;
}

int __wrap_eloop_init(void)
{
  return 0;
}

void __wrap_eloop_destroy(void) {}
void __wrap_eloop_run(void) {}

static void test_run_mdns(void **state)
{
  (void) state;
  
  struct mdns_context context;
  context.ifname = os_strdup("wlan0");
  assert_int_equal(run_mdns(&context), 0);
  os_free(context.ifname);
  close_mdns(&context);
}

static void test_close_mdns(void **state)
{
  (void) state;
  
  struct mdns_context context;
  context.ifname = os_strdup("wlan0");
  run_mdns(&context);
  os_free(context.ifname);
  assert_int_equal(close_mdns(&context), 0);
  
}

int main(int argc, char *argv[])
{  
  (void) argc; /* unused */
  (void) argv; /* unused */

  log_set_quiet(false);

  const struct CMUnitTest tests[] = {
    cmocka_unit_test(test_run_mdns),
    cmocka_unit_test(test_close_mdns),
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
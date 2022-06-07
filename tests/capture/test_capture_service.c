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
#include <setjmp.h>
#include <cmocka.h>

#include "utils/utarray.h"
#include "utils/log.h"
#include "utils/eloop.h"
#include "capture/capture_service.h"
#include "capture/header_middleware/header_middleware.h"
#include "capture/pcap_middleware/pcap_middleware.h"
#include "capture/pcap_service.h"
#include "capture/pcap_middleware/pcap_queue.h"
#include "capture/header_middleware/packet_queue.h"

static const UT_icd tp_list_icd = {sizeof(struct tuple_packet), NULL, NULL,
                                   NULL};

int __wrap_init_sqlite_header_db(sqlite3 *db) {
  (void)db;

  return 0;
}

int __wrap_init_sqlite_pcap_db(sqlite3 *db) {
  (void)db;

  return 0;
}

int __wrap_run_pcap(char *interface, bool immediate, bool promiscuous,
                    int timeout, char *filter, bool nonblock,
                    capture_callback_fn pcap_fn, void *fn_ctx,
                    struct pcap_context **pctx) {
  (void)fn_ctx;
  (void)pcap_fn;

  assert_string_equal(interface, "wlan0");
  assert_true(immediate);
  assert_true(promiscuous);
  assert_int_equal(timeout, 100);
  assert_string_equal(filter, "port 80");
  assert_true(nonblock);
  *pctx = os_zalloc(sizeof(struct pcap_context));
  return 0;
}

void __wrap_close_pcap(struct pcap_context *ctx) {
  if (ctx != NULL)
    os_free(ctx);
}

struct eloop_data *__wrap_eloop_init(void) {
  return (struct eloop_data *)mock();
}

int __wrap_eloop_register_read_sock(struct eloop_data *eloop, int sock,
                                    eloop_sock_handler handler,
                                    void *eloop_data, void *user_data) {
  (void)eloop;
  (void)sock;
  (void)handler;
  (void)eloop_data;
  (void)user_data;

  return 0;
}

int __wrap_eloop_register_timeout(struct eloop_data *eloop, unsigned long secs,
                                  unsigned long usecs,
                                  eloop_timeout_handler handler,
                                  void *eloop_data, void *user_data) {
  (void)eloop;
  (void)secs;
  (void)usecs;
  (void)handler;
  (void)eloop_data;
  (void)user_data;

  return 0;
}

void __wrap_eloop_run(struct eloop_data *eloop) { (void)eloop; }

void __wrap_eloop_free(struct eloop_data *eloop) { (void)eloop; }

uint32_t __wrap_run_register_db(char *address, char *name) {
  (void)address;
  (void)name;
  return 1;
}

int __wrap_extract_packets(const struct pcap_pkthdr *header,
                           const uint8_t *packet, char *interface,
                           char *hostname, char *id, UT_array **tp_array) {
  (void)header;
  (void)packet;
  (void)id;
  (void)hostname;
  (void)interface;

  struct tuple_packet tp;
  utarray_new(*tp_array, &tp_list_icd);

  tp.packet = NULL;
  tp.type = PACKET_ETHERNET;

  utarray_push_back(*tp_array, &tp);

  return 1;
}

struct packet_queue *__wrap_push_packet_queue(struct packet_queue *queue,
                                              struct tuple_packet tp) {
  assert_int_equal(tp.type, PACKET_ETHERNET);
  return queue;
}

struct pcap_queue *__wrap_push_pcap_queue(struct pcap_queue *queue,
                                          struct pcap_pkthdr *header,
                                          uint8_t *packet) {
  (void)packet;
  assert_int_equal(header->caplen, 100);
  assert_int_equal(header->len, 100);

  return queue;
}

void capture_config(struct capture_conf *config) {
  os_memset(config, 0, sizeof(struct capture_conf));

  config->promiscuous = true;
  config->immediate = true;
  config->buffer_timeout = 100;
  strcpy(config->filter, "port 80");
}

static void test_run_capture(void **state) {
  (void)state; /* unused */

  char *ifname = "wlan0";
  struct capture_conf config;

  capture_config(&config);

  struct eloop_data *eloop = os_zalloc(sizeof(struct eloop_data));
  assert_non_null(eloop);

  will_return_always(__wrap_eloop_init, eloop);
  int ret = run_capture(ifname, &config);

  assert_int_equal(ret, 0);
  os_free(eloop);
}

int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;

  log_set_quiet(false);

  const struct CMUnitTest tests[] = {cmocka_unit_test(test_run_capture)};

  return cmocka_run_group_tests(tests, NULL, NULL);
}
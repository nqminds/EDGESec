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

#include "supervisor/cmd_processor.h"

#include "utils/hashmap.h"
#include "utils/utarray.h"
#include "utils/log.h"
#include "utils/iptables.h"
#include "engine.h"

#define CMD_DELIMITER 0x20

ssize_t __wrap_write_domain_data(int sock, char *data, size_t data_len, struct sockaddr_un *addr, int addr_len)
{
  return data_len;
}

int __wrap_accept_mac_cmd(struct supervisor_context *context, uint8_t *mac_addr, int vlanid)
{
  check_expected(mac_addr);
  check_expected(vlanid);
  return 0;
}

int __wrap_deny_mac_cmd(struct supervisor_context *context, uint8_t *mac_addr)
{
  check_expected(mac_addr);
  return 0;
}

int __wrap_add_nat_cmd(struct supervisor_context *context, uint8_t *mac_addr)
{
  check_expected(mac_addr);
  return 0;
}

int __wrap_remove_nat_cmd(struct supervisor_context *context, uint8_t *mac_addr)
{
  check_expected(mac_addr);
  return 0;
}

int __wrap_assign_psk_cmd(struct supervisor_context *context, uint8_t *mac_addr, char *pass, int pass_len)
{
  check_expected(mac_addr);
  check_expected(pass);
  check_expected(pass_len);
  return 0;
}

int __wrap_set_ip_cmd(struct supervisor_context *context, uint8_t *mac_addr, char *ip_addr, bool add)
{

  check_expected(mac_addr);
  check_expected(ip_addr);
  check_expected(add);
  return 0;
}

int __wrap_add_bridge_cmd(struct supervisor_context *context, uint8_t *left_mac_addr, uint8_t *right_mac_addr)
{
  check_expected(left_mac_addr);
  check_expected(right_mac_addr);
  return 0;
}

int __wrap_remove_bridge_cmd(struct supervisor_context *context, uint8_t *left_mac_addr, uint8_t *right_mac_addr)
{
  check_expected(left_mac_addr);
  check_expected(right_mac_addr);
  return 0;
}

int __wrap_set_fingerprint_cmd(struct supervisor_context *context, char *src_mac_addr,
                        char *dst_mac_addr, char *protocol, char *fingerprint,
                        uint64_t timestamp, char *query)
{
  check_expected(src_mac_addr);
  check_expected(dst_mac_addr);
  check_expected(protocol);
  check_expected(fingerprint);
  check_expected(timestamp);
  check_expected(query);
  return 0;
}

ssize_t __wrap_query_fingerprint_cmd(struct supervisor_context *context, char *mac_addr, uint64_t timestamp,
                        char *op, char *protocol, char **out)
{
  check_expected(mac_addr);
  check_expected(timestamp);
  check_expected(op);
  check_expected(protocol);
  check_expected(out);
  return strlen(OK_REPLY);
}

uint8_t* __wrap_register_ticket_cmd(struct supervisor_context *context, uint8_t *mac_addr, char *label,
                        int vlanid)
{
  check_expected(mac_addr);
  check_expected(label);
  check_expected(vlanid);
  return OK_REPLY;
}

int __wrap_clear_psk_cmd(struct supervisor_context *context, uint8_t *mac_addr)
{
  check_expected(mac_addr);
  return 0;
}

int __wrap_get_mac_mapper(hmap_mac_conn **hmap, uint8_t mac_addr[ETH_ALEN], struct mac_conn_info *info)
{
  check_expected(mac_addr);
  check_expected(info);
  return 1;
}

static void test_process_domain_buffer(void **state)
{
  (void) state; /* unused */  

  UT_array *arr;
  utarray_new(arr, &ut_str_icd);

  char buf1[5] = {'c', CMD_DELIMITER, 'a', CMD_DELIMITER, 'b'};

  bool ret = process_domain_buffer(buf1, 5, arr, CMD_DELIMITER);

  assert_true(ret);

  char **p = NULL;
  p = (char**)utarray_next(arr, p);
  assert_string_equal(*p, "c");
  p = (char**)utarray_next(arr, p);
  assert_string_equal(*p, "a");
  p = (char**)utarray_next(arr, p);
  assert_string_equal(*p, "b");

  p = (char**)utarray_next(arr, p);
  assert_ptr_equal(p, NULL);

  utarray_free(arr);

  utarray_new(arr, &ut_str_icd);
  char buf2[4] = {'P', 'I', 'N', 'G'};
  ret = process_domain_buffer(buf2, 4, arr, CMD_DELIMITER);

  assert_true(ret);

  p = (char**)utarray_next(arr, p);
  assert_string_equal(*p, "PING");

  utarray_free(arr);
}

static void test_process_accept_mac_cmd(void **state)
{
  (void) state; /* unused */
  uint8_t addr[ETH_ALEN] = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
  UT_array *cmd_arr;
  struct client_address claddr;

  utarray_new(cmd_arr, &ut_str_icd);
  assert_int_not_equal(split_string_array("ACCEPT_MAC aa:bb:cc:dd:ee:ff 3", CMD_DELIMITER, cmd_arr), -1); 
  expect_memory(__wrap_accept_mac_cmd, mac_addr, addr, ETH_ALEN);
  expect_value(__wrap_accept_mac_cmd, vlanid, 3);
  assert_int_equal(process_accept_mac_cmd(0, &claddr, NULL, cmd_arr), strlen(OK_REPLY));
  utarray_free(cmd_arr);

  utarray_new(cmd_arr, &ut_str_icd);
  assert_int_not_equal(split_string_array("ACCEPT_MAC aa:bb:cc:dd:ee: 3", CMD_DELIMITER, cmd_arr), -1); 
  assert_int_equal(process_accept_mac_cmd(0, &claddr, NULL, cmd_arr), strlen(FAIL_REPLY));
  utarray_free(cmd_arr);

  utarray_new(cmd_arr, &ut_str_icd);
  assert_int_not_equal(split_string_array("ACCEPT_MAC aa:bb:cc:dd:ee:ff", CMD_DELIMITER, cmd_arr), -1); 
  assert_int_equal(process_accept_mac_cmd(0, &claddr, NULL, cmd_arr), strlen(FAIL_REPLY));
  utarray_free(cmd_arr);
}

static void test_process_deny_mac_cmd(void **state)
{
  (void) state; /* unused */
  uint8_t addr[ETH_ALEN] = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
  UT_array *cmd_arr;
  struct client_address claddr;

  utarray_new(cmd_arr, &ut_str_icd);
  assert_int_not_equal(split_string_array("DENY_MAC aa:bb:cc:dd:ee:ff", CMD_DELIMITER, cmd_arr), -1); 
  expect_memory(__wrap_deny_mac_cmd, mac_addr, addr, ETH_ALEN);
  assert_int_equal(process_deny_mac_cmd(0, &claddr, NULL, cmd_arr), strlen(OK_REPLY));
  utarray_free(cmd_arr);

  utarray_new(cmd_arr, &ut_str_icd);
  assert_int_not_equal(split_string_array("DENY_MAC aa:bb:cc:dd:ee:", CMD_DELIMITER, cmd_arr), -1); 
  assert_int_equal(process_deny_mac_cmd(0, &claddr, NULL, cmd_arr), strlen(FAIL_REPLY));
  utarray_free(cmd_arr);
}

static void test_process_add_nat_cmd(void **state)
{
  (void) state; /* unused */
  uint8_t addr[ETH_ALEN] = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
  UT_array *cmd_arr;
  struct client_address claddr;

  utarray_new(cmd_arr, &ut_str_icd);
  assert_int_not_equal(split_string_array("ADD_NAT aa:bb:cc:dd:ee:ff", CMD_DELIMITER, cmd_arr), -1); 
  expect_memory(__wrap_add_nat_cmd, mac_addr, addr, ETH_ALEN);
  assert_int_equal(process_add_nat_cmd(0, &claddr, NULL, cmd_arr), strlen(OK_REPLY));
  utarray_free(cmd_arr);

  utarray_new(cmd_arr, &ut_str_icd);
  assert_int_not_equal(split_string_array("ADD_NAT aa:bb:cc:dd:ee:", CMD_DELIMITER, cmd_arr), -1); 
  assert_int_equal(process_add_nat_cmd(0, &claddr, NULL, cmd_arr), strlen(FAIL_REPLY));
  utarray_free(cmd_arr);
}

static void test_process_remove_nat_cmd(void **state)
{
  (void) state; /* unused */
  uint8_t addr[ETH_ALEN] = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
  UT_array *cmd_arr;
  struct client_address claddr;

  utarray_new(cmd_arr, &ut_str_icd);
  assert_int_not_equal(split_string_array("REMOVE_NAT aa:bb:cc:dd:ee:ff", CMD_DELIMITER, cmd_arr), -1); 
  expect_memory(__wrap_remove_nat_cmd, mac_addr, addr, ETH_ALEN);
  assert_int_equal(process_remove_nat_cmd(0, &claddr, NULL, cmd_arr), strlen(OK_REPLY));
  utarray_free(cmd_arr);

  utarray_new(cmd_arr, &ut_str_icd);
  assert_int_not_equal(split_string_array("REMOVE_NAT aa:bb:cc:dd:ee:", CMD_DELIMITER, cmd_arr), -1); 
  assert_int_equal(process_remove_nat_cmd(0, &claddr, NULL, cmd_arr), strlen(FAIL_REPLY));
  utarray_free(cmd_arr);
}

static void test_process_assign_psk_cmd(void **state)
{
  (void) state; /* unused */

  uint8_t addr[ETH_ALEN] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
  uint8_t password[5] = {0x31, 0x32, 0x33, 0x34, 0x35};
  UT_array *cmd_arr;
  struct client_address claddr;

  utarray_new(cmd_arr, &ut_str_icd);

  assert_int_not_equal(split_string_array("ASSIGN_PSK 11:22:33:44:55:66 12345", CMD_DELIMITER, cmd_arr), -1);
  expect_memory(__wrap_assign_psk_cmd, mac_addr, addr, ETH_ALEN);
  expect_memory(__wrap_assign_psk_cmd, pass, password, 5);
  expect_value(__wrap_assign_psk_cmd, pass_len, 5);

  assert_int_equal(process_assign_psk_cmd(0, &claddr, NULL, cmd_arr), strlen(OK_REPLY));
  utarray_free(cmd_arr);

  utarray_new(cmd_arr, &ut_str_icd);
  assert_int_not_equal(split_string_array("ASSIGN_PSK 11:22:33:44:55: 12345", CMD_DELIMITER, cmd_arr), -1); 
  assert_int_equal(process_assign_psk_cmd(0, &claddr, NULL, cmd_arr), strlen(FAIL_REPLY));
  utarray_free(cmd_arr);

  utarray_new(cmd_arr, &ut_str_icd);
  assert_int_not_equal(split_string_array("ASSIGN_PSK 11:22:33:44:55:66", CMD_DELIMITER, cmd_arr), -1); 
  assert_int_equal(process_assign_psk_cmd(0, &claddr, NULL, cmd_arr), strlen(FAIL_REPLY));
  utarray_free(cmd_arr);

  utarray_new(cmd_arr, &ut_str_icd);
  assert_int_not_equal(split_string_array("ASSIGN_PSK 11:22:33:44:55:66 ", CMD_DELIMITER, cmd_arr), -1); 
  assert_int_equal(process_assign_psk_cmd(0, &claddr, NULL, cmd_arr), strlen(FAIL_REPLY));
  utarray_free(cmd_arr);

  utarray_new(cmd_arr, &ut_str_icd);
  assert_int_not_equal(split_string_array("ASSIGN_PSK 11:22:33:44:55: 12345", CMD_DELIMITER, cmd_arr), -1); 
  assert_int_equal(process_assign_psk_cmd(0, &claddr, NULL, cmd_arr), strlen(FAIL_REPLY));
  utarray_free(cmd_arr);
}


static void test_process_get_map_cmd(void **state)
{
  (void) state; /* unused */
  uint8_t addr[ETH_ALEN] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
  UT_array *cmd_arr;
  struct client_address claddr;
  struct supervisor_context context;
  os_memset(&context, 0, sizeof(struct supervisor_context));

  utarray_new(cmd_arr, &ut_str_icd);

  assert_int_not_equal(split_string_array("GET_MAP 11:22:33:44:55:66", CMD_DELIMITER, cmd_arr), -1);
  expect_memory(__wrap_get_mac_mapper, mac_addr, addr, ETH_ALEN);
  expect_any(__wrap_get_mac_mapper, info);

  int ret = process_get_map_cmd(0, &claddr, &context, cmd_arr);
  bool comp = ret > STRLEN("11:22:33:44:55:66");
  assert_true(comp);
  utarray_free(cmd_arr);

  utarray_new(cmd_arr, &ut_str_icd);
  assert_int_not_equal(split_string_array("GET_MAP 11:22:33:44:55:", CMD_DELIMITER, cmd_arr), -1);
  assert_int_equal(process_get_map_cmd(0, &claddr, &context, cmd_arr), strlen(FAIL_REPLY));
  utarray_free(cmd_arr);
}

static void test_process_get_all_cmd(void **state)
{
  (void) state; /* unused */

  struct supervisor_context ctx;
  uint8_t addr1[ETH_ALEN] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
  uint8_t addr2[ETH_ALEN] = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60};
  struct mac_conn p;
  UT_array *cmd_arr;
  struct client_address claddr;

  utarray_new(cmd_arr, &ut_str_icd);

  os_memset(&ctx, 0, sizeof(struct supervisor_context));

  assert_int_not_equal(split_string_array("GET_ALL", CMD_DELIMITER, cmd_arr), -1);
  assert_int_equal(process_get_all_cmd(0, &claddr, &ctx, cmd_arr), strlen(OK_REPLY));
  utarray_free(cmd_arr);

  utarray_new(cmd_arr, &ut_str_icd);

  assert_int_not_equal(split_string_array("GET_ALL", CMD_DELIMITER, cmd_arr), -1);
  os_memset(&p, 0, sizeof(struct mac_conn));
  os_memcpy(p.mac_addr, addr1, ETH_ALEN);
  put_mac_mapper(&(ctx.mac_mapper), p);

  os_memset(&p, 0, sizeof(struct mac_conn));
  os_memcpy(p.mac_addr, addr2, ETH_ALEN);
  put_mac_mapper(&(ctx.mac_mapper), p);

  int ret = process_get_all_cmd(0,&claddr, &ctx, cmd_arr);
  bool comp = ret > 2 * STRLEN("11:22:33:44:55:66");
  assert_true(comp);
  utarray_free(cmd_arr);
  free_mac_mapper(&(ctx.mac_mapper));
}

static void test_process_set_ip_cmd(void **state)
{
  (void) state; /* unused */

  uint8_t addr[ETH_ALEN] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
  char *ip = "10.0.1.23";
  UT_array *cmd_arr;
  struct client_address claddr;

  utarray_new(cmd_arr, &ut_str_icd);
  assert_int_not_equal(split_string_array("SET_IP add 11:22:33:44:55:66 10.0.1.23", CMD_DELIMITER, cmd_arr), -1);
  expect_memory(__wrap_set_ip_cmd, mac_addr, addr, ETH_ALEN);
  expect_string(__wrap_set_ip_cmd, ip_addr, ip);
  expect_value(__wrap_set_ip_cmd, add, 1);
  assert_int_equal(process_set_ip_cmd(0, &claddr, NULL, cmd_arr), strlen(OK_REPLY));
  utarray_free(cmd_arr);

  utarray_new(cmd_arr, &ut_str_icd);
  assert_int_not_equal(split_string_array("SET_IP old 11:22:33:44:55:66 10.0.1.23", CMD_DELIMITER, cmd_arr), -1);
  expect_memory(__wrap_set_ip_cmd, mac_addr, addr, ETH_ALEN);
  expect_string(__wrap_set_ip_cmd, ip_addr, ip);
  expect_value(__wrap_set_ip_cmd, add, 1);  
  assert_int_equal(process_set_ip_cmd(0, &claddr, NULL, cmd_arr), strlen(OK_REPLY));
  utarray_free(cmd_arr);

  utarray_new(cmd_arr, &ut_str_icd);
  assert_int_not_equal(split_string_array("SET_IP ol 11:22:33:44:55:66 10.0.1.23", CMD_DELIMITER, cmd_arr), -1);
  expect_memory(__wrap_set_ip_cmd, mac_addr, addr, ETH_ALEN);
  expect_string(__wrap_set_ip_cmd, ip_addr, ip);
  expect_value(__wrap_set_ip_cmd, add, 0);
  assert_int_equal(process_set_ip_cmd(0, &claddr, NULL, cmd_arr), strlen(OK_REPLY));
  utarray_free(cmd_arr);

  utarray_new(cmd_arr, &ut_str_icd);
  assert_int_not_equal(split_string_array("SET_IP 11:22:33:44:55:66 10.0.1.23", CMD_DELIMITER, cmd_arr), -1);
  assert_int_equal(process_set_ip_cmd(0, &claddr, NULL, cmd_arr), strlen(FAIL_REPLY));
  utarray_free(cmd_arr);

  utarray_new(cmd_arr, &ut_str_icd);
  assert_int_not_equal(split_string_array("SET_IP old 11:22:33:44:55: 10.0.1.23", CMD_DELIMITER, cmd_arr), -1);
  assert_int_equal(process_set_ip_cmd(0, &claddr, NULL, cmd_arr), strlen(FAIL_REPLY));
  utarray_free(cmd_arr);

  utarray_new(cmd_arr, &ut_str_icd);
  assert_int_not_equal(split_string_array("SET_IP old 11:22:33:44:55:65 a.0.1.23", CMD_DELIMITER, cmd_arr), -1);
  assert_int_equal(process_set_ip_cmd(0, &claddr, NULL, cmd_arr), strlen(FAIL_REPLY));
  utarray_free(cmd_arr);

}

static void test_process_add_bridge_cmd(void **state)
{
  (void) state; /* unused */

  uint8_t addr1[ETH_ALEN] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
  uint8_t addr2[ETH_ALEN] = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
  UT_array *cmd_arr;
  struct client_address claddr;

  utarray_new(cmd_arr, &ut_str_icd);
  assert_int_not_equal(split_string_array("ADD_BRIDGE 11:22:33:44:55:66 aa:bb:cc:dd:ee:ff", CMD_DELIMITER, cmd_arr), -1);
  expect_memory(__wrap_add_bridge_cmd, left_mac_addr, addr1, ETH_ALEN);
  expect_memory(__wrap_add_bridge_cmd, right_mac_addr, addr2, ETH_ALEN);
  assert_int_equal(process_add_bridge_cmd(0, &claddr, NULL, cmd_arr), strlen(OK_REPLY));
  utarray_free(cmd_arr);

  utarray_new(cmd_arr, &ut_str_icd);
  assert_int_not_equal(split_string_array("ADD_BRIDGE 11:22:33:44:55: aa:bb:cc:dd:ee:ff", CMD_DELIMITER, cmd_arr), -1);
  assert_int_equal(process_add_bridge_cmd(0, &claddr, NULL, cmd_arr), strlen(FAIL_REPLY));
  utarray_free(cmd_arr);

  utarray_new(cmd_arr, &ut_str_icd);
  assert_int_not_equal(split_string_array("ADD_BRIDGE 11:22:33:44:55:66 aa:bb:cc:dd:ee:", CMD_DELIMITER, cmd_arr), -1);
  assert_int_equal(process_add_bridge_cmd(0, &claddr, NULL, cmd_arr), strlen(FAIL_REPLY));
  utarray_free(cmd_arr);

  utarray_new(cmd_arr, &ut_str_icd);
  assert_int_not_equal(split_string_array("ADD_BRIDGE", CMD_DELIMITER, cmd_arr), -1);
  assert_int_equal(process_add_bridge_cmd(0, &claddr, NULL, cmd_arr), strlen(FAIL_REPLY));
  utarray_free(cmd_arr);
}

static void test_process_remove_bridge_cmd(void **state)
{
  (void) state; /* unused */

  uint8_t addr1[ETH_ALEN] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
  uint8_t addr2[ETH_ALEN] = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
  UT_array *cmd_arr;
  struct client_address claddr;

  utarray_new(cmd_arr, &ut_str_icd);
  assert_int_not_equal(split_string_array("REMOVE_BRIDGE 11:22:33:44:55:66 aa:bb:cc:dd:ee:ff", CMD_DELIMITER, cmd_arr), -1);
  expect_memory(__wrap_remove_bridge_cmd, left_mac_addr, addr1, ETH_ALEN);
  expect_memory(__wrap_remove_bridge_cmd, right_mac_addr, addr2, ETH_ALEN);
  assert_int_equal(process_remove_bridge_cmd(0, &claddr, NULL, cmd_arr), strlen(OK_REPLY));
  utarray_free(cmd_arr);

  utarray_new(cmd_arr, &ut_str_icd);
  assert_int_not_equal(split_string_array("REMOVE_BRIDGE 11:22:33:44:55: aa:bb:cc:dd:ee:ff", CMD_DELIMITER, cmd_arr), -1);
  assert_int_equal(process_remove_bridge_cmd(0, &claddr, NULL, cmd_arr), strlen(FAIL_REPLY));
  utarray_free(cmd_arr);

  utarray_new(cmd_arr, &ut_str_icd);
  assert_int_not_equal(split_string_array("REMOVE_BRIDGE 11:22:33:44:55:66 aa:bb:cc:dd:ee:", CMD_DELIMITER, cmd_arr), -1);
  assert_int_equal(process_remove_bridge_cmd(0, &claddr, NULL, cmd_arr), strlen(FAIL_REPLY));
  utarray_free(cmd_arr);

  utarray_new(cmd_arr, &ut_str_icd);
  assert_int_not_equal(split_string_array("REMOVE_BRIDGE", CMD_DELIMITER, cmd_arr), -1);
  assert_int_equal(process_remove_bridge_cmd(0, &claddr, NULL, cmd_arr), strlen(FAIL_REPLY));
  utarray_free(cmd_arr);
}

static void test_process_set_fingerprint_cmd(void **state)
{
  (void) state; /* unused */

  UT_array *cmd_arr;
  struct client_address claddr;

  utarray_new(cmd_arr, &ut_str_icd);
  assert_int_not_equal(split_string_array("SET_FINGERPRINT 11:22:33:44:55:66 aa:bb:cc:dd:ee:ff IP 12345 test", CMD_DELIMITER, cmd_arr), -1);
  expect_string(__wrap_set_fingerprint_cmd, src_mac_addr, "11:22:33:44:55:66");
  expect_string(__wrap_set_fingerprint_cmd, dst_mac_addr, "aa:bb:cc:dd:ee:ff");
  expect_string(__wrap_set_fingerprint_cmd, protocol, "IP");
  expect_string(__wrap_set_fingerprint_cmd, fingerprint, "12345");
  expect_any(__wrap_set_fingerprint_cmd, timestamp);
  expect_string(__wrap_set_fingerprint_cmd, query, "test");
  assert_int_equal(process_set_fingerprint_cmd(0, &claddr, NULL, cmd_arr), strlen(OK_REPLY));
  utarray_free(cmd_arr);

  utarray_new(cmd_arr, &ut_str_icd);
  assert_int_not_equal(split_string_array("SET_FINGERPRINT 11:22:33:44:55: aa:bb:cc:dd:ee:ff IP 12345 test", CMD_DELIMITER, cmd_arr), -1);
  assert_int_equal(process_set_fingerprint_cmd(0, &claddr, NULL, cmd_arr), strlen(FAIL_REPLY));
  utarray_free(cmd_arr);

  utarray_new(cmd_arr, &ut_str_icd);
  assert_int_not_equal(split_string_array("SET_FINGERPRINT 11:22:33:44:55:66 aa:bb:cc:dd:ee: IP 12345 test", CMD_DELIMITER, cmd_arr), -1);
  assert_int_equal(process_set_fingerprint_cmd(0, &claddr, NULL, cmd_arr), strlen(FAIL_REPLY));
  utarray_free(cmd_arr);

  utarray_new(cmd_arr, &ut_str_icd);
  assert_int_not_equal(split_string_array("SET_FINGERPRINT 11:22:33:44:55:66 aa:bb:cc:dd:ee:ff 12345 test", CMD_DELIMITER, cmd_arr), -1);
  assert_int_equal(process_set_fingerprint_cmd(0, &claddr, NULL, cmd_arr), strlen(FAIL_REPLY));
  utarray_free(cmd_arr);

  utarray_new(cmd_arr, &ut_str_icd);
  assert_int_not_equal(split_string_array("SET_FINGERPRINT 11:22:33:44:55:66 aa:bb:cc:dd:ee:ff IP ", CMD_DELIMITER, cmd_arr), -1);
  assert_int_equal(process_set_fingerprint_cmd(0, &claddr, NULL, cmd_arr), strlen(FAIL_REPLY));
  utarray_free(cmd_arr);
}

static void test_process_query_fingerprint_cmd(void **state)
{
  (void) state; /* unused */

  UT_array *cmd_arr;
  struct client_address claddr;

  utarray_new(cmd_arr, &ut_str_icd);
  assert_int_not_equal(split_string_array("QUERY_FINGERPRINT 11:22:33:44:55:66 12345 >= IP4", CMD_DELIMITER, cmd_arr), -1);
  expect_string(__wrap_query_fingerprint_cmd, mac_addr, "11:22:33:44:55:66");
  expect_value(__wrap_query_fingerprint_cmd, timestamp, 12345);
  expect_string(__wrap_query_fingerprint_cmd, op, ">=");
  expect_string(__wrap_query_fingerprint_cmd, protocol, "IP4");
  expect_any(__wrap_query_fingerprint_cmd, out);
  assert_int_equal(process_query_fingerprint_cmd(0, &claddr, NULL, cmd_arr), strlen(OK_REPLY));
  utarray_free(cmd_arr);

  utarray_new(cmd_arr, &ut_str_icd);
  assert_int_not_equal(split_string_array("QUERY_FINGERPRINT 11:22:33:44:55: 12345 >= IP4", CMD_DELIMITER, cmd_arr), -1);
  assert_int_equal(process_query_fingerprint_cmd(0, &claddr, NULL, cmd_arr), strlen(FAIL_REPLY));
  utarray_free(cmd_arr);

  utarray_new(cmd_arr, &ut_str_icd);
  assert_int_not_equal(split_string_array("QUERY_FINGERPRINT 11:22:33:44:55:66 a12345 >= IP4", CMD_DELIMITER, cmd_arr), -1);
  assert_int_equal(process_query_fingerprint_cmd(0, &claddr, NULL, cmd_arr), strlen(FAIL_REPLY));
  utarray_free(cmd_arr);

  utarray_new(cmd_arr, &ut_str_icd);
  assert_int_not_equal(split_string_array("QUERY_FINGERPRINT 11:22:33:44:55:66 12345 >== IP4", CMD_DELIMITER, cmd_arr), -1);
  assert_int_equal(process_query_fingerprint_cmd(0, &claddr, NULL, cmd_arr), strlen(FAIL_REPLY));
  utarray_free(cmd_arr);

  utarray_new(cmd_arr, &ut_str_icd);
  assert_int_not_equal(split_string_array("QUERY_FINGERPRINT 11:22:33:44:55:66 12345 >= 1234567812345678123456781234567812345678123456781234567812345678", CMD_DELIMITER, cmd_arr), -1);
  assert_int_equal(process_query_fingerprint_cmd(0, &claddr, NULL, cmd_arr), strlen(FAIL_REPLY));
  utarray_free(cmd_arr);
}

static void test_process_register_ticket_cmd(void **state)
{
  (void) state; /* unused */

  uint8_t addr[ETH_ALEN] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
  UT_array *cmd_arr;
  struct client_address claddr;

  utarray_new(cmd_arr, &ut_str_icd);
  assert_int_not_equal(split_string_array("REGISTER_TICKET 11:22:33:44:55:66 test 23", CMD_DELIMITER, cmd_arr), -1);
  expect_memory(__wrap_register_ticket_cmd, mac_addr, addr, ETH_ALEN);
  expect_string(__wrap_register_ticket_cmd, label, "test");
  expect_value(__wrap_register_ticket_cmd, vlanid, 23);
  assert_int_equal(process_register_ticket_cmd(0, &claddr, NULL, cmd_arr), strlen(OK_REPLY));
  utarray_free(cmd_arr);

  utarray_new(cmd_arr, &ut_str_icd);
  assert_int_not_equal(split_string_array("REGISTER_TICKET 11:22:33:44:55: test 23", CMD_DELIMITER, cmd_arr), -1);
  assert_int_equal(process_register_ticket_cmd(0, &claddr, NULL, cmd_arr), strlen(FAIL_REPLY));
  utarray_free(cmd_arr);

  utarray_new(cmd_arr, &ut_str_icd);
  assert_int_not_equal(split_string_array("REGISTER_TICKET 11:22:33:44:55:66 23", CMD_DELIMITER, cmd_arr), -1);
  assert_int_equal(process_register_ticket_cmd(0, &claddr, NULL, cmd_arr), strlen(FAIL_REPLY));
  utarray_free(cmd_arr);

  utarray_new(cmd_arr, &ut_str_icd);
  assert_int_not_equal(split_string_array("REGISTER_TICKET 11:22:33:44:55:66 test 23f", CMD_DELIMITER, cmd_arr), -1);
  assert_int_equal(process_register_ticket_cmd(0, &claddr, NULL, cmd_arr), strlen(FAIL_REPLY));
  utarray_free(cmd_arr);
}

static void test_process_clear_psk_cmd(void **state)
{
  (void) state; /* unused */

  uint8_t addr[ETH_ALEN] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
  UT_array *cmd_arr;
  struct client_address claddr;

  utarray_new(cmd_arr, &ut_str_icd);
  assert_int_not_equal(split_string_array("CLEAR_PSK 11:22:33:44:55:66", CMD_DELIMITER, cmd_arr), -1);
  expect_memory(__wrap_clear_psk_cmd, mac_addr, addr, ETH_ALEN);
  assert_int_equal(process_clear_psk_cmd(0, &claddr, NULL, cmd_arr), strlen(OK_REPLY));
  utarray_free(cmd_arr);

  utarray_new(cmd_arr, &ut_str_icd);
  assert_int_not_equal(split_string_array("CLEAR_PSK 11:22:33:44:55:", CMD_DELIMITER, cmd_arr), -1);
  assert_int_equal(process_clear_psk_cmd(0, &claddr, NULL, cmd_arr), strlen(FAIL_REPLY));
  utarray_free(cmd_arr);

}

int main(int argc, char *argv[])
{  
  log_set_quiet(false);

  const struct CMUnitTest tests[] = {
    cmocka_unit_test(test_process_domain_buffer),
    cmocka_unit_test(test_process_accept_mac_cmd),
    cmocka_unit_test(test_process_deny_mac_cmd),
    cmocka_unit_test(test_process_add_nat_cmd),
    cmocka_unit_test(test_process_remove_nat_cmd),
    cmocka_unit_test(test_process_assign_psk_cmd),
    cmocka_unit_test(test_process_get_map_cmd),
    cmocka_unit_test(test_process_get_all_cmd),
    cmocka_unit_test(test_process_set_ip_cmd),
    cmocka_unit_test(test_process_add_bridge_cmd),
    cmocka_unit_test(test_process_remove_bridge_cmd),
    cmocka_unit_test(test_process_set_fingerprint_cmd),
    cmocka_unit_test(test_process_query_fingerprint_cmd),
    cmocka_unit_test(test_process_register_ticket_cmd),
    cmocka_unit_test(test_process_clear_psk_cmd)
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}

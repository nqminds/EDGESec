/****************************************************************************
 * Copyright (C) 2021 by NQMCyber Ltd                                       *
 *                                                                          *
 * This file is part of EDGESec.                                            *
 *                                                                          *
 *   EDGESec is free software: you can redistribute it and/or modify it     *
 *   under the terms of the GNU Lesser General Public License as published  *
 *   by the Free Software Foundation, either version 3 of the License, or   *
 *   (at your option) any later version.                                    *
 *                                                                          *
 *   EDGESec is distributed in the hope that it will be useful,             *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 *   GNU Lesser General Public License for more details.                    *
 *                                                                          *
 *   You should have received a copy of the GNU Lesser General Public       *
 *   License along with EDGESec. If not, see <http://www.gnu.org/licenses/>.*
 ****************************************************************************/

/**
 * @file network_commands.c 
 * @author Alexandru Mereacre 
 * @brief File containing the implementation of the network commands.
 */
#include <libgen.h>

#include "mac_mapper.h"
#include "supervisor.h"
#include "sqlite_fingerprint_writer.h"
#include "sqlite_macconn_writer.h"
#include "network_commands.h"

#include "../crypt/crypt_service.h"
#include "../capture/capture_service.h"
#include "../utils/allocs.h"
#include "../utils/os.h"
#include "../utils/log.h"
#include "../utils/eloop.h"
#include "../utils/iptables.h"

#define ANALYSER_FILTER_FORMAT "\"ether dst " MACSTR " or ether src " MACSTR"\""

static const UT_icd fingerprint_icd = {sizeof(struct fingerprint_row), NULL, NULL, NULL};

bool save_mac_mapper(struct supervisor_context *context, struct mac_conn conn)
{
  struct crypt_pair pair;

  if (!put_mac_mapper(&context->mac_mapper, conn)) {
    log_trace("put_mac_mapper fail");
    return false;
  }

  if (save_sqlite_macconn_entry(context->macconn_db, &conn) < 0) {
    log_trace("upsert_sqlite_macconn_entry fail");
    return false;
  }

  pair.key = conn.info.id;
  pair.value = conn.info.pass;
  pair.value_size = conn.info.pass_len;

  if (put_crypt_pair(context->crypt_ctx, &pair) < 0) {
    log_trace("put_crypt_pair fail");
    return false;
  }

  return true;
}

void free_ticket(struct supervisor_context *context)
{
  struct auth_ticket *ticket = context->ticket;
  if (ticket != NULL) {
    log_trace("Freeing ticket");
    os_free(ticket);
    context->ticket = NULL;
  }
}

void eloop_ticket_timeout_handler(void *eloop_ctx, void *user_ctx)
{
  struct supervisor_context *context = (struct supervisor_context *) user_ctx;
  log_trace("Auth ticket timeout, removing ticket");
  free_ticket(context);
}

int run_analyser(struct capture_conf *config, pid_t *child_pid)
{
  int ret;
  char **process_argv = capture_config2opt(config);
  char *proc_name;
  if (process_argv == NULL) {
    log_trace("capture_config2opt fail");
    return -1;
  }

  ret = run_process(process_argv, child_pid);

  if ((proc_name = os_strdup(basename(process_argv[0]))) == NULL) {
    log_err("os_malloc");
    capture_freeopt(process_argv);
    return -1;
  }

  if (is_proc_running(proc_name) <= 0) {
    log_trace("is_proc_running fail");
    os_free(proc_name);
    capture_freeopt(process_argv);
    return -1;
  }

  log_trace("Found capture process running with pid=%d", *child_pid);
  os_free(proc_name);
  capture_freeopt(process_argv);
  return ret;
}

int schedule_analyser(struct supervisor_context *context, int vlanid)
{
  pid_t child_pid;
  struct capture_conf config;
  struct vlan_conn vlan_conn;

  if (get_vlan_mapper(&context->vlan_mapper, vlanid, &vlan_conn) <= 0) {
    log_trace("ifname not found for vlanid=%d", vlanid);
    return -1;
  }

  if (!vlan_conn.analyser_pid) {
    os_memcpy(&config, &context->capture_config, sizeof(config));

    log_trace("Starting analyser on if=%s", vlan_conn.ifname);
    os_memcpy(config.capture_interface, vlan_conn.ifname, IFNAMSIZ);

    if (run_analyser(&config, &child_pid) != 0) {
      log_trace("run_analyser fail");
      return -1;
    }

    vlan_conn.analyser_pid = child_pid;
    if (!put_vlan_mapper(&context->vlan_mapper, &vlan_conn)) {
      log_trace("put_vlan_mapper fail");
      return -1;
    }
  }

  return 0;
}

void configure_mac_info(struct mac_conn_info *info, bool allow_connection,
                        int vlanid, ssize_t pass_len, uint8_t *pass, char *label)
{
  info->allow_connection = allow_connection;
  info->vlanid = vlanid;
  info->pass_len = pass_len;
  os_memcpy(info->pass, pass, info->pass_len);
  if (label != NULL) {
    os_memcpy(info->label, label, MAX_DEVICE_LABEL_SIZE);
  }
}

struct mac_conn_info get_mac_conn_cmd(uint8_t mac_addr[], void *mac_conn_arg)
{
  struct supervisor_context *context = (struct supervisor_context *) mac_conn_arg;
  struct mac_conn conn;
  struct mac_conn_info info;

  init_default_mac_info(&info, context->default_open_vlanid, context->allow_all_nat);

  log_trace("REQUESTED vland id for mac=" MACSTR, MAC2STR(mac_addr));

  if (mac_addr == NULL) {
    log_trace("mac_addr is NULL");
    info.vlanid = -1;
    return info;
  }

  if (context == NULL) {
    log_trace("context is NULL");
    info.vlanid = -1;
    return info;
  }

  int find_mac = get_mac_mapper(&context->mac_mapper, mac_addr, &info);

  if (context->allow_all_connections) {
    configure_mac_info(&info, true, context->default_open_vlanid,
                       context->wpa_passphrase_len, context->wpa_passphrase, NULL);

    if (context->exec_capture) {
      if (schedule_analyser(context, info.vlanid) < 0) {
        log_trace("execute_capture fail");
        log_trace("REJECTING mac=" MACSTR, MAC2STR(mac_addr));
        info.vlanid = -1;
        return info;
      }
    }

    if (os_get_timestamp(&info.join_timestamp) < 0) {
      log_trace("os_get_timestamp fail");
        info.vlanid = -1;
        return info;
    }

    log_trace("ALLOWING mac=" MACSTR " on default vlanid=%d", MAC2STR(mac_addr), info.vlanid);
    os_memcpy(conn.mac_addr, mac_addr, ETH_ALEN);
    conn.info = info;
    if (!save_mac_mapper(context, conn)) {
      log_trace("save_mac_mapper fail");
      info.vlanid = -1;
    }

    return info;
  } else {
    if (find_mac == 1 && info.allow_connection && info.pass_len) {
      if (context->exec_capture) {
        if (schedule_analyser(context, info.vlanid) < 0) {
          log_trace("execute_capture fail");
          log_trace("REJECTING mac=" MACSTR, MAC2STR(mac_addr));
          info.vlanid = -1;
          return info;
        }
      }

      if (os_get_timestamp(&info.join_timestamp) < 0) {
        log_trace("os_get_timestamp fail");
          info.vlanid = -1;
          return info;
      }

      log_trace("ALLOWING mac=" MACSTR " on vlanid=%d", MAC2STR(mac_addr), info.vlanid);
      os_memcpy(conn.mac_addr, mac_addr, ETH_ALEN);
      conn.info = info;
      if (!save_mac_mapper(context, conn)) {
        log_trace("save_mac_mapper fail");
        info.vlanid = -1;
      }

      return info;
    } else if (find_mac == -1) {
      log_trace("get_mac_mapper fail");
    } else if (find_mac == 0 || (find_mac == 1 && info.allow_connection && !info.pass_len)) {
      log_trace("mac=" MACSTR " not assigned, checking for the active tickets", MAC2STR(mac_addr));
      info.allow_connection = true;

      if (context->ticket != NULL) {
        // Use ticket
        log_trace("Assigning auth ticket");
        info.vlanid = context->ticket->vlanid;
        info.pass_len = context->ticket->passphrase_len;
        os_memcpy(info.pass, context->ticket->passphrase, info.pass_len);
        os_memcpy(info.label, context->ticket->device_label, MAX_DEVICE_LABEL_SIZE);
        free_ticket(context);
      } else {
        // Assign to default VLAN ID
        log_trace("Assigning default connection");
        info.vlanid = context->default_open_vlanid;
        info.pass_len = context->wpa_passphrase_len;
        os_memcpy(info.pass, context->wpa_passphrase, info.pass_len);
      }

      if (context->exec_capture) {
        if (schedule_analyser(context, info.vlanid) < 0) {
          log_trace("execute_capture fail");
          log_trace("REJECTING mac=" MACSTR, MAC2STR(mac_addr));
          info.vlanid = -1;
          return info;
        }
      }

      if (os_get_timestamp(&info.join_timestamp) < 0) {
        log_trace("os_get_timestamp fail");
          info.vlanid = -1;
          return info;
      }

      log_trace("ALLOWING mac=" MACSTR " on vlanid=%d", MAC2STR(mac_addr), info.vlanid);
      os_memcpy(conn.mac_addr, mac_addr, ETH_ALEN);
      conn.info = info;
      if (!save_mac_mapper(context, conn)) {
        log_trace("save_mac_mapper fail");
        info.vlanid = -1;
      }

      return info;
    }
  }
  
  log_trace("REJECTING mac=" MACSTR, MAC2STR(mac_addr));
  info.vlanid = -1;
  return info;
}

int accept_mac_cmd(struct supervisor_context *context, uint8_t *mac_addr, int vlanid)
{
  struct mac_conn conn;
  struct mac_conn_info info;
  struct vlan_conn vlan_conn;
  init_default_mac_info(&info, context->default_open_vlanid, context->allow_all_nat);

  log_trace("ACCEPT_MAC mac=" MACSTR " with vlanid=%d", MAC2STR(mac_addr), vlanid);

  if (get_mac_mapper(&context->mac_mapper, mac_addr, &info) < 0) {
    log_trace("get_mac_mapper fail");
    return -1;
  }

  os_memcpy(conn.mac_addr, mac_addr, ETH_ALEN);
  info.allow_connection = true;
  info.vlanid = vlanid;
  os_memcpy(&conn.info, &info, sizeof(struct mac_conn_info));

  if (get_vlan_mapper(&context->vlan_mapper, conn.info.vlanid, &vlan_conn) <= 0) {
    log_trace("get_vlan_mapper fail");
    return -1;
  }

  os_memcpy(conn.info.ifname, vlan_conn.ifname, IFNAMSIZ);
  if (!save_mac_mapper(context, conn)) {
    log_trace("save_mac_mapper fail");
    return -1;
  }

  return 0;
}

int deny_mac_cmd(struct supervisor_context *context, uint8_t *mac_addr)
{
  struct mac_conn conn;
  struct mac_conn_info info;
  init_default_mac_info(&info, context->default_open_vlanid, context->allow_all_nat);

  log_trace("DENY_MAC mac=" MACSTR, MAC2STR(mac_addr));
  get_mac_mapper(&context->mac_mapper, mac_addr, &info);
  os_memcpy(conn.mac_addr, mac_addr, ETH_ALEN);
  info.allow_connection = false;
  os_memcpy(&conn.info, &info, sizeof(struct mac_conn_info));
  if (!save_mac_mapper(context, conn)) {
    log_trace("save_mac_mapper fail");
    return -1;
  }

  return 0;
}

int add_nat_cmd(struct supervisor_context *context, uint8_t *mac_addr)
{
  char ifname[IFNAMSIZ];
  struct mac_conn conn;
  struct mac_conn_info info;
  init_default_mac_info(&info, context->default_open_vlanid, context->allow_all_nat);

  log_trace("ADD_NAT mac=" MACSTR, MAC2STR(mac_addr));
  if (get_mac_mapper(&context->mac_mapper, mac_addr, &info) < 0) {
    log_trace("get_mac_mapper fail");
    return -1;
  }

  os_memcpy(conn.mac_addr, mac_addr, ETH_ALEN);
  info.nat = true;
  os_memcpy(&conn.info, &info, sizeof(struct mac_conn_info));

  if (validate_ipv4_string(info.ip_addr)) {
    if (!get_ifname_from_ip(&context->if_mapper, context->config_ifinfo_array, info.ip_addr, ifname)) {
      log_trace("get_ifname_from_ip fail");
      return -1;
    }
    if (!iptables_add_nat(context->iptables_ctx, info.ip_addr, ifname, context->nat_interface)) {
      log_trace("iptables_add_nat fail");
      return -1;
    }
  }

  if (!save_mac_mapper(context, conn)) {
    log_trace("save_mac_mapper fail");
    return -1;
  }

  return 0;
}

int remove_nat_cmd(struct supervisor_context *context, uint8_t *mac_addr)
{
  char ifname[IFNAMSIZ];
  struct mac_conn conn;
  struct mac_conn_info info;
  init_default_mac_info(&info, context->default_open_vlanid, context->allow_all_nat);

  log_trace("REMOVE_NAT mac=" MACSTR, MAC2STR(mac_addr));
  if (get_mac_mapper(&context->mac_mapper, mac_addr, &info) < 0) {
    log_trace("get_mac_mapper fail");
    return -1;
  }

  os_memcpy(conn.mac_addr, mac_addr, ETH_ALEN);
  info.nat = false;
  os_memcpy(&conn.info, &info, sizeof(struct mac_conn_info));

  if (validate_ipv4_string(info.ip_addr)) {
    if (!get_ifname_from_ip(&context->if_mapper, context->config_ifinfo_array, info.ip_addr, ifname)) {
      log_trace("get_ifname_from_ip fail");
      return -1;
    }
    if (!iptables_delete_nat(context->iptables_ctx, info.ip_addr, ifname, context->nat_interface)) {
      log_trace("iptables_delete_nat fail");
      return -1;
    }
  }

  if (!save_mac_mapper(context, conn)) {
    log_trace("save_mac_mapper fail");
    return -1;
  }

  return 0;
}

int assign_psk_cmd(struct supervisor_context *context, uint8_t *mac_addr,
  char *pass, int pass_len)
{
  struct mac_conn conn;
  struct mac_conn_info info;
  init_default_mac_info(&info, context->default_open_vlanid, context->allow_all_nat);

  log_trace("ASSIGN_PSK mac=" MACSTR ", pass_len=%d", MAC2STR(mac_addr), pass_len);

  get_mac_mapper(&context->mac_mapper, mac_addr, &info);
  os_memcpy(info.pass, pass, pass_len);
  info.pass_len = pass_len;
  os_memcpy(conn.mac_addr, mac_addr, ETH_ALEN);
  os_memcpy(&conn.info, &info, sizeof(struct mac_conn_info));

  if (!save_mac_mapper(context, conn)) {
    log_trace("save_mac_mapper fail");
    return -1;
  }

  return 0;
}

int set_ip_cmd(struct supervisor_context *context, uint8_t *mac_addr,
  char *ip_addr, bool add)
{
  UT_array *mac_list_arr;
  uint8_t *p = NULL;
  char ifname[IFNAMSIZ];
  struct mac_conn conn;
  struct mac_conn_info right_info, info;
  int ret;

  init_default_mac_info(&info, context->default_open_vlanid, context->allow_all_nat);

  if (!get_ifname_from_ip(&context->if_mapper, context->config_ifinfo_array, ip_addr, ifname)) {
    log_trace("get_ifname_from_ip fail");
    return -1;
  }

  ret = get_mac_mapper(&context->mac_mapper, mac_addr, &info);
  if (ret < 0) {
    log_trace("get_mac_mapper fail");
    return -1;
  }

  os_memcpy(info.ifname, ifname, IFNAMSIZ);
  os_memcpy(conn.mac_addr, mac_addr, ETH_ALEN);
  os_memcpy(&conn.info, &info, sizeof(struct mac_conn_info));
          
  if (add) os_strlcpy(conn.info.ip_addr, ip_addr, IP_LEN);
  else os_memset(conn.info.ip_addr, 0x0, IP_LEN);

  log_trace("SET_IP type=%d mac=" MACSTR " ip=%s if=%s", add, MAC2STR(mac_addr), ip_addr, ifname);
  if (!save_mac_mapper(context, conn)) {
    log_trace("save_mac_mapper fail");
    return -1;
  }

  // Change the NAT iptables rules
  if (add && info.nat) {
    log_trace("Adding NAT rule");
    if (!iptables_add_nat(context->iptables_ctx, ip_addr, ifname, context->nat_interface)) {
      log_trace("iptables_add_nat fail");
      return -1;
    }
  } else if (!add && info.nat){
    log_trace("Deleting NAT rule");
    if (!iptables_delete_nat(context->iptables_ctx, ip_addr, ifname, context->nat_interface)) {
      log_trace("iptables_delete_nat fail");
      return -1;
    }
  }

  // Change the bridge iptables rules
  // Get the list of all dst MACs to update the iptables
  if(get_src_mac_list(context->bridge_list, conn.mac_addr, &mac_list_arr) >= 0) {
    while(p = (uint8_t *) utarray_next(mac_list_arr, p)) {
      if(get_mac_mapper(&context->mac_mapper, p, &right_info) == 1) {
        if (validate_ipv4_string(right_info.ip_addr)) {
          if (add) {
            log_trace("Adding iptable rule for sip=%s sif=%s dip=%s dif=%s", conn.info.ip_addr, conn.info.ifname, right_info.ip_addr, right_info.ifname);
            if (!iptables_add_bridge(context->iptables_ctx, conn.info.ip_addr, conn.info.ifname, right_info.ip_addr, right_info.ifname)) {
              log_trace("iptables_add_bridge fail");
              utarray_free(mac_list_arr);
              return -1;
            }
          } else {
            log_trace("Removing iptable rule for sip=%s sif=%s dip=%s dif=%s", info.ip_addr, info.ifname, right_info.ip_addr, right_info.ifname);
            if (!iptables_delete_bridge(context->iptables_ctx, info.ip_addr, info.ifname, right_info.ip_addr, right_info.ifname)) {
              log_trace("remove_bridge_rules fail");
              utarray_free(mac_list_arr);
              return -1;
            }
          }
        }
      }
    }
    utarray_free(mac_list_arr);
  } else return -1;

  return 0;
}

int add_bridge_cmd(struct supervisor_context *context, uint8_t *left_mac_addr, uint8_t *right_mac_addr)
{
  struct mac_conn_info left_info, right_info;

  if (add_bridge_mac(context->bridge_list, left_mac_addr, right_mac_addr) >= 0) {
    log_trace("ADD_BRIDGE left_mac=" MACSTR " right_mac=" MACSTR, MAC2STR(left_mac_addr), MAC2STR(right_mac_addr));
    if (get_mac_mapper(&context->mac_mapper, left_mac_addr, &left_info) == 1 &&
        get_mac_mapper(&context->mac_mapper, right_mac_addr, &right_info) == 1
    ) {
      if (validate_ipv4_string(left_info.ip_addr) && validate_ipv4_string(right_info.ip_addr)) {
        log_trace("Adding iptable rule for sip=%s sif=%s dip=%s dif=%s", left_info.ip_addr, left_info.ifname, right_info.ip_addr, right_info.ifname);
        if (!iptables_add_bridge(context->iptables_ctx, left_info.ip_addr, left_info.ifname, right_info.ip_addr, right_info.ifname)) {
          log_trace("iptables_add_bridge fail");
          return -1;
        }
      }
    }
  } else {
    log_trace("add_bridge_mac fail");
    return -1;
  }

  return 0;
}

int remove_bridge_cmd(struct supervisor_context *context, uint8_t *left_mac_addr, uint8_t *right_mac_addr)
{
  struct mac_conn_info left_info, right_info;

  if (remove_bridge_mac(context->bridge_list, left_mac_addr, right_mac_addr) >= 0) {
    log_trace("REMOVE_BRIDGE left_mac=" MACSTR " right_mac=" MACSTR, MAC2STR(left_mac_addr), MAC2STR(right_mac_addr));
    if (get_mac_mapper(&context->mac_mapper, left_mac_addr, &left_info) == 1 &&
        get_mac_mapper(&context->mac_mapper, right_mac_addr, &right_info) == 1
    ) {
      if (validate_ipv4_string(left_info.ip_addr) && validate_ipv4_string(right_info.ip_addr)) {
        log_trace("Removing iptable rule for sip=%s sif=%s dip=%s dif=%s", left_info.ip_addr, left_info.ifname, right_info.ip_addr, right_info.ifname);
        if (!iptables_delete_bridge(context->iptables_ctx, left_info.ip_addr, left_info.ifname, right_info.ip_addr, right_info.ifname)) {
          log_trace("iptables_delete_bridge fail");
          return -1;
        }
      }
    }
  } else {
    log_trace("remove_bridge_mac fail");
    return -1;
  }

  return 0;
}

int set_fingerprint_cmd(struct supervisor_context *context, char *src_mac_addr,
                        char *dst_mac_addr, char *protocol, char *fingerprint,
                        uint64_t timestamp, char *query)
{
  struct fingerprint_row row_src = {.mac = src_mac_addr, .protocol = protocol,
                                .fingerprint = fingerprint, .timestamp = timestamp,
                                .query = query};

  struct fingerprint_row row_dst = {.mac = dst_mac_addr, .protocol = protocol,
                                .fingerprint = fingerprint, .timestamp = timestamp,
                                .query = query};

  log_trace("SET_FINGERPRINT for src_mac=%s, dst_mac=%s, protocol=%s and timestamp=%"PRIu64, src_mac_addr,
            dst_mac_addr, protocol, timestamp);
  if (save_sqlite_fingerprint_row(context->fingeprint_db, &row_src) < 0) {
    log_trace("save_sqlite_fingerprint_entry fail");
    return -1;
  }

  if (save_sqlite_fingerprint_row(context->fingeprint_db, &row_dst) < 0) {
    log_trace("save_sqlite_fingerprint_entry fail");
    return -1;
  }

  return 0;
}

void free_row_array(char *row_array[])
{
  int idx = 0;
  while(row_array[idx] != NULL) {
    os_free(row_array[idx]);
    idx ++;
  }
}

ssize_t query_fingerprint_cmd(struct supervisor_context *context, char *mac_addr, uint64_t timestamp,
                        char *op, char *protocol, char **out)
{
  UT_array *rows = NULL;
  ssize_t out_size = 0;
  struct fingerprint_row *p = NULL;
  char *row_array[6] = {}, *row;
  char *proto = (strcmp(protocol, "all") == 0) ? NULL : protocol;

  // Create the connections list
  utarray_new(rows, &fingerprint_icd);

  if (rows == NULL) {
    log_trace("utarray_new fail");
    return -1;
  }

  *out = NULL;
  log_trace("QUERY_FINGERPRINT for mac=%s, protocol=%s op=\"%s\" and timestamp=%"PRIu64, mac_addr,
            protocol, op, timestamp);
  if (get_sqlite_fingerprint_rows(context->fingeprint_db, mac_addr,
                                     timestamp, op, proto, rows) < 0)
  {
    log_trace("get_sqlite_fingerprint_rows fail");
    free_sqlite_fingerprint_rows(rows);
    return -1;
  }

  while(p = (struct fingerprint_row *) utarray_next(rows, p)) {
    os_memset(row_array, 0, 6);

    if (p->mac != NULL) {
      row_array[0] = os_malloc(strlen(p->mac) + 2);
      if (row_array[0] == NULL) {
        log_err("os_malloc");
        free_sqlite_fingerprint_rows(rows);
        if (*out != NULL) os_free(*out);
        return -1;
      }
      sprintf(row_array[0], "%s,", p->mac);
    } else {
      row_array[0] = os_malloc(2);
      if (row_array[0] == NULL) {
        log_err("os_malloc");
        free_sqlite_fingerprint_rows(rows);
        if (*out != NULL) os_free(*out);
        return -1;
      }

      sprintf(row_array[0], ",");
    }

    if (p->protocol != NULL) {
      row_array[1] = os_malloc(strlen(p->protocol) + 2);
      if (row_array[1] == NULL) {
        log_err("os_malloc");
        free_row_array(row_array);
        free_sqlite_fingerprint_rows(rows);
        if (*out != NULL) os_free(*out);
        return -1;
      }

      sprintf(row_array[1], "%s,", p->protocol);
    } else {
      row_array[1] = os_malloc(2);
      if (row_array[1] == NULL) {
        log_err("os_malloc");
        free_row_array(row_array);
        free_sqlite_fingerprint_rows(rows);
        if (*out != NULL) os_free(*out);
        return -1;
      }
      sprintf(row_array[1], ",");
    }

    if (p->fingerprint != NULL) {
      row_array[2] = os_malloc(strlen(p->fingerprint) + 2);
      if (row_array[2] == NULL) {
        log_err("os_malloc");
        free_row_array(row_array);
        free_sqlite_fingerprint_rows(rows);
        if (*out != NULL) os_free(*out);
        return -1;
      }

      sprintf(row_array[2], "%s,", p->fingerprint);
    } else {
      row_array[2] = os_malloc(2);
      if (row_array[2] == NULL) {
        log_err("os_malloc");
        free_row_array(row_array);
        free_sqlite_fingerprint_rows(rows);
        if (*out != NULL) os_free(*out);
        return -1;
      }

      sprintf(row_array[2], ",");
    }

    row_array[3] = os_malloc(MAX_UINT64_DIGITS + 2);
    if (row_array[3] == NULL) {
      log_err("os_malloc");
      free_row_array(row_array);
      free_sqlite_fingerprint_rows(rows);
      if (*out != NULL) os_free(*out);
      return -1;
    }
    sprintf(row_array[3], "%"PRIu64",", p->timestamp);

    if (p->query != NULL) {
      row_array[4] = os_malloc(strlen(p->query) + 2);
      if (row_array[4] == NULL) {
        log_err("os_malloc");
        free_row_array(row_array);
        free_sqlite_fingerprint_rows(rows);
        if (*out != NULL) os_free(*out);
        return -1;
      }
      sprintf(row_array[4], "%s\n", p->query);
    } else {
      row_array[4] = os_malloc(2);
      if (row_array[4] == NULL) {
        log_err("os_malloc");
        free_row_array(row_array);
        free_sqlite_fingerprint_rows(rows);
        if (*out != NULL) os_free(*out);
        return -1;
      }
      sprintf(row_array[4], "\n");
    }

    row = os_zalloc(strlen(row_array[0]) + strlen(row_array[1]) + strlen(row_array[2]) +
                    strlen(row_array[3]) + strlen(row_array[4]) + 1);

    if (row == NULL) {
      log_err("os_zalloc");
      free_row_array(row_array);
      free_sqlite_fingerprint_rows(rows);
      return -1;
    }

    for (int idx = 0; idx < 5; idx ++) {
      strcat(row, row_array[idx]);
    }

    free_row_array(row_array);

    if (*out == NULL) {
      out_size = strlen(row) + 1;
      *out = os_zalloc(out_size);
    } else {
      out_size += strlen(row);
      *out = os_realloc(*out, out_size);
    }

    if (*out == NULL) {
      log_trace("os_zalloc/os_realloc");
      os_free(row);
      free_sqlite_fingerprint_rows(rows);
      return -1;
    }

    strcat(*out, row);
    os_free(row);
  }

  free_sqlite_fingerprint_rows(rows);
  return out_size;
}

uint8_t* register_ticket_cmd(struct supervisor_context *context, uint8_t *mac_addr, char *label,
                        int vlanid)
{
  log_trace("REGISTER_TICKET for mac=" MACSTR ", label=%s and vlanid=%d", MAC2STR(mac_addr), label, vlanid);

  if (context->ticket != NULL) {
    log_trace("Auth ticket is still active");
    return NULL;
  }

  context->ticket = os_zalloc(sizeof(struct auth_ticket));

  if (context->ticket == NULL) {
    log_err("os_malloc");
    return NULL;
  }

  os_memcpy(context->ticket->issuer_mac_addr, mac_addr, ETH_ALEN);
  strcpy(context->ticket->device_label, label);
  context->ticket->vlanid = vlanid;
  context->ticket->passphrase_len = TICKET_PASSPHRASE_SIZE;

  if (os_get_random_number_s(context->ticket->passphrase, context->ticket->passphrase_len) < 0) {
    log_trace("os_get_random_number_s fail");
    os_free(context->ticket);
    return NULL;
  }

  if (eloop_register_timeout(TICKET_TIMEOUT, 0, eloop_ticket_timeout_handler, NULL, (void *)context) < 0) {
    log_trace("eloop_register_timeout fail");
    os_free(context->ticket);
    return NULL;
  }

  return context->ticket->passphrase;
}

int clear_psk_cmd(struct supervisor_context *context, uint8_t *mac_addr)
{  
  struct mac_conn conn;
  struct mac_conn_info info;

  init_default_mac_info(&info, context->default_open_vlanid, context->allow_all_nat);

  log_trace("CLEAR_PSK for mac=" MACSTR, MAC2STR(mac_addr));

  get_mac_mapper(&context->mac_mapper, mac_addr, &info);
  os_memset(info.pass, 0, AP_SECRET_LEN);
  info.pass_len = 0;
  os_memcpy(conn.mac_addr, mac_addr, ETH_ALEN);
  os_memcpy(&conn.info, &info, sizeof(struct mac_conn_info));

  if (!save_mac_mapper(context, conn)) {
    log_trace("save_mac_mapper fail");
    return -1;
  }

  return 0;
}
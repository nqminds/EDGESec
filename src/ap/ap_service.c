/****************************************************************************
 * Copyright (C) 2020 by NQMCyber Ltd                                       *
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
 * @file hostapd_service.c 
 * @author Alexandru Mereacre 
 * @brief File containing the implementation of the hostapd service.
 * 
 * Defines the functions to start and stop the acces point service (AP). It also 
 * defines auxiliary commands to manage the acces control list for stations
 * connected to the AP.
 */
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>

#include "ap_config.h"
#include "ap_service.h"
#include "hostapd.h"

#include "../supervisor/supervisor_config.h"
#include "../radius/radius_server.h"
#include "../utils/allocs.h"
#include "../utils/os.h"
#include "../utils/eloop.h"
#include "../utils/iface.h"
#include "../utils/log.h"
#include "../utils/domain.h"

#define ATTACH_AP_COMMAND         "ATTACH"

#define AP_STA_DISCONNECTED       "AP-STA-DISCONNECTED"
#define AP_STA_CONNECTED          "AP-STA-CONNECTED"

typedef void (*ap_service_fn)(struct supervisor_context *context, uint8_t mac_addr[], enum AP_CONNECTION_STATUS status);

int ping_ap_command(struct apconf *hconf)
{
  char *reply = NULL;

  if (writeread_domain_data_str(hconf->ctrl_interface_path, PING_AP_COMMAND, &reply) < 0) {
    log_trace("writeread_domain_data_str fail");
    return -1;
  }

  if (strcmp(reply, PING_AP_COMMAND_REPLY) != 0) {
    log_trace(PING_AP_COMMAND_REPLY" reply doesn't match %s", reply);
    os_free(reply);
    return -1;
  }

  os_free(reply);
  return 0;
}

int denyacl_ap_command(struct apconf *hconf, char *cmd, char *mac_addr)
{
  char *buffer;
  char *reply = NULL;

  if (mac_addr == NULL) {
    log_trace("mac_addr is NULL");
    return -1;
  }

  if ((buffer = os_zalloc(strlen(cmd) + strlen(mac_addr) + 1)) == NULL) {
    log_err("os_zalloc");
    return -1;
  }

  sprintf(buffer, "%s %s", cmd, mac_addr);
  if (writeread_domain_data_str(hconf->ctrl_interface_path, buffer, &reply) < 0) {
    log_trace("writeread_domain_data_str fail");
    return -1;
  }

  if (strcmp(reply, GENERIC_AP_COMMAND_OK_REPLY) != 0) {
    log_trace(GENERIC_AP_COMMAND_OK_REPLY" reply doesn't match %s", reply);
    os_free(reply);
    return -1;
  }

  os_free(reply);
  return 0;
}

int denyacl_add_ap_command(struct apconf *hconf, char *mac_addr)
{
  return denyacl_ap_command(hconf, DENYACL_ADD_COMMAND, mac_addr);
}

int denyacl_del_ap_command(struct apconf *hconf, char *mac_addr)
{
  return denyacl_ap_command(hconf, DENYACL_DEL_COMMAND, mac_addr);
}

int disconnect_ap_command(struct apconf *hconf, char *mac_addr)
{
  if (denyacl_add_ap_command(hconf, mac_addr) < 0) {
    log_trace("denyacl_add_ap_command fail");
    return -1;
  }

  if (denyacl_del_ap_command(hconf, mac_addr) < 0) {
    log_trace("denyacl_del_ap_command fail");
    return -1;
  }

  return 0;
}

int check_sta_ap_command(struct apconf *hconf, char *mac_addr)
{
  char *buffer;
  char *reply = NULL;

  if (mac_addr == NULL) {
    log_trace("mac_addr is NULL");
    return -1;
  }

  if ((buffer = os_zalloc(strlen(STA_AP_COMMAND) + strlen(mac_addr) + 1)) == NULL) {
    log_err("os_zalloc");
    return -1;
  }

  sprintf(buffer, STA_AP_COMMAND" %s", mac_addr);
  if (writeread_domain_data_str(hconf->ctrl_interface_path, buffer, &reply) < 0) {
    log_trace("writeread_domain_data_str fail");
    return -1;
  }

  if (strcmp(reply, GENERIC_AP_COMMAND_FAIL_REPLY) == 0) {
    log_trace("no STA registered with mac=%s", mac_addr);
    os_free(reply);
    return -1;
  }

  if (!strlen(reply)) {
    log_trace("no reply for mac=%s", mac_addr);
    os_free(reply);
    return -1;
  }

  os_free(reply);
  return 0;
}

int find_ap_status(char *ap_answer, uint8_t *mac_addr, enum AP_CONNECTION_STATUS *status)
{
  UT_array *str_arr;
  char **ptr = NULL;

  utarray_new(str_arr, &ut_str_icd);

  if (split_string_array(ap_answer, 0x20, str_arr) > 1) {
    ptr = (char**) utarray_next(str_arr, ptr);
    if (ptr != NULL && *ptr != NULL) {
      if (strstr(*ptr, AP_STA_CONNECTED) != NULL) {
        *status = AP_CONNECTED_STATUS;
      } else if (strstr(ap_answer, AP_STA_DISCONNECTED) != NULL) {
        *status = AP_DISCONNECTED_STATUS;
      } else {
        utarray_free(str_arr);
        return -1;
      }

      ptr = (char**) utarray_next(str_arr, ptr);
      if (ptr != NULL && *ptr != NULL) {
        if (hwaddr_aton2(*ptr, mac_addr) != -1) {
          utarray_free(str_arr);
          return 0;
        }
      }
    }
  }

  utarray_free(str_arr);
  return -1;
}

void ap_sock_handler(int sock, void *eloop_ctx, void *sock_ctx)
{
  uint8_t mac_addr[ETH_ALEN];
  enum AP_CONNECTION_STATUS status;
  uint32_t bytes_available;
  char *rec_data, *trimmed;
  struct supervisor_context *context = (struct supervisor_context *) sock_ctx;
  ap_service_fn fn = (ap_service_fn) eloop_ctx;

  if (ioctl(sock, FIONREAD, &bytes_available) == -1) {
    log_err("ioctl");
    return;
  }

  rec_data = os_zalloc(bytes_available + 1);
  if (rec_data == NULL) {
    log_err("os_zalloc");
    return;
  }

  if (read_domain_data_s(sock, rec_data, bytes_available, context->hconfig.ctrl_interface_path, MSG_DONTWAIT) < 0) {
    log_trace("read_domain_data_s fail");
    os_free(rec_data);
    return;
  }

  if ((trimmed = rtrim(rec_data, NULL)) == NULL) {
    log_trace("rtrim fail");
    os_free(rec_data);
    return;
  }

  if (find_ap_status(trimmed, mac_addr, &status) > -1) {
    fn(context, mac_addr, status);
  }

  os_free(rec_data);
}

int register_ap_event(struct supervisor_context *context, void *ap_callback_fn)
{
  ssize_t cmd_len = (ssize_t) STRLEN(ATTACH_AP_COMMAND);

  if ((context->ap_sock = create_domain_client(NULL)) == -1) {
    log_debug("create_domain_client fail");
    return -1;
  }

  if (eloop_register_read_sock(context->ap_sock, ap_sock_handler, ap_callback_fn, (void *)context) ==  -1) {
    log_trace("eloop_register_read_sock fail");
    return -1;
  }

  log_trace("Sending command %s to socket_path=%s", ATTACH_AP_COMMAND, context->hconfig.ctrl_interface_path);
  if (write_domain_data_s(context->ap_sock, ATTACH_AP_COMMAND, cmd_len, context->hconfig.ctrl_interface_path) != cmd_len) {
    log_trace("write_domain_data_s fail");
    return -1;
  }

  return 0;
}

int run_ap(struct supervisor_context *context, bool exec_ap, bool generate_ssid,
           void *ap_callback_fn)
{
  char hostname[OS_HOST_NAME_MAX];
  int res;
  if (!generate_vlan_conf(context->hconfig.vlan_file, context->hconfig.interface)) {
    log_trace("generate_vlan_conf fail");
    return -1;
  }

  if (generate_ssid) {
    if (get_hostname(hostname) < 0) {
      log_debug("get_hostname fail");
      return -1;
    }
    os_strlcpy(context->hconfig.ssid, hostname, AP_NAME_LEN);
    log_info("Regenarating SSID=%s", context->hconfig.ssid);
  }

  if (!generate_hostapd_conf(&context->hconfig, &context->rconfig)) {
    unlink(context->hconfig.vlan_file);
    log_trace("generate_hostapd_conf fail");
    return -1;
  }

  if (exec_ap) {
    res = run_ap_process(&context->hconfig);
  } else {
    res = signal_ap_process(&context->hconfig);
  }

  if (!res && ping_ap_command(&context->hconfig) < 0) {
    log_trace("ping_ap_command fail");
    return -1;
  }

  if (register_ap_event(context, ap_callback_fn) < 0) {
    log_trace("register_ap_event fail");
    return -1;
  }

  return res;
}

bool close_ap(struct supervisor_context *context)
{
  if (context->ap_sock != -1) {
    close(context->ap_sock);
    context->ap_sock = -1;
  }

  return kill_ap_process();
}

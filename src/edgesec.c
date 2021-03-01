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
 * @file edgesec.c 
 * @author Alexandru Mereacre 
 * @brief File containing the edgesec tool implementations.
 */

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

#include "version.h"
#include "utils/log.h"
#include "utils/os.h"
#include "utils/minIni.h"
#include "utils/utarray.h"
#include "utils/os.h"
#include "utils/if.h"
#include "dhcp/dhcp_config.h"
#include "engine.h"
#include "if_service.h"

#define OPT_STRING    ":c:dvh"
#define USAGE_STRING  "\t%s [-c filename] [-d] [-h] [-v]\n"

static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;

static const UT_icd config_ifinfo_icd = {sizeof(config_ifinfo_t), NULL, NULL, NULL};
static const UT_icd mac_conn_icd = {sizeof(struct mac_conn), NULL, NULL, NULL};
static const UT_icd config_dhcpinfo_icd = {sizeof(config_dhcpinfo_t), NULL, NULL, NULL};

static __thread char version_buf[10];

static void lock_fn(bool lock)
{
  int res;

  if (lock) {
    res = pthread_mutex_lock(&mtx);
    if (res != 0) {
      log_err_exp("pthread_mutex_lock\n");
    }
  } else {
    res = pthread_mutex_unlock(&mtx);
    if (res != 0) {
      log_err_exp("pthread_mutex_unlock\n");
    }
  }
}

char *get_static_version_string(uint8_t major, uint8_t minor, uint8_t patch)
{
  int ret = snprintf(version_buf, 10, "%d.%d.%d", major, minor, patch);

  if (ret < 0) {
    log_trace("snprintf");
    return NULL;
  }

  return version_buf;
}

void show_app_version(void)
{
  fprintf(stdout, "edgesec app version %s\n",
    get_static_version_string(EDGESEC_VERSION_MAJOR, EDGESEC_VERSION_MINOR,
                              EDGESEC_VERSION_PATCH));
}
void show_app_help(char *app_name)
{
  show_app_version();
  fprintf(stdout, "Usage:\n");
  fprintf(stdout, USAGE_STRING, app_name);
  fprintf(stdout, "\nOptions:\n");
  fprintf(stdout, "\t-c filename\t Path to the config file name\n");
  fprintf(stdout, "\t-d\t\t Verbosity level (use multiple -dd... to increase)\n");
  fprintf(stdout, "\t-h\t\t Show help\n");
  fprintf(stdout, "\t-v\t\t Show app version\n\n");
  fprintf(stdout, "Copyright Nquirignminds Ltd\n\n");
  exit(EXIT_SUCCESS);
}

/* Diagnose an error in command-line arguments and
   terminate the process */
void log_cmdline_error(const char *format, ...)
{
    va_list argList;

    fflush(stdout);           /* Flush any pending stdout */

    fprintf(stdout, "Command-line usage error: ");
    va_start(argList, format);
    vfprintf(stdout, format, argList);
    va_end(argList);

    fflush(stderr);           /* In case stderr is not line-buffered */
    exit(EXIT_FAILURE);
}

void process_app_options(int argc, char *argv[], uint8_t *verbosity,
                          const char **config_filename)
{
  int opt;

  while ((opt = getopt(argc, argv, OPT_STRING)) != -1) {
    switch (opt) {
    case 'h':
      show_app_help(argv[0]);
      break;
    case 'v':
      show_app_version();
      break;
    case 'c':
      *config_filename = optarg;
      break;
    case 'd':
      (*verbosity)++;
      break;
    case ':':
      log_cmdline_error("Missing argument for -%c\n", optopt);
      break;
    case '?':
      log_cmdline_error("Unrecognized option -%c\n", optopt);
      break;
    default: show_app_help(argv[0]);
    }
  }
}

bool get_config_dhcpinfo(char *info, config_dhcpinfo_t *el)
{
  UT_array *info_arr;
  utarray_new(info_arr, &ut_str_icd);

  split_string_array(info, ',', info_arr);

  if (!utarray_len(info_arr))
    goto err;

  char **p = NULL;
  p = (char**) utarray_next(info_arr, p);
  if (*p != NULL) {
    el->vlanid = (int) strtol(*p, NULL, 10);
    if (errno == EINVAL)
      goto err;
  } else
    goto err;

  p = (char**) utarray_next(info_arr, p);
  if (*p != NULL) {
    strcpy(el->ip_addr_low, *p);
  } else
    goto err;

  p = (char**) utarray_next(info_arr, p);
  if (*p != NULL)
    strcpy(el->ip_addr_upp, *p);
  else
    goto err;

  p = (char**) utarray_next(info_arr, p);
  if (*p != NULL)
    strcpy(el->subnet_mask, *p);
  else
    goto err;

  p = (char**) utarray_next(info_arr, p);
  if (*p != NULL)
    strcpy(el->lease_time, *p);
  else
    goto err;

  utarray_free(info_arr);
  return true;

err:
  utarray_free(info_arr);
  return false;
}

bool get_config_ifinfo(char *info, config_ifinfo_t *el)
{
  UT_array *info_arr;
  utarray_new(info_arr, &ut_str_icd);

  split_string_array(info, ',', info_arr);

  if (!utarray_len(info_arr))
    goto err;

  char **p = NULL;
  p = (char**) utarray_next(info_arr, p);
  if (*p != NULL) {
    el->vlanid = (int) strtol(*p, NULL, 10);
    if (errno == EINVAL)
      goto err;
  } else
    goto err;

  p = (char**) utarray_next(info_arr, p);
  if (*p != NULL) {
    strcpy(el->ip_addr, *p);
  } else
    goto err;

  p = (char**) utarray_next(info_arr, p);
  if (*p != NULL)
    strcpy(el->brd_addr, *p);
  else
    goto err;

  p = (char**) utarray_next(info_arr, p);
  if (*p != NULL)
    strcpy(el->subnet_mask, *p);
  else
    goto err;

  utarray_free(info_arr);
  return true;

err:
  utarray_free(info_arr);
  return false;
}

bool get_connection_info(char *info, struct mac_conn *el)
{
  UT_array *info_arr;
  utarray_new(info_arr, &ut_str_icd);

  split_string_array(info, ',', info_arr);

  if (!utarray_len(info_arr))
    goto err;

  char **p = NULL;

  p = (char**) utarray_next(info_arr, p);
  if (*p != NULL) {
    if (strcmp(*p, "a") == 0)
      el->info.allow_connection = true;
    else if (strcmp(*p, "d") == 0)
      el->info.allow_connection = false;
    else
      goto err;
  } else
    goto err;

  p = (char**) utarray_next(info_arr, p);
  if (*p != NULL) {
    if (hwaddr_aton2(*p, el->mac_addr) == -1) {
      goto err;
    }
  } else
    goto err;

  p = (char**) utarray_next(info_arr, p);
  if (*p != NULL) {
    el->info.vlanid = (int) strtol(*p, NULL, 10);
    if (errno == EINVAL)
      goto err;
  } else
    goto err;

  p = (char**) utarray_next(info_arr, p);
  if (*p != NULL) {
    el->info.nat = (bool) strtol(*p, NULL, 10);
    if (errno == EINVAL)
      goto err;    
  } else
    goto err;

  p = (char**) utarray_next(info_arr, p);
  if (*p != NULL) {
    strncpy(el->info.pass, *p, AP_SECRET_LEN);
    el->info.pass_len = strlen(el->info.pass);
  } else
    goto err;

  os_memset(el->info.ip_addr, 0x0, IP_LEN);
  
  utarray_free(info_arr);
  return true;

err:
  utarray_free(info_arr);
  return false;
}

bool load_interface_list(const char *filename, struct app_config *config)
{
  char *key = os_malloc(INI_BUFFERSIZE);
  int idx = 0;
  while(ini_getkey("interfaces", idx++, key, INI_BUFFERSIZE, filename) > 0) {
    char *value = os_malloc(INI_BUFFERSIZE);
    ini_gets("interfaces", key, "", value, INI_BUFFERSIZE, filename);
    if (strstr(key, "if") == (char *)key) {
      config_ifinfo_t el;
      if(!get_config_ifinfo(value, &el)) {
        os_free(value);
        os_free(key);
        return false;
      }
      utarray_push_back(config->config_ifinfo_array, &el);
    }
    os_free(value);
    os_free(key);
    key = os_malloc(INI_BUFFERSIZE);
  }

  os_free(key);
  return true;
}

bool load_dhcp_list(const char *filename, struct app_config *config)
{
  char *key = os_malloc(INI_BUFFERSIZE);
  int idx = 0;
  while(ini_getkey("dhcp", idx++, key, INI_BUFFERSIZE, filename) > 0) {
    char *value = os_malloc(INI_BUFFERSIZE);
    ini_gets("dhcp", key, "", value, INI_BUFFERSIZE, filename);
    if (strstr(key, "dhcpRange") == (char *)key) {
      config_dhcpinfo_t el;
      if(!get_config_dhcpinfo(value, &el)) {
        os_free(value);
        os_free(key);
        return false;
      }
      utarray_push_back(config->dhcp_config.config_dhcpinfo_array, &el);
    }
    os_free(value);
    os_free(key);
    key = os_malloc(INI_BUFFERSIZE);
  }

  os_free(key);
  return true;
}

bool load_connection_list(const char *filename, struct app_config *config)
{
  char *key = os_malloc(INI_BUFFERSIZE);
  int idx = 0;
  while(ini_getkey("connections", idx++, key, INI_BUFFERSIZE, filename) > 0) {
    char *value = os_malloc(INI_BUFFERSIZE);
    int count = ini_gets("connections", key, "", value, INI_BUFFERSIZE, filename);
    if (count) {
      struct mac_conn el;
      if(!get_connection_info(value, &el)) {
        os_free(value);
        os_free(key);
        return false;
      }

      utarray_push_back(config->connections, &el);
    }

    os_free(value);
    os_free(key);
    key = os_malloc(INI_BUFFERSIZE);
  }

  os_free(key);
  return true;
}
bool load_radius_conf(const char *filename, struct app_config *config)
{
  char *value = os_malloc(INI_BUFFERSIZE);

  // Load AP name
  value = os_malloc(INI_BUFFERSIZE);
  int ret = ini_gets("ap", "ssid", "", value, INI_BUFFERSIZE, filename);
  if (!ret) {
    fprintf(stderr, "ap ssid was not specified\n");
    os_free(value);
    return false;
  }

  strncpy(config->hconfig.ssid, value, AP_NAME_LEN);
  os_free(value);

  // Load AP password
  value = os_malloc(INI_BUFFERSIZE);
  ini_gets("ap", "wpaPassphrase", "", value, INI_BUFFERSIZE, filename);

  strncpy(config->hconfig.wpa_passphrase, value, AP_SECRET_LEN);
  os_free(value);

  // Load AP interface
  value = os_malloc(INI_BUFFERSIZE);
  ret = ini_gets("ap", "interface", "", value, INI_BUFFERSIZE, filename);
  if (!ret) {
    fprintf(stderr, "AP interface was not specified\n");
    os_free(value);
    return false;
  }

  strncpy(config->hconfig.interface, value, IFNAMSIZ);
  os_free(value);

  // Load radius port
  config->rconfig.radius_port = (int) ini_getl("radius", "port", 1812, filename);

  // Load radius client ip
  value = os_malloc(INI_BUFFERSIZE);
  ini_gets("radius", "clientIP", "127.0.0.1", value, INI_BUFFERSIZE, filename);

  strncpy(config->rconfig.radius_client_ip, value, IP_LEN);
  os_free(value);

  // Load radius client mask
  config->rconfig.radius_client_mask = (int) ini_getl("radius", "clientMask", 32, filename);

  // Load radius server ip
  value = os_malloc(INI_BUFFERSIZE);
  ini_gets("radius", "serverIP", "127.0.0.1", value, INI_BUFFERSIZE, filename);

  strncpy(config->rconfig.radius_server_ip, value, IP_LEN);
  os_free(value);

  // Load radius server mask
  config->rconfig.radius_server_mask = (int) ini_getl("radius", "serverMask", 32, filename);

  // Load radius secret
  value = os_malloc(INI_BUFFERSIZE);
  ini_gets("radius", "secret", "", value, INI_BUFFERSIZE, filename);
  strncpy(config->rconfig.radius_secret, value, RADIUS_SECRET_LEN);
  os_free(value);

  return true;
}

bool load_ap_conf(const char *filename, struct app_config *config)
{
  char *value = os_malloc(INI_BUFFERSIZE);

  // Load ap file path
  int ret = ini_gets("ap", "apFilePath", "", value, INI_BUFFERSIZE, filename);
  if (!ret) {
    fprintf(stderr, "apFilePath was not specified\n");
    os_free(value);
    return false;
  }

  strncpy(config->hconfig.ap_file_path, value, MAX_OS_PATH_LEN);
  os_free(value);

  // Load ap bin path
  value = os_malloc(INI_BUFFERSIZE);
  ret = ini_gets("ap", "apBinPath", "", value, INI_BUFFERSIZE, filename);
  if (!ret) {
    fprintf(stderr, "apBinPath was not specified\n");
    os_free(value);
    return false;
  }

  strncpy(config->hconfig.ap_bin_path, value, MAX_OS_PATH_LEN);
  os_free(value);

  // Load ap log path
  value = os_malloc(INI_BUFFERSIZE);
  ret = ini_gets("ap", "apLogPath", "", value, INI_BUFFERSIZE, filename);
  strncpy(config->hconfig.ap_log_path, value, MAX_OS_PATH_LEN);
  os_free(value);

  // Load ap bridge
  value = os_malloc(INI_BUFFERSIZE);
  ret = ini_gets("ap", "bridge", "", value, INI_BUFFERSIZE, filename);
  if (!ret) {
    fprintf(stderr, "ap bridge was not specified\n");
    os_free(value);
    return false;
  }

  strncpy(config->hconfig.bridge, value, IFNAMSIZ);
  os_free(value);

  // Load ap driver
  value = os_malloc(INI_BUFFERSIZE);
  ini_gets("ap", "driver", "nl80211", value, INI_BUFFERSIZE, filename);
  strncpy(config->hconfig.driver, value, AP_DRIVE_LEN);
  os_free(value);

  // Load ap hw mode
  value = os_malloc(INI_BUFFERSIZE);
  ini_gets("ap", "hwMode", "g", value, INI_BUFFERSIZE, filename);
  strncpy(config->hconfig.hw_mode, value, AP_HWMODE_LEN);
  os_free(value);

  // Load ap channel
  config->hconfig.channel = (int) ini_getl("ap", "channel", 11, filename);

  // Load ap wmmEnabled
  config->hconfig.wmm_enabled = (int) ini_getl("ap", "wmmEnabled", 1, filename);

  // Load ap authAlgs
  config->hconfig.auth_algs = (int) ini_getl("ap", "authAlgs", 1, filename);

  // Load ap wpa
  config->hconfig.wpa = (int) ini_getl("ap", "wpa", 2, filename);

  // Load ap wpaKeyMgmt
  value = os_malloc(INI_BUFFERSIZE);
  ini_gets("ap", "wpaKeyMgmt", "WPA-PSK", value, INI_BUFFERSIZE, filename);
  strncpy(config->hconfig.wpa_key_mgmt, value, AP_WPA_KEY_MGMT_LEN);
  os_free(value);

  // Load ap rsnPairwise
  value = os_malloc(INI_BUFFERSIZE);
  ini_gets("ap", "rsnPairwise", "CCMP", value, INI_BUFFERSIZE, filename);
  strncpy(config->hconfig.rsn_pairwise, value, AP_RSN_PAIRWISE_LEN);
  os_free(value);
  
  // Load ap ctrlInterface
  value = os_malloc(INI_BUFFERSIZE);
  ini_gets("ap", "ctrlInterface", "/var/run/hostapd", value, INI_BUFFERSIZE, filename);
  strncpy(config->hconfig.ctrl_interface, value, MAX_OS_PATH_LEN);
  os_free(value);

  // Load ap macaddrAcl
  config->hconfig.macaddr_acl = (int) ini_getl("ap", "macaddrAcl", 2, filename);

  // Load ap dynamicVlan
  config->hconfig.dynamic_vlan = (int) ini_getl("ap", "dynamicVlan", 1, filename);

  // Load ap vlanBridge
  value = os_malloc(INI_BUFFERSIZE);
  ret = ini_gets("ap", "vlanBridge", "", value, INI_BUFFERSIZE, filename);
  if (!ret) {
    fprintf(stderr, "ap vlanBridge was not specified\n");
    os_free(value);
    return false;
  }

  strncpy(config->hconfig.vlan_bridge, value, IFNAMSIZ);
  os_free(value);

  // Load ap vlanFile
  value = os_malloc(INI_BUFFERSIZE);
  ret = ini_gets("ap", "vlanFile", "", value, INI_BUFFERSIZE, filename);
  if (!ret) {
    fprintf(stderr, "ap vlanFile was not specified\n");
    os_free(value);
    return false;
  }

  strncpy(config->hconfig.vlan_file, value, MAX_OS_PATH_LEN);
  os_free(value);

  // Load ap loggerStdout
  config->hconfig.logger_stdout = (int) ini_getl("ap", "loggerStdout", -1, filename);

  // Load ap loggerStdoutLevel
  config->hconfig.logger_stdout_level = (int) ini_getl("ap", "loggerStdoutLevel", 0, filename);

  // Load ap loggerSyslog
  config->hconfig.logger_syslog = (int) ini_getl("ap", "loggerSyslog", -1, filename);

  // Load ap loggerStdoutLevel
  config->hconfig.logger_syslog_level = (int) ini_getl("ap", "loggerSyslogLevel", 0, filename);

  // Load ap ignoreBroadcastSsid
  config->hconfig.ignore_broadcast_ssid = (int) ini_getl("ap", "ignoreBroadcastSsid", 0, filename);

  // Load ap wpaPskRadius
  config->hconfig.wpa_psk_radius = (int) ini_getl("ap", "wpaPskRadius", 2, filename);

  return true;
}

bool load_dns_conf(const char *filename, struct app_config *config)
{
  char *value = os_malloc(INI_BUFFERSIZE);

  // Load the DNS server addresses
  ini_gets("dns", "servers", "", value, INI_BUFFERSIZE, filename);
  split_string_array(value, ',', config->dns_config.server_array);
  os_free(value);

  return true;
}

bool load_dhcp_conf(const char *filename, struct app_config *config)
{
  char *value = os_malloc(INI_BUFFERSIZE);

  // Load dhpc config file path
  int ret = ini_gets("dhcp", "dhcpConfigPath", "", value, INI_BUFFERSIZE, filename);
  if (!ret) {
    fprintf(stderr, "dhcp dhcpConfigPath was not specified\n");
    os_free(value);
    return false;
  }

  strncpy(config->dhcp_config.dhcp_conf_path, value, MAX_OS_PATH_LEN);
  os_free(value);

  // Load dhpc script file path
  value = os_malloc(INI_BUFFERSIZE);
  ret = ini_gets("dhcp", "dhcpScriptPath", "", value, INI_BUFFERSIZE, filename);
  if (!ret) {
    fprintf(stderr, "dhcp dhcpScriptPath was not specified\n");
    os_free(value);
    return false;
  }

  strncpy(config->dhcp_config.dhcp_script_path, value, MAX_OS_PATH_LEN);
  os_free(value);

  // Load the dhcprange params
  if (!load_dhcp_list(filename, config)) {
    fprintf(stderr, "load_dhcp_list parsing error\n");
    return false;
  }
  return true;
}

void load_app_config(const char *filename, struct app_config *config)
{
  FILE *fp = fopen(filename, "rb");

  if (fp == NULL) {
    fprintf(stderr, "Couldn't open %s config file.\n", filename);
    exit(1);
  }

  char *value = os_malloc(INI_BUFFERSIZE);

  // Load the bin paths array
  ini_gets("system", "binPath", "/bin", value, INI_BUFFERSIZE, filename);
  split_string_array(value, ':', config->bin_path_array);
  os_free(value);

  // Load create interfaces flag
  config->create_interfaces = ini_getbool("system", "createInterfaces", 0, filename);

  // Load ignore error on interface create
  config->ignore_if_error = ini_getbool("system", "ignoreErrorOnIfCreate", 0, filename);

  // Load the AP detect flag
  config->ap_detect = ini_getbool("system", "apDetect", 0, filename);

  // Load the exec ap flag
  config->exec_ap = ini_getbool("system", "execAp", 0, filename);

  // Load the exec radius flag
  config->exec_radius = ini_getbool("system", "execRadius", 0, filename);

  // Load the exec dhcp flag
  config->exec_dhcp = ini_getbool("system", "execDhcp", 0, filename);

  // Load the default open vlanid
  config->default_open_vlanid = (int) ini_getl("system", "defaultOpenVlanId", 0, filename);

  // Load NAT interface
  value = os_malloc(INI_BUFFERSIZE);
  ini_gets("nat", "natInterface", "", value, INI_BUFFERSIZE, filename);

  strncpy(config->nat_interface, value, IFNAMSIZ);
  os_free(value);

  // Load domainServerPath
  value = os_malloc(INI_BUFFERSIZE);
  int ret = ini_gets("supervisor", "domainServerPath", "", value, INI_BUFFERSIZE, filename);
  if (!ret) {
    fprintf(stderr, "Domain server path was not specified\n");
    exit(1);
  }

  strncpy(config->domain_server_path, value, MAX_OS_PATH_LEN);
  os_free(value);

  // Load allow all connection flag
  config->allow_all_connections = ini_getbool("system", "allowAllConnections", 0, filename);

  // Load killRunningProcess flag
  config->kill_running_proc = ini_getbool("system", "killRunningProcess", 0, filename);

  // Load ap radius config params
  if(!load_radius_conf(filename, config)) {
    fprintf(stderr, "radius config parsing error.\n");
    exit(1);
  }

  // Load ap config params
  if(!load_ap_conf(filename, config)) {
    fprintf(stderr, "ap config parsing error.\n");
    exit(1);
  }

  // Load the DNS server configuration
  if(!load_dns_conf(filename, config)) {
    fprintf(stderr, "dns config parsing error.\n");
    exit(1);
  }

  // Load the DHCP server configuration
  if(!load_dhcp_conf(filename, config)) {
    fprintf(stderr, "dhcp config parsing error.\n");
    exit(1);
  }

  // Load the list of connections
  if(!load_connection_list(filename, config)) {
    fprintf(stderr, "Connection list parsing error.\n");
    exit(1);
  }

  // Load the list of interfaces
  if(!load_interface_list(filename, config)) {
    fprintf(stderr, "Interface list parsing error.\n");
    exit(1);
  }
}

char *get_app_name(char *app_path) {
  return basename(app_path);
}

int main(int argc, char *argv[])
{
  uint8_t verbosity = 0;
  uint8_t level = 0;
  const char *filename = NULL;
  UT_array *bin_path_arr;
  UT_array *config_ifinfo_arr;
  UT_array *config_dhcpinfo_arr;
  UT_array *mac_conn_arr;
  UT_array *server_arr;
  struct app_config config;
  
  // Init the app config struct
  memset(&config, 0, sizeof(struct app_config));

  // Create the empty dynamic array for bin path strings
  utarray_new(bin_path_arr, &ut_str_icd);
  config.bin_path_array = bin_path_arr;

  // Create the config interface
  utarray_new(config_ifinfo_arr, &config_ifinfo_icd);
  config.config_ifinfo_array = config_ifinfo_arr;

  // Create the dhcp config interface
  utarray_new(config_dhcpinfo_arr, &config_dhcpinfo_icd);
  config.dhcp_config.config_dhcpinfo_array = config_dhcpinfo_arr;

  // Create the connections list
  utarray_new(mac_conn_arr, &mac_conn_icd);
  config.connections = mac_conn_arr;

  // Create the dns server array
  utarray_new(server_arr, &ut_str_icd);
  config.dns_config.server_array = server_arr;

  process_app_options(argc, argv, &verbosity, &filename);

  if (verbosity > MAX_LOG_LEVELS) {
    level = 0;
  } else if (!verbosity) {
    level = MAX_LOG_LEVELS - 1;
  } else {
    level = MAX_LOG_LEVELS - verbosity;
  }
  
  if (optind <= 1) show_app_help(argv[0]);

  load_app_config(filename, &config);

  // Kill all edgesec processes if running
  if (config.kill_running_proc) {
    if(!kill_process(get_app_name(argv[0]))){
      fprintf(stderr, "kill_process fail.\n");
      exit(1);
    }
  }

  if (!run_engine(&config, level)) {
    fprintf(stderr, "Failed to start edgesec engine.\n");
  } else
    fprintf(stderr, "Edgesec engine stopped.\n");

  utarray_free(bin_path_arr);
  utarray_free(config_ifinfo_arr);
  utarray_free(config_dhcpinfo_arr);
  utarray_free(mac_conn_arr);
  utarray_free(server_arr);
  exit(0);
}
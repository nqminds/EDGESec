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
 * @file engine.h 
 * @author Alexandru Mereacre 
 * @brief File containing the definition of the app configuration structure.
 */
#ifndef ENGINE_H
#define ENGINE_H

#include <net/if.h>
#include <inttypes.h>
#include <stdbool.h>

#include "utils/utarray.h"
#include "utils/hashmap.h"
#include "utils/os.h"
#include "hostapd/config_generator.h"
#include "radius/radius_server.h"
#include "if_service.h"
#include "supervisor/mac_mapper.h"

/**
 * @brief The App configuration structures. Used for configuring the networking services.
 * 
 */
struct app_config {
  UT_array            *bin_path_array;                      /**< The array including the paths of systems binaries. */
  bool                ap_detect;                            /**< Flag to detect an existing wifi interface to create the access point. */
  bool                exec_hostapd;                         /**< Flag to execute the hostapd service. */
  bool                exec_radius;                          /**< Flag to execute the radius service. */
  char                nat_interface[IFNAMSIZ];              /**< The NAT interface string. */
  bool                create_interfaces;                    /**< Flag to create the WiFi subnet interfaces. */
  bool                ignore_if_error;                      /**< Flag if set ignores the errors if subnet already exists. */
  int                 default_open_vlanid;                  /**< Sets the default vlan index for open connections or if MAC is not in the list of connections. */
  UT_array            *config_ifinfo_array;                 /**< Interface list mapping interface name and IP address range. */
  char                subnet_mask[IP_LEN];                  /**< Subnet mask string for WiFi subnets. */
  char                domain_server_path[MAX_OS_PATH_LEN];  /**< Path to the control server. */
  bool                allow_all_connections;                /**< Flag to allow all connections. */
  UT_array            *connections;                         /**< MAC mapper to @c struct mac_conn. */
  struct radius_conf  rconfig;                              /**< Radius service configuration. */
  struct hostapd_conf hconfig;                              /**< Hostapd service configuration. */
};

/**
 * @brief Executes the edgesec WiFi networking engine. Creates subnets and starts the supervisor, radius servers and hostapd service.
 * 
 * @param app_config The app configuration structures, setting WiFi network config params.
 * @param log_level The logging level.
 * @return @c true if succes, @c false if a service fails to start.
 */
bool run_engine(struct app_config *app_config, uint8_t log_level);

#endif
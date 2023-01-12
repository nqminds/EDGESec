/**
 * @file
 * @author Alexandru Mereacre
 * @date 2021
 * @copyright
 * SPDX-FileCopyrightText: © 2021 NQMCyber Ltd and edgesec contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 * @brief File containing the definition of the radius service.
 */

#ifndef RADIUS_SERVICE_H
#define RADIUS_SERVICE_H

#include <eloop.h>
#include "../supervisor/supervisor.h"

#include "radius_config.h"
#include "radius_server.h"

/**
 * @brief Runs the radius service
 *
 * @param eloop The eloop context
 * @param rconf The radius config
 * @param get_vlaninfo_fn The radius callback function
 * @param radius_callback_args The Radius callback arguments
 * @return Pointer to private RADIUS server context or NULL on failure
 */
struct radius_context *run_radius(struct eloop_data *eloop,
                                  struct radius_conf *rconf,
                                  get_vlaninfo_cb get_vlaninfo_fn,
                                  void *ctx_cb);

/**
 * @brief Closes the radius service
 *
 * @param ctx Pointer to private RADIUS server context
 */
void close_radius(struct radius_context *ctx);

#endif

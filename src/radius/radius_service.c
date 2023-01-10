/**
 * @file
 * @author Alexandru Mereacre
 * @date 2021
 * @copyright
 * SPDX-FileCopyrightText: © 2021 NQMCyber Ltd and edgesec contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 * @brief File containing the implementation of the radius service.
 */

#include <stdbool.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "common.h"

#include <eloop.h>
#include <eap_server/eap.h>

#include "radius_server.h"

#define EAP_SERVER_IDENTITY "edgesec"

int radius_get_eap_user(void *ctx, const u8 *identity,
				       size_t identity_len, int phase2,
				       struct eap_user *user) {
  (void)ctx;
  (void)identity;
  (void)identity_len;

  // struct radius_context *context = (struct radius_context *) ctx;

  *user = (struct eap_user){
      .macacl = 1,
  };

  // user->methods[0].vendor = EAP_VENDOR_IETF;
  // user->methods[0].method = EAP_TYPE_TLS;
  log_trace("radius_get_eap_user: phase2=%d", phase2);
  return 0;
}

int generate_client_conf(struct radius_conf *rconf) {
  FILE *fp = fopen(rconf->client_conf_path, "w");

  if (fp == NULL) {
    log_errno("fopen");
    return -1;
  }

  log_debug("Writing into %s", rconf->client_conf_path);

  fprintf(fp, "%s/%d %s\n", rconf->radius_client_ip, rconf->radius_client_mask, rconf->radius_secret);

  fclose(fp);
  return 0;
}

struct eap_config* generate_eap_config(struct radius_conf *rconf) {
  (void)rconf;

  struct eap_config *cfg = sys_zalloc(sizeof(struct eap_config));

  if (cfg == NULL) {
    log_errno("sys_zalloc");
    return NULL;
  }

	cfg->ssl_ctx = NULL; // Actual ssl context
	cfg->tls_session_lifetime = 0;

#define TLS_CONN_DISABLE_TLSv1_3 BIT(13)

	cfg->tls_flags = TLS_CONN_DISABLE_TLSv1_3;
	cfg->max_auth_rounds = 100;
	cfg->max_auth_rounds_short = 50;
	cfg->server_id = (u8 *) os_strdup(EAP_SERVER_IDENTITY);
	cfg->server_id_len = os_strlen(EAP_SERVER_IDENTITY);
	cfg->erp = -1;

  /*
  cfg->eap_server = 0;
	cfg->msg_ctx = NULL;
	cfg->eap_sim_db_priv = NULL;
  cfg->pac_opaque_encr_key = 0;
	cfg->eap_fast_a_id = NULL;
	cfg->eap_fast_a_id_len = 0;
  cfg->eap_fast_a_id_info = NULL;
	cfg->eap_fast_prov = 0;
	cfg->pac_key_lifetime = 0;
	cfg->pac_key_refresh_time = 0;
	cfg->eap_teap_auth = 0;
	cfg->eap_teap_pac_no_inner = 0;
	cfg->eap_teap_separate_result = 0;
	cfg->eap_teap_id = 0;
	cfg->eap_sim_aka_result_ind = 0;
	cfg->eap_sim_id = 0;
	cfg->tnc = 0;
	cfg->wps = NULL;
	cfg->fragment_size = 0;
	cfg->pwd_group = 0;
	cfg->pbc_in_m1 = 0;
  */

  return cfg;
}

int generate_radius_server_conf(struct eloop_data *eloop,
                                struct radius_conf *rconf,
                                struct radius_context *context) {
	context->conf.eloop = eloop;
	context->conf.auth_port = rconf->radius_port;
	context->conf.client_file = rconf->client_conf_path;
	context->conf.conf_ctx = (void *)context;
	context->conf.get_eap_user = radius_get_eap_user;

  if((context->conf.eap_cfg = generate_eap_config(rconf)) == NULL) {
    log_error("generate_eap_config fail");
    return -1;
  }

  context->conf.acct_port = 0;
  context->conf.sqlite_file = NULL;
  context->conf.subscr_remediation_url = NULL;
  context->conf.subscr_remediation_method = 0;
	context->conf.erp_domain = NULL;
	context->conf.ipv6 = 0;
  context->conf.eap_req_id_text = NULL;
  context->conf.hs20_sim_provisioning_url = NULL;
  context->conf.t_c_server_url = NULL;

  return 0;
}

void close_radius(struct radius_context *ctx) {
  if (ctx != NULL) {
    if (ctx->conf.eap_cfg != NULL) {
      os_free(ctx->conf.eap_cfg->server_id);
      os_free(ctx->conf.eap_cfg);
      ctx->conf.eap_cfg = NULL;
    }
    radius_server_deinit(ctx->srv);
    ctx->srv = NULL;
    os_free(ctx);
  }
}

struct radius_context *run_radius(struct eloop_data *eloop,
                                  struct radius_conf *rconf,
                                  mac_conn_fn radius_callback_fn,
                                  void *radius_callback_args) {
  (void)eloop;
  (void)rconf;
  (void)radius_callback_fn;
  (void)radius_callback_args;

  struct radius_context *context = sys_zalloc(sizeof(struct radius_context));

  if (context == NULL) {
    log_errno("sys_zalloc");
    return NULL;
  }

  if (generate_client_conf(rconf) < 0) {
    log_error("generate_client_conf fail");
    close_radius(context);
    return NULL;
  }

  if (generate_radius_server_conf(eloop, rconf, context) < 0) {
    log_error("generate_radius_server_conf fail");
    close_radius(context);
    return NULL;
  }

  if ((context->srv = radius_server_init(&context->conf)) == NULL) {
    log_error("radius_server_init failure");
    close_radius(context);
    return NULL;
  }

  return context;
}

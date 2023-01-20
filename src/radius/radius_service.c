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

#include <crypto/tls.h>
#include <eloop.h>
#include <eap_server/eap.h>

#include "../supervisor/dev_mapper.h"
#include "../supervisor/identity.h"
#include "./attr_mapper.h"
#include "radius.h"
#include "radius_server.h"

#define EAP_SERVER_IDENTITY "edgesec"

struct hostapd_radius_attr *get_vlan_attribute(uint16_t vlan_id, struct hostapd_radius_attr **last) {
  char id_str[5];
  struct hostapd_radius_attr *attr = NULL,
                             *attr_medium_type = NULL,
                             *attr_id = NULL;

#define RADIUS_ATTR_TUNNEL_VALUE 13
#define RADIUS_ATTR_TUNNEL_MEDIUM_VALUE 6

  *last = NULL;

  if ((attr = sys_zalloc(sizeof(*attr))) == NULL) {
    log_errno("sys_zalloc");
    return NULL;
  }

  attr->type = RADIUS_ATTR_TUNNEL_TYPE;
  if ((attr->val = wpabuf_alloc(4)) == NULL) {
    log_error("wpabuf_alloc fail");
    goto get_vlan_attribute_fail;
  }

  wpabuf_put_be32(attr->val, RADIUS_ATTR_TUNNEL_VALUE);

  if ((attr_medium_type = sys_zalloc(sizeof(*attr_medium_type))) == NULL) {
    log_errno("sys_zalloc");
    goto get_vlan_attribute_fail;
  }

  attr_medium_type->type = RADIUS_ATTR_TUNNEL_MEDIUM_TYPE;

  if ((attr_medium_type->val = wpabuf_alloc(4)) == NULL) {
    log_error("wpabuf_alloc fail");
    goto get_vlan_attribute_fail;
  }

  wpabuf_put_be32(attr_medium_type->val, RADIUS_ATTR_TUNNEL_MEDIUM_VALUE);

  if ((attr_id = sys_zalloc(sizeof(*attr_id))) == NULL) {
    log_errno("sys_zalloc");
    goto get_vlan_attribute_fail;
  }

  sprintf(id_str, "%d", vlan_id);
  attr_id->type = RADIUS_ATTR_TUNNEL_PRIVATE_GROUP_ID;
  if((attr_id->val = wpabuf_alloc_copy(id_str, strlen(id_str))) == NULL) {
    log_errno("wpabuf_alloc_copy fail");
    goto get_vlan_attribute_fail;
  }

  attr->next = attr_medium_type;
  attr_medium_type->next = attr_id;
  attr_id->next = NULL;

  *last = attr_id;

  return attr;

get_vlan_attribute_fail:
    free_attr(attr);
    free_attr(attr_medium_type);
    free_attr(attr_id);
  return NULL;
}

struct hostapd_radius_attr *
get_tunnel_pass_attribute(const uint8_t *req_authenticator, const uint8_t *secret,
                       size_t secret_len, const uint8_t *key, size_t key_len) {
  struct hostapd_radius_attr *attr = NULL;
  uint16_t salt;
  size_t elen;
  uint8_t *buf = NULL, *pos = NULL;
  uint8_t tag_salt_len = 3;
  uint16_t packet_len =
      tag_salt_len + 1 + key_len + 15; // tag + salt + len + key_len + padding

  if ((buf = sys_zalloc(packet_len)) == NULL) {
    log_errno("sys_zalloc");
    return 0;
  }

  if (get_random((uint8_t *)&salt, sizeof(salt)) < 0) {
    log_error("get_random fail");
    goto get_password_attribute_fail;
  }

  salt |= 0x8000;

  pos = buf + 1;
  WPA_PUT_BE16(pos, salt);

  pos += 2;
  encrypt_ms_key(key, key_len, salt, req_authenticator, secret, secret_len, pos,
                 &elen);

  if ((attr = sys_zalloc(sizeof(*attr))) == NULL) {
    log_errno("sys_zalloc");
    goto get_password_attribute_fail;
  }

  attr->type = RADIUS_ATTR_TUNNEL_PASSWORD;
  if((attr->val = wpabuf_alloc_copy(buf, tag_salt_len + elen)) == NULL) {
    log_errno("wpabuf_alloc_copy fail");
    goto get_password_attribute_fail;
  }

  attr->next = NULL;

  os_free(buf);
  return attr;

get_password_attribute_fail:
  free_attr(attr);
  os_free(buf);
  return NULL;
}

int save_user_attribute(struct radius_context *context, const u8 *identity,
				       size_t identity_len,
				       struct eap_user *user, struct radius_msg *msg,
               enum RADIUS_USER_ATTR user_attr, struct identity_info *iinfo) {

  if (user_attr == RADIUS_USER_VLAN_ATTR || user_attr == RADIUS_USER_VLANPASS_ATTR) {
    struct hostapd_radius_attr *last_attr = NULL;
    struct hostapd_radius_attr *vlan_attr = get_vlan_attribute(iinfo->vlanid, &last_attr);

    if (vlan_attr == NULL) {
      log_error("get_vlan_attribute fail");
      return -1;
    }

    if (msg != NULL && iinfo->id_pass_len && user_attr == RADIUS_USER_VLANPASS_ATTR) {
      struct radius_hdr *hdr = radius_msg_get_hdr(msg);
      struct hostapd_radius_attr *pass_attr = get_tunnel_pass_attribute(
                                                    hdr->authenticator,
                                                    (const uint8_t *)context->rconf->radius_secret,
                                                    os_strlen(context->rconf->radius_secret),
                                                    iinfo->id_pass, iinfo->id_pass_len);
      if (pass_attr == NULL) {
        log_error("get_tunnel_pass_attribute fail");
        free_attr(vlan_attr);
        return -1;
      }

      last_attr->next = pass_attr;
    } else {
      log_trace("msg attr is NULL");
    }

    log_trace("Saving user RADIUS attribute");
    if (put_attr_mapper(&context->attr_mapper, identity, identity_len, vlan_attr) < 0) {
      log_error("put_attr_mapper fail");
      free_attr(vlan_attr);
      return -1;
    }
    if (get_attr_mapper(&context->attr_mapper, identity, identity_len, &user->accept_attr) < 0) {
      log_error("get_attr_mapper fail");
      free_attr(vlan_attr);
      return -1;
    }
  }

  return 0;
}

int radius_get_eap_user(void *ctx, const u8 *identity,
				       size_t identity_len, int phase2,
				       struct eap_user *user, struct radius_msg *msg,
               enum RADIUS_USER_ATTR user_attr) {
  struct radius_context *context = (struct radius_context *) ctx;

  *user = (struct eap_user){
      .macacl = 1,
  };

  user->methods[0].vendor = EAP_VENDOR_IETF;
  user->methods[0].method = EAP_TYPE_TLS;

  log_trace("radius_get_eap_user: phase2=%d", phase2);
  if (identity_len && identity != NULL) {
    log_trace("Received RADIUS identity=%.*s", identity_len, identity);

    user->password = (u8 *) os_memdup(identity, identity_len);
    user->password_len = identity_len;
    user->salt = NULL;

    if (context->get_identity_ac_fn != NULL) {
      struct identity_info *iinfo = NULL;

      if ((iinfo = context->get_identity_ac_fn(identity, identity_len, context->ctx_cb)) == NULL) {
        log_error("get_identity_ac_fn fail");
        return -1;
      } else {
        if (iinfo->access == IDENTITY_ACCESS_DENY) {
          user->macacl = 0;
        } else if (iinfo->access == IDENTITY_ACCESS_ALLOW) {
          if (save_user_attribute(context, identity, identity_len, user, msg, user_attr, iinfo) < 0) {
            log_error("save_user_attribute fail");
            free_identity_info(iinfo);
            return -1;
          }
          user->macacl = 1;
        }
      }
      free_identity_info(iinfo);
    } else {
      log_error("RADIUS callback is NULL");
      user->macacl = 0;
    }
  } else {
    log_trace("Identity is NULL for RADIUS EAP user.");
    user->macacl = 0;
  }

  return 0;
}

int generate_client_conf(struct radius_conf *rconf) {
  log_debug("Writing into %s", rconf->client_conf_path);

  FILE *fp = fopen(rconf->client_conf_path, "w");

  if (fp == NULL) {
    log_errno("fopen");
    return -1;
  }

  fprintf(fp, "%s/%d %s\n", rconf->radius_client_ip, rconf->radius_client_mask, rconf->radius_secret);

  fclose(fp);
  return 0;
}

/**
 * @brief Register EAP server methods.
 *
 * @retval  0 on success.
 * @retval -1 on error.
 * @retval -2 on error.
 */
static int register_eap_methods(void) {
  int ret = 0;

  ret = eap_server_identity_register();

  if (ret == 0) {
    ret = eap_server_md5_register();
  }

  if (ret == 0) {
    ret = eap_server_tls_register();
  }

  if (ret == 0) {
    ret = eap_server_mschapv2_register();
  }

  if (ret == 0) {
    ret = eap_server_peap_register();
  }

  if (ret == 0) {
    ret = eap_server_gtc_register();
  }

  if (ret == 0) {
    ret = eap_server_ttls_register();
  }

  if (ret == 0) {
    ret = eap_server_pax_register();
  }

  if (ret == 0) {
    ret = eap_server_psk_register();
  }

  if (ret == 0) {
    ret = eap_server_sake_register();
  }

  if (ret == 0) {
    ret = eap_server_gpsk_register();
  }

  return ret;
}

void *init_eap_tls(struct radius_conf *rconf) {
  void *tls_ctx = NULL;

  struct tls_config tconf;
  struct tls_connection_params tparams;

  os_memset(&tconf, 0, sizeof(tconf));

  if ((tls_ctx = tls_init(&tconf)) == NULL) {
    log_error("tls_init fail");
    return NULL;
  }

  os_memset(&tparams, 0, sizeof(tparams));

  log_trace("Loading EAP CA file %s", rconf->eap_ca_cert_path);
  tparams.ca_cert = rconf->eap_ca_cert_path;

  log_trace("Loading EAP server certificate file %s", rconf->eap_server_cert_path);
  tparams.client_cert = rconf->eap_server_cert_path;

  log_trace("Loading EAP server private key file %s", rconf->eap_server_key_path);
  tparams.private_key = rconf->eap_server_key_path;

  log_trace("Loading EAP DH config params file %s", rconf->eap_dh_path);
  tparams.dh_file = rconf->eap_dh_path;

  if (tls_global_set_params(tls_ctx, &tparams)) {
    log_error("tls_global_set_params fail");
    tls_deinit(tls_ctx);
    return NULL;
  }

  if (tls_global_set_verify(tls_ctx, 0, 1)) {
    log_error("tls_global_set_verify fail");
    tls_deinit(tls_ctx);
    return NULL;
  }

  return tls_ctx;
}

struct eap_config* generate_eap_config(struct radius_conf *rconf) {
  (void)rconf;

  struct eap_config *cfg = sys_zalloc(sizeof(struct eap_config));

  if (cfg == NULL) {
    log_errno("sys_zalloc");
    return NULL;
  }

	cfg->tls_session_lifetime = 0;

#define TLS_CONN_DISABLE_TLSv1_3 BIT(13)

	cfg->tls_flags = TLS_CONN_DISABLE_TLSv1_3;
	cfg->max_auth_rounds = 100;
	cfg->max_auth_rounds_short = 50;
	cfg->server_id = (u8 *) os_strdup(EAP_SERVER_IDENTITY);
	cfg->server_id_len = os_strlen(EAP_SERVER_IDENTITY);
	// cfg->erp = -1;

  cfg->eap_server = 1;
/*
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

	context->sconf = sys_zalloc(sizeof(struct radius_server_conf));

  if (context->sconf == NULL) {
    log_errno("sys_zalloc");
    return -1;
  }

  context->sconf->eloop = eloop;
	context->sconf->auth_port = rconf->radius_port;
	context->sconf->client_file = rconf->client_conf_path;
	context->sconf->conf_ctx = (void *)context;
	context->sconf->get_eap_user = radius_get_eap_user;

  context->sconf->acct_port = 0;
  context->sconf->sqlite_file = NULL;
  context->sconf->subscr_remediation_url = NULL;
  context->sconf->subscr_remediation_method = 0;
	context->sconf->erp_domain = NULL;
	context->sconf->ipv6 = 0;
  context->sconf->eap_req_id_text = NULL;
  context->sconf->hs20_sim_provisioning_url = NULL;
  context->sconf->t_c_server_url = NULL;
  return 0;
}

void close_radius(struct radius_context *context) {
  if (context != NULL) {
    if (context->sconf != NULL) {
      if (context->sconf->eap_cfg != NULL) {
        os_free(context->sconf->eap_cfg->server_id);
        if (context->sconf->eap_cfg->ssl_ctx != NULL) {
          tls_deinit(context->sconf->eap_cfg->ssl_ctx);
        }
        os_free(context->sconf->eap_cfg);
        eap_server_unregister_methods();
      }
      os_free(context->sconf);
    }

    free_attr_mapper(&context->attr_mapper);
    radius_server_deinit(context->srv);
    os_free(context);
  }
}

struct radius_context *run_radius(struct eloop_data *eloop,
                                  struct radius_conf *rconf,
                                  get_identity_ac_cb get_identity_ac_fn,
                                  void *ctx_cb) {
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

  context->rconf = rconf;
  context->get_identity_ac_fn = get_identity_ac_fn;
  context->ctx_cb = ctx_cb;

  if (register_eap_methods() < 0) {
    log_error("eap_server_register_methods fail");
    close_radius(context);
    return NULL;
  }

  if((context->sconf->eap_cfg = generate_eap_config(rconf)) == NULL) {
    log_error("generate_eap_config fail");
    close_radius(context);
    return NULL;
  }

  if((context->sconf->eap_cfg->ssl_ctx = init_eap_tls(rconf)) == NULL) {
    log_error("init_eap_tls fail");
    close_radius(context);
    return NULL;
  }

  if ((context->srv = radius_server_init(context->sconf)) == NULL) {
    log_error("radius_server_init failure");
    close_radius(context);
    return NULL;
  }

  return context;
}

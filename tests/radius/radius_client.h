/*
 * RADIUS client
 * Copyright (c) 2002-2009, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef RADIUS_CLIENT_H
#define RADIUS_CLIENT_H

#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>
#include <linux/types.h>
#include <linux/posix_types.h>
#include <asm/types.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <net/if.h>

#include "ip_addr.h"

struct radius_msg;

/**
 * struct hostapd_radius_server - RADIUS server information for RADIUS client
 *
 * This structure contains information about a RADIUS server. The values are
 * mainly for MIB information. The MIB variable prefix (radiusAuth or
 * radiusAcc) depends on whether this is an authentication or accounting
 * server.
 *
 * radiusAuthClientPendingRequests (or radiusAccClientPendingRequests) is the
 * number struct radius_client_data::msgs for matching msg_type.
 */
struct hostapd_radius_server {
  /**
   * addr - radiusAuthServerAddress or radiusAccServerAddress
   */
  struct hostapd_ip_addr addr;

  /**
   * port - radiusAuthClientServerPortNumber or radiusAccClientServerPortNumber
   */
  int port;

  /**
   * shared_secret - Shared secret for authenticating RADIUS messages
   */
  uint8_t *shared_secret;

  /**
   * shared_secret_len - Length of shared_secret in octets
   */
  size_t shared_secret_len;

  /* Dynamic (not from configuration file) MIB data */

  /**
   * index - radiusAuthServerIndex or radiusAccServerIndex
   */
  int index;

  /**
   * round_trip_time - radiusAuthClientRoundTripTime or
   * radiusAccClientRoundTripTime Round-trip time in hundredths of a second.
   */
  int round_trip_time;

  /**
   * requests - radiusAuthClientAccessRequests or radiusAccClientRequests
   */
  uint32_t requests;

  /**
   * retransmissions - radiusAuthClientAccessRetransmissions or
   * radiusAccClientRetransmissions
   */
  uint32_t retransmissions;

  /**
   * access_accepts - radiusAuthClientAccessAccepts
   */
  uint32_t access_accepts;

  /**
   * access_rejects - radiusAuthClientAccessRejects
   */
  uint32_t access_rejects;

  /**
   * access_challenges - radiusAuthClientAccessChallenges
   */
  uint32_t access_challenges;

  /**
   * responses - radiusAccClientResponses
   */
  uint32_t responses;

  /**
   * malformed_responses - radiusAuthClientMalformedAccessResponses or
   * radiusAccClientMalformedResponses
   */
  uint32_t malformed_responses;

  /**
   * bad_authenticators - radiusAuthClientBadAuthenticators or
   * radiusAccClientBadAuthenticators
   */
  uint32_t bad_authenticators;

  /**
   * timeouts - radiusAuthClientTimeouts or radiusAccClientTimeouts
   */
  uint32_t timeouts;

  /**
   * unknown_types - radiusAuthClientUnknownTypes or radiusAccClientUnknownTypes
   */
  uint32_t unknown_types;

  /**
   * packets_dropped - radiusAuthClientPacketsDropped or
   * radiusAccClientPacketsDropped
   */
  uint32_t packets_dropped;
};

/**
 * struct hostapd_radius_servers - RADIUS servers for RADIUS client
 */
struct hostapd_radius_servers {
  /**
   * auth_servers - RADIUS Authentication servers in priority order
   */
  struct hostapd_radius_server *auth_servers;

  /**
   * num_auth_servers - Number of auth_servers entries
   */
  int num_auth_servers;

  /**
   * auth_server - The current Authentication server
   */
  struct hostapd_radius_server *auth_server;

  /**
   * acct_servers - RADIUS Accounting servers in priority order
   */
  struct hostapd_radius_server *acct_servers;

  /**
   * num_acct_servers - Number of acct_servers entries
   */
  int num_acct_servers;

  /**
   * acct_server - The current Accounting server
   */
  struct hostapd_radius_server *acct_server;

  /**
   * retry_primary_interval - Retry interval for trying primary server
   *
   * This specifies a retry interval in sexconds for trying to return to
   * the primary RADIUS server. RADIUS client code will automatically try
   * to use the next server when the current server is not replying to
   * requests. If this interval is set (non-zero), the primary server
   * will be retried after the specified number of seconds has passed
   * even if the current used secondary server is still working.
   */
  int retry_primary_interval;

  /**
   * msg_dumps - Whether RADIUS message details are shown in stdout
   */
  int msg_dumps;

  /**
   * client_addr - Client (local) address to use if force_client_addr
   */
  struct hostapd_ip_addr client_addr;

  /**
   * force_client_addr - Whether to force client (local) address
   */
  int force_client_addr;
};

/**
 * RadiusType - RADIUS server type for RADIUS client
 */
typedef enum {
  /**
   * RADIUS authentication
   */
  RADIUS_AUTH,

  /**
   * RADIUS_ACCT - RADIUS accounting
   */
  RADIUS_ACCT,

  /**
   * RADIUS_ACCT_INTERIM - RADIUS interim accounting message
   *
   * Used only with radius_client_send(). This behaves just like
   * RADIUS_ACCT, but removes any pending interim RADIUS Accounting
   * messages for the same STA before sending the new interim update.
   */
  RADIUS_ACCT_INTERIM
} RadiusType;

/**
 * RadiusRxResult - RADIUS client RX handler result
 */
typedef enum {
  /**
   * RADIUS_RX_PROCESSED - Message processed
   *
   * This stops handler calls and frees the message.
   */
  RADIUS_RX_PROCESSED,

  /**
   * RADIUS_RX_QUEUED - Message has been queued
   *
   * This stops handler calls, but does not free the message; the handler
   * that returned this is responsible for eventually freeing the
   * message.
   */
  RADIUS_RX_QUEUED,

  /**
   * RADIUS_RX_UNKNOWN - Message is not for this handler
   */
  RADIUS_RX_UNKNOWN,

  /**
   * RADIUS_RX_INVALID_AUTHENTICATOR - Message has invalid Authenticator
   */
  RADIUS_RX_INVALID_AUTHENTICATOR
} RadiusRxResult;

struct radius_client_data;

int radius_client_register(
    struct radius_client_data *radius, RadiusType msg_type,
    RadiusRxResult (*handler)(struct radius_msg *msg, struct radius_msg *req,
                              const uint8_t *shared_secret,
                              size_t shared_secret_len, void *data),
    void *data);
void radius_client_set_interim_error_cb(struct radius_client_data *radius,
                                        void (*cb)(const uint8_t *addr,
                                                   void *ctx),
                                        void *ctx);
int radius_client_send(struct radius_client_data *radius,
                       struct radius_msg *msg, RadiusType msg_type,
                       const uint8_t *addr);
uint8_t radius_client_get_id(struct radius_client_data *radius);
void radius_client_flush(struct radius_client_data *radius, int only_auth);
struct radius_client_data *
radius_client_init(void *ctx, struct hostapd_radius_servers *conf);
void radius_client_deinit(struct radius_client_data *radius);
void radius_client_flush_auth(struct radius_client_data *radius,
                              const uint8_t *addr);
int radius_client_get_mib(struct radius_client_data *radius, char *buf,
                          size_t buflen);
void radius_client_reconfig(struct radius_client_data *radius,
                            struct hostapd_radius_servers *conf);

#endif /* RADIUS_CLIENT_H */

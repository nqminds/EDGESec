/*
 * Event loop
 * Copyright (c) 2002-2006, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 *
 * This file defines an event loop interface that supports processing events
 * from registered timeouts (i.e., do something after N seconds), sockets
 * (e.g., a new packet available for reading), and signals. eloop.c is an
 * implementation of this interface using select() and sockets. This is
 * suitable for most UNIX/POSIX systems. When porting to other operating
 * systems, it may be necessary to replace that implementation with OS specific
 * mechanisms.
 */

/**
 * @file eloop.h
 * @author Jouni Malinen
 * @brief Event loop.
 */

#ifndef ELOOP_H
#define ELOOP_H

#include "allocs.h"
#include "os.h"
#include "list.h"

/**
 * ELOOP_ALL_CTX - eloop_cancel_timeout() magic number to match all timeouts
 */
#define ELOOP_ALL_CTX (void *)-1

/**
 * eloop_event_type - eloop socket event type for eloop_register_sock()
 * @EVENT_TYPE_READ: Socket has data available for reading
 * @EVENT_TYPE_WRITE: Socket has room for new data to be written
 * @EVENT_TYPE_EXCEPTION: An exception has been reported
 */
typedef enum {
  EVENT_TYPE_READ = 0,
  EVENT_TYPE_WRITE,
  EVENT_TYPE_EXCEPTION
} eloop_event_type;

/**
 * eloop_sock_handler - eloop socket event callback type
 * @sock: File descriptor number for the socket
 * @eloop_ctx: Registered callback context data (eloop_data)
 * @sock_ctx: Registered callback context data (user_data)
 */
typedef void (*eloop_sock_handler)(int sock, void *eloop_ctx, void *sock_ctx);

/**
 * eloop_event_handler - eloop generic event callback type
 * @eloop_ctx: Registered callback context data (eloop_data)
 * @user_ctx: Registered callback context data (user_data)
 */
typedef void (*eloop_event_handler)(void *eloop_ctx, void *user_ctx);

/**
 * eloop_timeout_handler - eloop timeout event callback type
 * @eloop_ctx: Registered callback context data (eloop_data)
 * @user_ctx: Registered callback context data (user_data)
 */
typedef void (*eloop_timeout_handler)(void *eloop_ctx, void *user_ctx);

/**
 * eloop_signal_handler - eloop signal event callback type
 * @sig: Signal number
 * @signal_ctx: Registered callback context data (user_data from
 * eloop_register_signal(), eloop_register_signal_terminate(), or
 * eloop_register_signal_reconfig() call)
 */
// typedef void (*eloop_signal_handler)(int sig, void *signal_ctx);

struct eloop_sock {
  int sock;
  void *eloop_data;
  void *user_data;
  eloop_sock_handler handler;
};

struct eloop_timeout {
  struct dl_list list;
  struct os_reltime time;
  void *eloop_data;
  void *user_data;
  eloop_timeout_handler handler;
};

struct eloop_sock_table {
  int count;
  struct eloop_sock *table;
  eloop_event_type type;
  int changed;
};

struct eloop_data {
  int max_sock;
  int count; /* sum of all table counts */
  int max_fd;
  struct eloop_sock *fd_table;
  int epollfd;
  int epoll_max_event_num;
  struct epoll_event *epoll_events;
  struct eloop_sock_table readers;
  struct eloop_sock_table writers;
  struct eloop_sock_table exceptions;
  struct dl_list timeout;
  int terminate;
};

/**
 * eloop_init() - Initialize global event loop data
 * Returns: struct eloop_data on success, NULL on failure
 *
 * This function must be called before any other eloop_* function.
 */
struct eloop_data *eloop_init(void);

/**
 * eloop_free() - Free's the eloop context
 * @eloop: eloop context
 *
 * This function must be called before any other eloop_* function.
 */
void eloop_free(struct eloop_data *eloop);

/**
 * eloop_register_read_sock - Register handler for read events
 * @eloop: eloop context
 * @sock: File descriptor number for the socket
 * @handler: Callback function to be called when data is available for reading
 * @eloop_data: Callback context data (eloop_ctx)
 * @user_data: Callback context data (sock_ctx)
 * Returns: 0 on success, -1 on failure
 *
 * Register a read socket notifier for the given file descriptor. The handler
 * function will be called whenever data is available for reading from the
 * socket. The handler function is responsible for clearing the event after
 * having processed it in order to avoid eloop from calling the handler again
 * for the same event.
 */
int eloop_register_read_sock(struct eloop_data *eloop, int sock,
                             eloop_sock_handler handler, void *eloop_data,
                             void *user_data);

/**
 * eloop_unregister_read_sock - Unregister handler for read events
 * @eloop: eloop context
 * @sock: File descriptor number for the socket
 *
 * Unregister a read socket notifier that was previously registered with
 * eloop_register_read_sock().
 */
void eloop_unregister_read_sock(struct eloop_data *eloop, int sock);

/**
 * eloop_register_sock - Register handler for socket events
 * @eloop: eloop context
 * @sock: File descriptor number for the socket
 * @type: Type of event to wait for
 * @handler: Callback function to be called when the event is triggered
 * @eloop_data: Callback context data (eloop_ctx)
 * @user_data: Callback context data (sock_ctx)
 * Returns: 0 on success, -1 on failure
 *
 * Register an event notifier for the given socket's file descriptor. The
 * handler function will be called whenever the that event is triggered for the
 * socket. The handler function is responsible for clearing the event after
 * having processed it in order to avoid eloop from calling the handler again
 * for the same event.
 */
int eloop_register_sock(struct eloop_data *eloop, int sock,
                        eloop_event_type type, eloop_sock_handler handler,
                        void *eloop_data, void *user_data);

/**
 * eloop_unregister_sock - Unregister handler for socket events
 * @eloop: eloop context
 * @sock: File descriptor number for the socket
 * @type: Type of event for which sock was registered
 *
 * Unregister a socket event notifier that was previously registered with
 * eloop_register_sock().
 */
void eloop_unregister_sock(struct eloop_data *eloop, int sock,
                           eloop_event_type type);

/**
 * eloop_register_timeout - Register timeout
 * @eloop: eloop context
 * @secs: Number of seconds to the timeout
 * @usecs: Number of microseconds to the timeout
 * @handler: Callback function to be called when timeout occurs
 * @eloop_data: Callback context data (eloop_ctx)
 * @user_data: Callback context data (sock_ctx)
 * Returns: 0 on success, -1 on failure
 *
 * Register a timeout that will cause the handler function to be called after
 * given time.
 */
int eloop_register_timeout(struct eloop_data *eloop, unsigned long secs,
                           unsigned long usecs, eloop_timeout_handler handler,
                           void *eloop_data, void *user_data);

/**
 * eloop_cancel_timeout - Cancel timeouts
 * @eloop: eloop context
 * @handler: Matching callback function
 * @eloop_data: Matching eloop_data or %ELOOP_ALL_CTX to match all
 * @user_data: Matching user_data or %ELOOP_ALL_CTX to match all
 * Returns: Number of cancelled timeouts
 *
 * Cancel matching <handler,eloop_data,user_data> timeouts registered with
 * eloop_register_timeout(). ELOOP_ALL_CTX can be used as a wildcard for
 * cancelling all timeouts regardless of eloop_data/user_data.
 */
int eloop_cancel_timeout(struct eloop_data *eloop,
                         eloop_timeout_handler handler, void *eloop_data,
                         void *user_data);

/**
 * eloop_cancel_timeout_one - Cancel a single timeout
 * @eloop: eloop context
 * @handler: Matching callback function
 * @eloop_data: Matching eloop_data
 * @user_data: Matching user_data
 * @remaining: Time left on the cancelled timer
 * Returns: Number of cancelled timeouts
 *
 * Cancel matching <handler,eloop_data,user_data> timeout registered with
 * eloop_register_timeout() and return the remaining time left.
 */
int eloop_cancel_timeout_one(struct eloop_data *eloop,
                             eloop_timeout_handler handler, void *eloop_data,
                             void *user_data, struct os_reltime *remaining);

/**
 * eloop_is_timeout_registered - Check if a timeout is already registered
 * @eloop: eloop context
 * @handler: Matching callback function
 * @eloop_data: Matching eloop_data
 * @user_data: Matching user_data
 * Returns: 1 if the timeout is registered, 0 if the timeout is not registered
 *
 * Determine if a matching <handler,eloop_data,user_data> timeout is registered
 * with eloop_register_timeout().
 */
int eloop_is_timeout_registered(struct eloop_data *eloop,
                                eloop_timeout_handler handler, void *eloop_data,
                                void *user_data);

/**
 * eloop_deplete_timeout - Deplete a timeout that is already registered
 * @eloop: eloop context
 * @req_secs: Requested number of seconds to the timeout
 * @req_usecs: Requested number of microseconds to the timeout
 * @handler: Matching callback function
 * @eloop_data: Matching eloop_data
 * @user_data: Matching user_data
 * Returns: 1 if the timeout is depleted, 0 if no change is made, -1 if no
 * timeout matched
 *
 * Find a registered matching <handler,eloop_data,user_data> timeout. If found,
 * deplete the timeout if remaining time is more than the requested time.
 */
int eloop_deplete_timeout(struct eloop_data *eloop, unsigned long req_secs,
                          unsigned long req_usecs,
                          eloop_timeout_handler handler, void *eloop_data,
                          void *user_data);

/**
 * eloop_replenish_timeout - Replenish a timeout that is already registered
 * @eloop: eloop context
 * @req_secs: Requested number of seconds to the timeout
 * @req_usecs: Requested number of microseconds to the timeout
 * @handler: Matching callback function
 * @eloop_data: Matching eloop_data
 * @user_data: Matching user_data
 * Returns: 1 if the timeout is replenished, 0 if no change is made, -1 if no
 * timeout matched
 *
 * Find a registered matching <handler,eloop_data,user_data> timeout. If found,
 * replenish the timeout if remaining time is less than the requested time.
 */
int eloop_replenish_timeout(struct eloop_data *eloop, unsigned long req_secs,
                            unsigned long req_usecs,
                            eloop_timeout_handler handler, void *eloop_data,
                            void *user_data);

/**
 * eloop_sock_requeue - Requeue sockets
 * @eloop: eloop context
 * Requeue sockets after forking because some implementations require this,
 * such as epoll and kqueue.
 */
int eloop_sock_requeue(struct eloop_data *eloop);

/**
 * eloop_run - Start the event loop
 * @eloop: eloop context
 * Start the event loop and continue running as long as there are any
 * registered event handlers. This function is run after event loop has been
 * initialized with event_init() and one or more events have been registered.
 */
void eloop_run(struct eloop_data *eloop);

/**
 * eloop_terminate - Terminate event loop
 * @eloop: eloop context
 * Terminate event loop even if there are registered events. This can be used
 * to request the program to be terminated cleanly.
 */
void eloop_terminate(struct eloop_data *eloop);

/**
 * eloop_terminated - Check whether event loop has been terminated
 * @eloop: eloop context
 * Returns: 1 = event loop terminate, 0 = event loop still running
 *
 * This function can be used to check whether eloop_terminate() has been called
 * to request termination of the event loop. This is normally used to abort
 * operations that may still be queued to be run when eloop_terminate() was
 * called.
 */
int eloop_terminated(struct eloop_data *eloop);

#endif /* ELOOP_H */

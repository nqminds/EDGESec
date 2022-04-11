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
 * @file sqlite_pcap_writer.c
 * @author Alexandru Mereacre
 * @brief File containing the implementation of the sqlite pcap writer utilities.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sqlite3.h>

#include "sqlite_pcap_writer.h"

#include "../utils/allocs.h"
#include "../utils/os.h"
#include "../utils/log.h"
#include "../utils/sqliteu.h"

void free_sqlite_pcap_db(sqlite3 *db)
{
  if (db != NULL) {
    sqlite3_close(db);
  }
}

int open_sqlite_pcap_db(char *db_path, sqlite3** sql)
{
  sqlite3 *db = NULL;
  int rc;

  if ((rc = sqlite3_open(db_path, &db)) != SQLITE_OK) {
    log_debug("Cannot open database: %s %s", sqlite3_errmsg(db), db_path);
    sqlite3_close(db);
    return -1;
  }

  *sql = db;

  rc = check_table_exists(db, PCAP_TABLE_NAME);

  if (rc == 0) {
    log_debug("pcap table doesn't exist creating...");
    if (execute_sqlite_query(db, PCAP_CREATE_TABLE) < 0) {
      log_debug("execute_sqlite_query fail");
      free_sqlite_pcap_db(db);
      return -1;
    }
  } else if (rc < 0) {
    log_debug("check_table_exists fail");
    free_sqlite_pcap_db(db);
    return -1;
  }

  return 0;
}

int save_sqlite_pcap_entry(sqlite3 *db, char *name, uint64_t timestamp, uint32_t caplen, uint32_t length,
                           char *interface, char *filter)
{
  sqlite3_stmt *res = NULL;
  int column_idx;

  if (sqlite3_prepare_v2(db, PCAP_INSERT_INTO, -1, &res, 0) != SQLITE_OK) {
    log_trace("Failed to prepare statement: %s", sqlite3_errmsg(db));
    return -1;
  }

  column_idx = sqlite3_bind_parameter_index(res, "@timestamp");
  if(sqlite3_bind_int64(res, column_idx, timestamp) != SQLITE_OK) {
    log_trace("sqlite3_bind_int64 fail");
    sqlite3_finalize(res);
    return -1;
  }

  column_idx = sqlite3_bind_parameter_index(res, "@caplen");
  if (sqlite3_bind_int64(res, column_idx, caplen) != SQLITE_OK) {
    log_trace("sqlite3_bind_int64 fail");
    sqlite3_finalize(res);
    return -1;
  }

  column_idx = sqlite3_bind_parameter_index(res, "@length");
  if (sqlite3_bind_int64(res, column_idx, length) != SQLITE_OK) {
    log_trace("sqlite3_bind_int64 fail");
    sqlite3_finalize(res);
    return -1;
  }

  column_idx = sqlite3_bind_parameter_index(res, "@name");
  if (sqlite3_bind_text(res, column_idx, name, -1, NULL) != SQLITE_OK) {
    log_trace("sqlite3_bind_text fail");
    sqlite3_finalize(res);
    return -1;
  }

  column_idx = sqlite3_bind_parameter_index(res, "@interface");
  if (sqlite3_bind_text(res, column_idx, interface, -1, NULL) != SQLITE_OK) {
    log_trace("sqlite3_bind_text fail");
    sqlite3_finalize(res);
    return -1;
  }

  column_idx = sqlite3_bind_parameter_index(res, "@filter");
  if (sqlite3_bind_text(res, column_idx, filter, -1, NULL) != SQLITE_OK) {
    log_trace("sqlite3_bind_text fail");
    sqlite3_finalize(res);
    return -1;
  }

  sqlite3_step(res);
  sqlite3_finalize(res);

  return 0;
}

int get_first_pcap_entry(sqlite3 *db, uint64_t *timestamp, uint64_t *caplen)
{
  int rc;
  sqlite3_stmt *res = NULL;

  if (sqlite3_prepare_v2(db, PCAP_SELECT_FIRST_ENTRY, -1, &res, 0) != SQLITE_OK) {
    log_trace("Failed to prepare statement: %s", sqlite3_errmsg(db));
    return -1;
  }

  *timestamp = 0;
  *caplen = 0;

  rc = sqlite3_step(res);

  if(rc == SQLITE_ROW) {
    *timestamp = sqlite3_column_int64(res, 0);
    *caplen = sqlite3_column_int64(res, 1);
  } else if (rc == SQLITE_OK || rc == SQLITE_DONE) {
    log_trace("No rows");
    sqlite3_finalize(res);
    return 1;
  } else {
    log_trace("sqlite3_step fail");
    sqlite3_finalize(res);
    return -1;
  }

  sqlite3_finalize(res);
  return 0;
}

int sum_pcap_group(sqlite3 *db, uint64_t lt, uint32_t lim, uint64_t *ht, uint64_t *sum)
{
  int rc;
  sqlite3_stmt *res = NULL;
  int column_idx;

  *sum = 0;

  if (sqlite3_prepare_v2(db, PCAP_SUM_GROUP, -1, &res, 0) != SQLITE_OK) {
    log_trace("Failed to prepare statement: %s", sqlite3_errmsg(db));
    return -1;
  }

  column_idx = sqlite3_bind_parameter_index(res, "@lt");
  if(sqlite3_bind_int64(res, column_idx, lt) != SQLITE_OK) {
    log_trace("sqlite3_bind_int64 fail");
    sqlite3_finalize(res);
    return -1;
  }

  column_idx = sqlite3_bind_parameter_index(res, "@lim");
  if(sqlite3_bind_int64(res, column_idx, lim) != SQLITE_OK) {
    log_trace("sqlite3_bind_int64 fail");
    sqlite3_finalize(res);
    return -1;
  }

  while ((rc = sqlite3_step(res)) == SQLITE_ROW) {
    *ht = sqlite3_column_int64(res, 0);
    *sum = *sum + sqlite3_column_int64(res, 1);
  }

  if (rc != SQLITE_OK && rc != SQLITE_DONE) {
    log_trace("sqlite3_step fail");
    sqlite3_finalize(res);
    return -1;
  }


  sqlite3_finalize(res);
  return 0;
}

int get_pcap_meta_array(sqlite3 *db, uint64_t lt, uint32_t lim, UT_array *pcap_meta_arr)
{
  int rc;
  struct pcap_file_meta p;
  sqlite3_stmt *res = NULL;
  int column_idx;

  if (sqlite3_prepare_v2(db, PCAP_SELECT_GROUP, -1, &res, 0) != SQLITE_OK) {
    log_trace("Failed to prepare statement: %s", sqlite3_errmsg(db));
    return -1;
  }

  column_idx = sqlite3_bind_parameter_index(res, "@lt");
  if(sqlite3_bind_int64(res, column_idx, lt) != SQLITE_OK) {
    log_trace("sqlite3_bind_int64 fail");
    sqlite3_finalize(res);
    return -1;
  }

  column_idx = sqlite3_bind_parameter_index(res, "@lim");
  if(sqlite3_bind_int64(res, column_idx, lim) != SQLITE_OK) {
    log_trace("sqlite3_bind_int64 fail");
    sqlite3_finalize(res);
    return -1;
  }

  while ((rc = sqlite3_step(res)) == SQLITE_ROW) {
    os_memset(&p, 0, sizeof(struct pcap_file_meta));
    p.timestamp = sqlite3_column_int64(res, 0);
    p.name = os_strdup((char*) sqlite3_column_text(res, 1));
    utarray_push_back(pcap_meta_arr, &p);
  }

  if (rc != SQLITE_OK && rc != SQLITE_DONE) {
    log_trace("sqlite3_step fail");
    sqlite3_finalize(res);
    return -1;
  }


  sqlite3_finalize(res);
  return 0;
}

int delete_pcap_entries(sqlite3 *db, uint64_t lt, uint64_t ht)
{
  int rc;
  sqlite3_stmt *res = NULL;
  int column_idx;

  if (sqlite3_prepare_v2(db, PCAP_DELETE_GROUP, -1, &res, 0) != SQLITE_OK) {
    log_trace("Failed to prepare statement: %s", sqlite3_errmsg(db));
    return -1;
  }

  column_idx = sqlite3_bind_parameter_index(res, "@lt");
  if(sqlite3_bind_int64(res, column_idx, lt) != SQLITE_OK) {
    log_trace("sqlite3_bind_int64 fail");
    sqlite3_finalize(res);
    return -1;
  }

  column_idx = sqlite3_bind_parameter_index(res, "@ht");
  if(sqlite3_bind_int64(res, column_idx, ht) != SQLITE_OK) {
    log_trace("sqlite3_bind_int64 fail");
    sqlite3_finalize(res);
    return -1;
  }

  rc = sqlite3_step(res);

  if (rc != SQLITE_OK && rc != SQLITE_DONE) {
    sqlite3_finalize(res);
    return -1;
  }

  sqlite3_finalize(res);
  return 0;
}
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
 * @file sqlite_crypt_writer.c
 * @author Alexandru Mereacre 
 * @brief File containing the implementation of the sqlite crypt writer utilities.
 */

#include <stdint.h>
#include <sqlite3.h>

#include "sqlite_crypt_writer.h"

#include "../utils/os.h"
#include "../utils/log.h"
#include "../utils/sqliteu.h"

int open_sqlite_crypt_db(char *db_path, sqlite3** sql)
{
  sqlite3 *db = NULL;
  int rc;
  
  if ((rc = sqlite3_open(db_path, &db)) != SQLITE_OK) {     
    log_debug("Cannot open database: %s", sqlite3_errmsg(db));
    sqlite3_close(db);
    return -1;
  }

  *sql = db;

  rc = check_table_exists(db, CRYPT_STORE_TABLE_NAME);

  if (rc == 0) {
    log_debug("%s table doesn't exist creating...", CRYPT_STORE_TABLE_NAME);
    if (execute_sqlite_query(db, CRYPT_STORE_CREATE_TABLE) < 0) {
      log_debug("execute_sqlite_query fail");
      free_sqlite_crypt_db(db);
      return -1;
    }
  } else if (rc < 0) {
    log_debug("check_table_exists fail");
    free_sqlite_crypt_db(db);
    return -1;
  }

  rc = check_table_exists(db, CRYPT_SECRETS_TABLE_NAME);

  if (rc == 0) {
    log_debug("%s table doesn't exist creating...", CRYPT_SECRETS_TABLE_NAME);
    if (execute_sqlite_query(db, CRYPT_SECRETS_CREATE_TABLE) < 0) {
      log_debug("execute_sqlite_query fail");
      free_sqlite_crypt_db(db);
      return -1;
    }
  } else if (rc < 0) {
    log_debug("check_table_exists fail");
    free_sqlite_crypt_db(db);
    return -1;
  }

  return 0;
}

void free_sqlite_crypt_db(sqlite3 *db)
{
  if (db != NULL) {
    sqlite3_close(db);
  }
}

int save_sqlite_store_entry(sqlite3 *db, char *key, char *value, char *id)
{
  sqlite3_stmt *res = NULL;
  int column_idx;

  if (sqlite3_prepare_v2(db, CRYPT_STORE_INSERT_INTO, -1, &res, 0) != SQLITE_OK) {
    log_trace("Failed to prepare statement: %s", sqlite3_errmsg(db));
    return -1;
  }

  column_idx = sqlite3_bind_parameter_index(res, "@key");
  if (sqlite3_bind_text(res, column_idx, key, -1, NULL) != SQLITE_OK) {
    log_trace("sqlite3_bind_text fail");
    sqlite3_finalize(res);
    return -1;
  }

  column_idx = sqlite3_bind_parameter_index(res, "@value");
  if (sqlite3_bind_text(res, column_idx, value, -1, NULL) != SQLITE_OK) {
    log_trace("sqlite3_bind_text fail");
    sqlite3_finalize(res);
    return -1;
  }

  column_idx = sqlite3_bind_parameter_index(res, "@id");
  if (sqlite3_bind_text(res, column_idx, id, -1, NULL) != SQLITE_OK) {
    log_trace("sqlite3_bind_text fail");
    sqlite3_finalize(res);
    return -1;
  }

  sqlite3_step(res);
  sqlite3_finalize(res);

  return 0;
}

int save_sqlite_secrets_entry(sqlite3 *db, char *id, char *value)
{
  sqlite3_stmt *res = NULL;
  int column_idx;

  if (sqlite3_prepare_v2(db, CRYPT_SECRETS_INSERT_INTO, -1, &res, 0) != SQLITE_OK) {
    log_trace("Failed to prepare statement: %s", sqlite3_errmsg(db));
    return -1;
  }

  column_idx = sqlite3_bind_parameter_index(res, "@id");
  if (sqlite3_bind_text(res, column_idx, id, -1, NULL) != SQLITE_OK) {
    log_trace("sqlite3_bind_text fail");
    sqlite3_finalize(res);
    return -1;
  }

  column_idx = sqlite3_bind_parameter_index(res, "@value");
  if (sqlite3_bind_text(res, column_idx, value, -1, NULL) != SQLITE_OK) {
    log_trace("sqlite3_bind_text fail");
    sqlite3_finalize(res);
    return -1;
  }

  sqlite3_step(res);
  sqlite3_finalize(res);

  return 0;
}

void free_sqlite_store_row(struct store_row *row)
{
  if (row != NULL) {
    if (row->key != NULL)
      os_free(row->key);
    if (row->value != NULL)
      os_free(row->value);
    if (row->id != NULL)
      os_free(row->id);

    os_free(row);
  }
}

struct store_row* get_sqlite_store_row(sqlite3 *db, char *key)
{
  struct store_row *row;
  sqlite3_stmt *res;
  char *sql = CRYPT_STORE_GET;
  int rc;
  
  if (key == NULL) {
    log_trace("key param is NULL");
    return NULL;
  }

  row = (struct store_row *)os_malloc(sizeof(struct store_row));

  rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);


  if (rc == SQLITE_OK)
    sqlite3_bind_text(res, 1, key, -1, NULL);
  else {
    log_trace("Failed to execute statement: %s", sqlite3_errmsg(db));
    free_sqlite_store_row(row);
    return NULL;
  }

  log_trace("%s", sql);
  rc = sqlite3_step(res);

  if (rc == SQLITE_ROW) {
    row->key = os_strdup(key);
    row->value = os_strdup(sqlite3_column_text(res, 0));
    row->id = os_strdup(sqlite3_column_text(res, 1));
    sqlite3_finalize(res);
    return row;
  }

  return NULL;
}

void free_sqlite_secrets_row(struct secrets_row *row)
{
  if (row != NULL) {
    if (row->id != NULL)
      os_free(row->id);
    if (row->value != NULL)
      os_free(row->value);
    os_free(row);
  }
}


struct secrets_row* get_sqlite_secrets_row(sqlite3 *db, char *id)
{
  struct secrets_row *row;
  sqlite3_stmt *res;
  char *sql = CRYPT_SECRETS_GET;
  int rc;
  
  if (id == NULL) {
    log_trace("id param is NULL");
    return NULL;
  }

  row = (struct secrets_row *)os_malloc(sizeof(struct secrets_row));

  rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);

  if (rc == SQLITE_OK)
    sqlite3_bind_text(res, 1, id, -1, NULL);
  else {
    log_trace("Failed to execute statement: %s", sqlite3_errmsg(db));
    free_sqlite_secrets_row(row);
    return NULL;
  }

  log_trace("%s", sql);
  rc = sqlite3_step(res);

  if (rc == SQLITE_ROW) {
    row->id = os_strdup(id);
    row->value = os_strdup(sqlite3_column_text(res, 0));
    sqlite3_finalize(res);
    return row;
  }

  return NULL;
}

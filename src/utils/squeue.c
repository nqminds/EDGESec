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
 * @file squeue.c
 * @author Alexandru Mereacre 
 * @brief File containing the implementation of the string queue utilities.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "squeue.h"
#include "allocs.h"
#include "os.h"
#include "log.h"

struct string_queue* init_string_queue(ssize_t max_length)
{
  struct string_queue *queue;
  queue = os_zalloc(sizeof(*queue));

  if (queue == NULL) {
    log_err("os_zalloc");
    return NULL;
  }

  queue->max_length = max_length;

  dl_list_init(&queue->list);

  return queue;
}

int push_string_queue(struct string_queue* queue, char *str)
{
  struct string_queue* el;
  ssize_t max_length;

  if (str == NULL) {
    log_debug("str param is NULL");
    return -1;
  }

  if (queue == NULL) {
    log_debug("queue param is NULL");
    return -1;
  }

  max_length = queue->max_length;

  if ((el = init_string_queue(queue->max_length)) == NULL) {
    log_debug("init_string_queue fail");
    return -1;
  }

  el->str = os_strdup(str);

  dl_list_add_tail(&queue->list, &el->list);

  if (get_string_queue_length(queue) > max_length && max_length >= 0) {
    el = dl_list_first(&queue->list, struct string_queue, list);
    free_string_queue_el(el);
  }

  return 0;
}

void free_string_queue_el(struct string_queue* el)
{
  if (el != NULL) {
    dl_list_del(&el->list);
    os_free(el->str);
	  os_free(el);
  }
}

int peek_string_queue(struct string_queue* queue, char **str)
{
  struct string_queue* el = NULL;
  *str = NULL;

  if (queue == NULL)
    return -1;

  el = dl_list_first(&queue->list, struct string_queue, list);

  if (el != NULL)
    *str = os_strdup(el->str);

  return 0;
}

int pop_string_queue(struct string_queue* queue, char **str)
{
  struct string_queue* el = NULL;
  int ret = peek_string_queue(queue, str);

  if (ret == 0) {
    el = dl_list_first(&queue->list, struct string_queue, list);
    free_string_queue_el(el);
  }

  return ret;
}

void empty_string_queue(struct string_queue* queue, ssize_t count)
{
  struct string_queue* el;
  ssize_t num = 0;

  while ((el = dl_list_first(&queue->list, struct string_queue, list)) != NULL &&
         ((num < count && count > 0) || count < 0))
  {
    free_string_queue_el(el);
    num ++;
  }
}

void free_string_queue(struct string_queue* queue)
{
  empty_string_queue(queue, -1);
  free_string_queue_el(queue);
}

ssize_t get_string_queue_length(struct string_queue* queue)
{
  return (queue != NULL) ? dl_list_len(&queue->list) : 0;
}

char* concat_string_queue(struct string_queue *queue, ssize_t count)
{
  struct string_queue *el;
  char *concat_str = NULL;
  ssize_t size = 1, num = 0;

  dl_list_for_each(el, &queue->list, struct string_queue, list) {
    if (el != NULL && ((num < count && count > 0) || count < 0)) {
      size += strlen(el->str);
      if (concat_str == NULL) {
        concat_str = os_zalloc(size);
      } else concat_str = os_realloc(concat_str, size);

      if (concat_str == NULL) {
        log_err("os_malloc");
        return NULL;
      }

      strcat(concat_str, el->str);
    }
    num ++;
  }

  // // Process all strings in the queue
  // while(get_string_queue_length(queue)) {
  //   if ((el = pop_string_queue(queue)) != NULL) {
  //     size += strlen(el->str);
  //     if (concat_str == NULL) {
  //       concat_str = os_zalloc(size);
  //     } else concat_str = os_realloc(concat_str, size);

  //     if (concat_str == NULL) {
  //       log_err("os_malloc");
  //       return NULL;
  //     }

  //     strcat(concat_str, el->str);

  //     // Process string element
  //     free_string_queue_el(el);
  //   }
  // }

  return concat_str;
}

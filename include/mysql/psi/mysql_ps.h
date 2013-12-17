/* Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef MYSQL_PS_H
#define MYSQL_PS_H

/**
  @file mysql/psi/mysql_ps.h
  Instrumentation helpers for prepared statements.
*/

#include "mysql/psi/psi.h"

#ifndef PSI_PS_CALL
#define PSI_PS_CALL(M) PSI_DYNAMIC_CALL(M)                               
#endif    

#ifdef HAVE_PSI_PS_INTERFACE
  #define MYSQL_CREATE_PS(IDENTITY, LOCKER, SQLTEXT, SQLTEXT_LENGTH) \
    inline_mysql_create_prepared_stmt(IDENTITY, LOCKER, SQLTEXT, SQLTEXT_LENGTH)
  #define MYSQL_START_PS(PS_STATE, PREPARED_STMT) \
    inline_mysql_start_prepare_stmt(PS_STATE, PREPARED_STMT)
  #define MYSQL_START_PS_EXECUTE(PS_STATE, PREPARED_STMT) \
    inline_mysql_start_prepared_stmt_execute(PS_STATE, PREPARED_STMT) 
  #define MYSQL_DESTROY_PS(PREPARED_STMT) \
    inline_mysql_destroy_prepared_stmt(PREPARED_STMT)
#else
  #define MYSQL_CREATE_PS(IDENTITY, LOCKER, SQLTEXT, SQLTEXT_LENGTH) \
    do {} while (0)
  #define MYSQL_START_PS(PS_STATE, PREPARED_STMT) \
    do {} while (0)
  #define MYSQL_START_PS_EXECUTE(PS_STATE, PREPARED_STMT) \
    do {} while (0)
  #define MYSQL_DESTROY_PS(PREPARED_STMT) \
    do {} while (0)
#endif


#ifdef HAVE_PSI_PS_INTERFACE
  #define MYSQL_END_PS(LOCKER) \
    inline_mysql_end_prepare_stmt(LOCKER)
  #define MYSQL_END_PS_EXECUTE(LOCKER) \
    inline_mysql_end_prepared_stmt_execute(LOCKER)
#else
  #define MYSQL_END_PS(LOCKER) \
    do {} while (0)
  #define MYSQL_END_PS_EXECUTE(LOCKER) \
    do {} while (0)
#endif

#ifdef HAVE_PSI_PS_INTERFACE
static inline struct PSI_prepared_stmt*
inline_mysql_create_prepared_stmt(void *identity,
                                  PSI_statement_locker *locker,
                                  char *sqltext, uint sqltext_length)
{
  return PSI_PS_CALL(create_prepared_stmt)(identity, locker,
                                           sqltext, sqltext_length);
}

static inline struct PSI_prepared_stmt_locker*
inline_mysql_start_prepare_stmt(PSI_prepared_stmt_locker_state *state,
                                PSI_prepared_stmt* prepared_stmt)
{
  return PSI_PS_CALL(start_prepare_stmt)(state, prepared_stmt);
}

static inline void 
inline_mysql_end_prepare_stmt(PSI_prepared_stmt_locker *locker)
{
  if (likely(locker != NULL))
    PSI_PS_CALL(end_prepare_stmt)(locker);
}

static inline struct PSI_prepared_stmt_locker*
inline_mysql_start_prepared_stmt_execute(PSI_prepared_stmt_locker_state *state,
                                         PSI_prepared_stmt* prepared_stmt)
{
  return PSI_PS_CALL(start_prepared_stmt_execute)(state, prepared_stmt);
}

static inline void 
inline_mysql_end_prepared_stmt_execute(PSI_prepared_stmt_locker *locker)
{
  if (likely(locker != NULL))
    PSI_PS_CALL(end_prepared_stmt_execute)(locker);
}

static inline void 
inline_mysql_destroy_prepared_stmt(PSI_prepared_stmt *prepared_stmt)
{
  if (prepared_stmt != NULL)
    PSI_PS_CALL(destroy_prepared_stmt)(prepared_stmt);
}
#endif

#endif

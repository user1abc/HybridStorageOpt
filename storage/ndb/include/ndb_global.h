/*
   Copyright (C) 2003 MySQL AB, 2009 Sun Microsystems, Inc.

    All rights reserved. Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDB_GLOBAL_H
#define NDB_GLOBAL_H

#include <my_global.h>
#include <mysql_com.h>
#include <ndb_types.h>

#ifndef NDB_PORT
/* Default port used by ndb_mgmd */
#define NDB_PORT 1186
#endif

#if defined(_WIN32) || defined(_WIN64) || defined(__WIN32__) || defined(WIN32)
#define NDB_WIN32 1
#define NDB_WIN 1
#define PATH_MAX 256
#define DIR_SEPARATOR "\\"
#define MYSQLCLUSTERDIR "c:\\mysql\\mysql-cluster"
#define HAVE_STRCASECMP
#pragma warning(disable: 4503 4786)
#else
#undef NDB_WIN32
#undef NDB_WIN
#define DIR_SEPARATOR "/"
#endif

#if ! (NDB_SIZEOF_CHAR == SIZEOF_CHAR)
#error "Invalid define for Uint8"
#endif

#if ! (NDB_SIZEOF_INT == SIZEOF_INT)
#error "Invalid define for Uint32"
#endif

#if ! (NDB_SIZEOF_LONG_LONG == SIZEOF_LONG_LONG)
#error "Invalid define for Uint64"
#endif

#ifdef _AIX
#undef _H_STRINGS
#endif
#include <m_string.h>
#include <m_ctype.h>
#include <ctype.h>

#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif

#ifdef TIME_WITH_SYS_TIME
#include <time.h>
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif

#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif

#ifndef HAVE_STRDUP
extern char * strdup(const char *s);
#endif

#ifndef HAVE_STRCASECMP
extern int strcasecmp(const char *s1, const char *s2);
extern int strncasecmp(const char *s1, const char *s2, size_t n);
#endif

static const char table_name_separator =  '/';

#if defined(_AIX) || defined(WIN32) || defined(NDB_VC98)
#define STATIC_CONST(x) enum { x }
#else
#define STATIC_CONST(x) static const Uint32 x
#endif

#ifdef  __cplusplus
extern "C" {
#endif
	
#include <assert.h>

#ifdef  __cplusplus
}
#endif

#include "ndb_init.h"

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

#if defined(_lint) || defined(FORCE_INIT_OF_VARS)
#define LINT_SET_PTR = {0,0}
#else
#define LINT_SET_PTR
#endif

#ifndef MIN
#define MIN(x,y) (((x)<(y))?(x):(y))
#endif

#ifndef MAX
#define MAX(x,y) (((x)>(y))?(x):(y))
#endif

#define NDB_O_DIRECT_WRITE_ALIGNMENT 512

#ifndef STATIC_ASSERT
#if defined VM_TRACE
/**
 * Compile-time assert for use from procedure body
 * Zero length array not allowed in C
 * Add use of array to avoid compiler warning
 */
#define STATIC_ASSERT(expr) { char static_assert[(expr)? 1 : 0] = {'\0'}; if (static_assert[0]) {}; }
#else
#define STATIC_ASSERT(expr)
#endif
#endif

#define NDB_ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))


/*
  NDB_STATIC_ASSERT(expr)
   - Check coding assumptions during compile time
     by laying out code that will generate a compiler error
     if the expression is false.
*/

#if (_MSC_VER > 1500) || (defined __GXX_EXPERIMENTAL_CXX0X__)

/*
  Prefer to use the 'static_assert' function from C++0x
  to get best error message
*/
#define NDB_STATIC_ASSERT(expr) static_assert(expr, #expr)

#else

/*
  Fallback to use home grown solution
*/

#define STR_CONCAT_(x, y) x##y
#define STR_CONCAT(x, y) STR_CONCAT_(x, y)

#define NDB_STATIC_ASSERT(expr) \
  enum {STR_CONCAT(static_assert_, __LINE__) = 1 / (!!(expr)) }

#undef STR_CONCAT_
#undef STR_CONCAT

#endif


#if (_MSC_VER > 1500) || (defined __GXX_EXPERIMENTAL_CXX0X__)
#define HAVE_COMPILER_TYPE_TRAITS
#endif

#ifdef HAVE_COMPILER_TYPE_TRAITS
#define ASSERT_TYPE_HAS_CONSTRUCTOR(x)     \
  NDB_STATIC_ASSERT(!__has_trivial_constructor(x))
#else
#define ASSERT_TYPE_HAS_CONSTRUCTOR(x)
#endif

/**
 * visual studio is stricter than gcc for __is_pod, settle for __has_trivial_constructor
 *  until we really really made all signal data classes POD
 */
#if (_MSC_VER > 1500)
#define NDB_ASSERT_POD(x) \
  NDB_STATIC_ASSERT(__has_trivial_constructor(x))
#elif defined __GXX_EXPERIMENTAL_CXX0X__
#define NDB_ASSERT_POD(x) \
  NDB_STATIC_ASSERT(__is_pod(x))
#else
#define NDB_ASSERT_POD(x)
#endif

/*
 * require is like a normal assert, only it's always on (eg. in release)
 */
C_MODE_START
/** see below */
typedef int(*RequirePrinter)(const char *fmt, ...);
void require_failed(int exitcode, RequirePrinter p,
                    const char* expr, const char* file, int line);
int ndbout_printer(const char * fmt, ...);
C_MODE_END
/*
 *  this allows for an exit() call if exitcode is not zero
 *  and takes a Printer to print the error
 */
#define require_exit_or_core_with_printer(v, exitcode, printer) \
  do { if (likely(!(!(v)))) break;                                    \
       require_failed((exitcode), (printer), #v, __FILE__, __LINE__); \
  } while (0)

/*
 *  this allows for an exit() call if exitcode is not zero
*/
#define require_exit_or_core(v, exitcode) \
       require_exit_or_core_with_printer((v), (exitcode), 0)

/*
 * this require is like a normal assert.  (only it's always on)
*/
#define require(v) require_exit_or_core_with_printer((v), 0, 0)

#endif

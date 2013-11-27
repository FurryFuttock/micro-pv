/* -*-  Mode:C; c-basic-offset:4; tab-width:4 -*-
 ****************************************************************************
 * (C) 2003 - Rolf Neugebauer - Intel Research Cambridge
 ****************************************************************************
 *
 *        File: types.h
 *      Author: Rolf Neugebauer (neugebar@dcs.gla.ac.uk)
 *     Changes:
 *
 *        Date: May 2003
 *
 * Environment: Xen Minimal OS
 * Description: a random collection of type definitions
 *
 ****************************************************************************
 * $Id: h-insert.h,v 1.4 2002/11/08 16:03:55 rn Exp $
 ****************************************************************************
 */

#ifndef _TYPES_H_
#define _TYPES_H_
#include <stddef.h>

#include "arch_limits.h"

/* FreeBSD compat types */
typedef unsigned char       u_char;
typedef unsigned int        u_int;
typedef unsigned long       u_long;
typedef long                quad_t;
typedef unsigned long       u_quad_t;
typedef struct {unsigned long pte; } pte_t;

typedef unsigned long       uintptr_t;
typedef long                intptr_t;
typedef unsigned char uint8_t;
typedef   signed char int8_t;
typedef unsigned short uint16_t;
typedef   signed short int16_t;
typedef unsigned int uint32_t;
typedef   signed int int32_t;
typedef   signed long int64_t;
typedef unsigned long uint64_t;
typedef uint64_t uintmax_t;
typedef  int64_t intmax_t;
typedef uint64_t off_t;
typedef intptr_t            ptrdiff_t;
typedef long ssize_t;

#endif /* _TYPES_H_ */

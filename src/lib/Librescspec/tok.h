/*
 *  Copyright (C) 2003-2016 Altair Engineering, Inc. All rights reserved.
 *
 *  ALTAIR ENGINEERING INC. Proprietary and Confidential. Contains Trade Secret
 *  Information. Not for use or disclosure outside ALTAIR and its licensed
 *  clients. Information contained herein shall not be decompiled, disassembled,
 *  duplicated or disclosed in whole or in part for any purpose. Usage of the
 *  software is only as explicitly permitted in the end user software license
 *  agreement.
 *
 *  Copyright notice does not imply publication.
 *
 */
#include <pbs_config.h>   /* the master config generated by configure */

#ifndef TOK_H
#define TOK_H

#include "pbs_rescspec.h"

/* prototypes */
int make_token(int);
char *trans_sym_name(int code);

#ifndef YYSTYPE
#define YYSTYPE rescspec *
#endif

#include "y.tab.h"

#endif

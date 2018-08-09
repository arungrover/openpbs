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
#ifndef	_PARSER_H
#define	_PARSER_H
#ifdef	__cplusplus
extern "C" {
#endif

#include "pbs_rescspec.h"

rescspec *make_tree(rescspec *root, rescspec *left, rescspec *right);
rescspec *tree_finished(rescspec *root);

#ifdef	__cplusplus
}
#endif
#endif	/* _PARSER_H */

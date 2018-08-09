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
#ifndef	_PBS_RESCSPEC_H
#define	_PBS_RESCSPEC_H
#ifdef	__cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <pbs_ifl.h>

/* parse tree */
struct rescspec;

typedef struct rescspec rescspec;

struct rescspec
{
	int token_code;		/* numeric code of the token i.e. ">" -> 257 */
	char *lex_info;		/* the string of the token i.e. ">" */
	int char_num;			/* number of chars to token */
	rescspec *parent;		/* the parent of the node in the parse tree */
	rescspec *left_child;		/* the left child of the node */
	rescspec *right_child;	/* the right child of the node */
};

/* prototypes */
rescspec *rescspec_parse(char *str_spec);
struct batch_status *rescspec_get_resources(rescspec *spec);
struct batch_status *rescspec_get_assignments(rescspec *spec);
int rescspec_evaluate(rescspec *spec, struct attrl *list, char *logbuf);
rescspec *new_rescspec();
void free_rescspec(rescspec *root);
rescspec *dup_rescspec(rescspec *rspec);
void rescspec_print_errors(int p);


/* test functions */
void print_attrl(struct attrl *list);
void print_rescspec_tree(rescspec *root, FILE *fp);

#ifdef	__cplusplus
}
#endif
#endif	/* _PBS_RESCSPEC_H */

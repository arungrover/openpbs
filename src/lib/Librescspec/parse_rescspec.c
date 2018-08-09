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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <pbs_ifl.h>
#include "tok.h"
#include "pbs_rescspec.h"
#include "parser.h"
#include "pbs_share.h"

#define YYSTYPE rescspec *

extern int yydebug;

/* static prototypes */
static struct attrl *add_attrl(struct attrl *head);
static struct attrl *find_attrl(struct attrl *list, char *name);
static struct attrl *gather_rescs(rescspec *root, struct attrl *list);
static struct attrl *gather_assns(rescspec *root, struct attrl *list);
static double res_to_num(char *res_str);
static int compare_strings(int token_code, char *lval, char *rval);
static int compare_numeric(int token_code, char *lval, char *rval);
static int is_num(char *str);

void yy_scan_string(const char *str);
int yyparse(void);


/* static constants */
#define KILO            1024UL
#define MEGATOKILO      1024UL
#define GIGATOKILO      1048576UL
#define TERATOKILO      1073741824UL


int line_num = 1;
int cur_char = 0;
int yydebug;
int print_errors = 0;

rescspec *rescspec_root;

/*
 *
 *	parse_rescspec - call the parser to parse a rescspec and return it
 *
 *	  spec - the spec to parse
 *
 *	returns the parsed spec
 *
 */
rescspec *
rescspec_parse(char *str_spec)
{
	if (str_spec == NULL)
		return NULL;

	cur_char = 0;
	line_num = 1;
	yy_scan_string(str_spec);
	if (yyparse()) {
		/*    pbs_errno = PBSE_BADNODESPEC; */
		return NULL;	/* parse error */
	}

	return rescspec_root;
}

/*
 *
 *	rescspec_evaluate - evaluate a rescspec parsetree to see if it is
 *			    satasifiable with resource value pairs passed in
 *
 *	  spec   - the spec parse tree
 *	  list   - list of resource value pairs
 *	  logbug - a buffer for logging - must be passed in as ""
 *
 *	returns 0	: false
 *		else	: true
 *
 */
int
rescspec_evaluate(rescspec *spec, struct attrl *list, char *logbuf)
{
	/* the value of the attr on the left size of the operator */
	struct attrl *attr_val_left;
	/* the value of the attr on the right side of the operator (if an attr) */
	struct attrl *attr_val_right;
	/* the values to pass to the compare function */
	char *val1 = NULL, *val2 = NULL;
	int rc;	/* return code */

	if (spec == NULL)
		return 0;

	switch (spec->token_code) {
		case TOK_LP:	/* left parens will only have a left child */
			return rescspec_evaluate(spec->left_child, list, logbuf);
		case TOK_AND:
			return rescspec_evaluate(spec->left_child, list, logbuf) &&
			rescspec_evaluate(spec->right_child, list, logbuf) ? 1 : 0;
		case TOK_OR:
			return rescspec_evaluate(spec->left_child, list, logbuf) ||
			rescspec_evaluate(spec->right_child, list, logbuf) ? 1 : 0;

		case TOK_NOT:
			return !rescspec_evaluate(spec->left_child, list, logbuf);
	}

	if (spec->left_child->token_code ==TOK_RESC) {
		attr_val_left  = find_attrl(list, spec->left_child->lex_info);
		if (attr_val_left != NULL)
			val1 = attr_val_left->value;
	}
	else
		val1 = spec->left_child->lex_info;

	if (spec->right_child->token_code ==TOK_RESC) {
		attr_val_right = find_attrl(list, spec->right_child->lex_info);
		if (attr_val_right != NULL)
			val2 = attr_val_right->value;
	}
	else
		val2 = spec->right_child->lex_info;

	if (val1 != NULL && val2 != NULL) {
		if (is_num(val1) && is_num(val2))
			rc = compare_numeric(spec->token_code, val1, val2);
		else
			rc = compare_strings(spec->token_code, val1, val2);

		if (!rc && logbuf != NULL && logbuf[0] == '\0')
			sprintf(logbuf, "[%d] %s %s %s failed - L: %s R: %s\n",
				spec->char_num, spec->left_child->lex_info,
				spec->lex_info, spec->right_child->lex_info,
				val1, val2);

		return rc;
	}
	else
		return 0;
}




/*
 *
 *	rescspec_get_resources - return the resources used specified in
 *				 a rescspec.  The list which is returned can
 *				 then be completd and passed to
 *				 rescspec_evaluate()
 *
 *	  spec - the parse tree of the spec
 *
 *	returns a list of resources
 */
struct batch_status *rescspec_get_resources(rescspec *spec)
{
	struct batch_status *list;

	if (spec == NULL)
		return NULL;

	if ((list = (struct batch_status *) malloc(sizeof(struct batch_status))) == NULL)
		return NULL;

	/* we really only want the attrl part.  We pass back batch_status for the
	 * the ease of freeing it with pbs_statfree()
	 */
	list->next = NULL;
	list->name = NULL;
	list->text = NULL;
	list->attribs = gather_rescs(spec, NULL);
	return list;
}

/*
 *
 *	rescspec_get_assignments - get a list of resource assignments from
 *				   a rescspec
 *	  spec - the parsetree of the spec
 *
 *	returns batch status with resource list in the attribs
 *
 */
struct batch_status *rescspec_get_assignments(rescspec *spec)
{
	struct batch_status *list;

	if (spec == NULL)
		return NULL;

	if ((list = (struct batch_status *) malloc(sizeof(struct batch_status))) == NULL)
		return NULL;

	/* we really only want the attrl part.  We pass back batch_status for the
	 * the ease of freeing it with pbs_statfree()
	 */
	list->next = NULL;
	list->name = NULL;
	list->text = NULL;
	list->attribs = gather_assns(spec, NULL);
	return list;
}

/*
 *
 *	find_attrl - find attrl by resource in a list
 *
 *	  list - the list of attrls
 *	  name - the name of the resource
 *
 *	returns found attrl or NULL
 *
 */
static struct attrl *find_attrl(struct attrl *list, char *name)
{
	struct attrl *cur;

	if (name == NULL || list == NULL)
		return NULL;

	cur = list;
	while (cur != NULL && strcmp(cur->resource, name))
		cur = cur->next;

	return cur;
}

/*
 *
 *	add_attrl - allocate and add an attrl to a list
 *
 *	  head - the head of the list
 *
 *	returns the new node;
 *
 */
static struct attrl *add_attrl(struct attrl *head)
{
	struct attrl *cur;
	struct attrl *prev;

	prev = cur = head;

	while (cur != NULL) {
		prev = cur;
		cur = cur->next;
	}

	if ((cur = (struct attrl *) malloc(sizeof(struct attrl))) == NULL  )
		return NULL;

	cur->next = NULL;
	cur->name = NULL;
	cur->resource = NULL;
	cur->value = NULL;

	if (prev != NULL)
		prev->next = cur;

	return cur;
}

/*
 *
 *	gather_rescs - recursivily gather the TOK_RESC's out of a spec tree
 *
 *	  root - the root of the parse (sub)tree
 *	  list - the attrib list
 *
 */
static struct attrl *gather_rescs(rescspec *root, struct attrl *list)
{
	struct attrl *al;

	if (root == NULL)
		return list;

	if (root->token_code == TOK_RESC) {
		if ((al = add_attrl(list)) == NULL)
			return list;
		if ((al->name = strdup(ATTR_l)) == NULL) {
			free(al);
			return list;
		}
		if ((al->resource = strdup(root->lex_info)) == NULL) {
			free(al->name);
			free(al);
			return list;
		}
		al->value = NULL;
		switch (root->parent->token_code) {
			case TOK_GT:
				al->op = GT;
				break;
			case TOK_GE:
				al->op = GE;
				break;
			case TOK_LT:
				al->op = LT;
				break;
			case TOK_LE:
				al->op = LE;
			case TOK_NE:
				al->op = NE;
				break;
			case TOK_EQ_COMP:
				al->op = EQ;
				break;
			case TOK_EQ_ASSN:
				al->op = SET;
				break;
			default:
				al->op = DFLT;
		}
		if (list == NULL)
			list = al;
	}

	list = gather_rescs(root->left_child, list);
	list = gather_rescs(root->right_child, list);

	return list;
}


static struct attrl *gather_assns(rescspec *root, struct attrl *list)
{
	struct attrl *al;
	if (root == NULL)
		return list;

	if (root->token_code == TOK_EQ_ASSN) {
		if ((al = add_attrl(list)) == NULL)
			return list;
		if ((al->name = strdup(ATTR_l)) == NULL) {
			free(al);
			return list;
		}
		if ((al->resource = strdup(root->left_child  ->lex_info)) == NULL) {
			free(al->name);
			free(al);
			return list;
		}
		if ((al->value = strdup(root->right_child->lex_info)) == NULL) {
			free(al->resource);
			free(al->name);
			free(al);
			return list;
		}
		al->op = (enum batch_op)0;
		if (list == NULL)
			list = al;
	}

	list = gather_assns(root->left_child, list);
	list = gather_assns(root->right_child, list);

	return list;
}

void
print_attrl(struct attrl *list)
{
	struct attrl *cur;

	cur = list;

	while (cur != NULL) {
		printf("%s", cur->resource);
		if (cur->value != NULL)
			printf(" = %s", cur->value);
		printf("\n");
		cur = cur->next;
	}
}

/*
 *
 *      res_to_num - convert a resource string to a  sch_resource_t to
 *                      kilobytes
 *                      example: 1mb -> 1024
 *				 1mw -> 1024 * SIZEOF_WORD
 *
 *      res_str - the resource string
 *
 *      return a number in kilobytes or -1 on error
 *
 */

static double
res_to_num(char * res_str)
{
	double count = -1;			/* convert string resource to numeric */
	double count2 = -1;			/* convert string resource to numeric */
	char *endp;				/* used for strtol() */
	char *endp2;				/* used for strtol() */
	long multiplier;			/* multiplier to count */

	count = strtod(res_str, &endp);

	if (*endp == ':') { /* time resource -> convert to seconds */
		count2 = strtod(endp+1, &endp2);
		if (*endp2 == ':') { /* form of HH:MM:SS */
			count *= 3600;
			count += count2 * 60;
			count += strtol(endp2 + 1, &endp, 10);
			if (*endp != '\0')
				count = -1;
		}
		else			 { /* form of MM:SS */
			count *= 60;
			count += count2;
		}
		multiplier = 1;
	}
	else if (*endp == 'k' || *endp == 'K')
		multiplier = 1;
	else if (*endp == 'm' || *endp == 'M')
		multiplier = MEGATOKILO;
	else if (*endp == 'g' || *endp == 'G')
		multiplier = GIGATOKILO;
	else if (*endp == 't' || *endp == 'T')
		multiplier = TERATOKILO;
	else if (*endp == 'b' || *endp == 'B') {
		count /= KILO;
		multiplier = 1;
	}
	else if (*endp == 'w') {
		count /= KILO;
		multiplier = SIZEOF_WORD;
	}
	else	/* catch all */
		multiplier = 1;

	if (*endp != '\0' && *(endp + 1) == 'w')
		multiplier *= SIZEOF_WORD;

	return count * multiplier;
}

/*
 *
 *	is_num - checks to see if the string is a number, size, or float
 *		 or time in string form
 *
 *	  str - the string to test
 *
 *	returns  1 if str is a number
 *		 0 if str is not a number
 *		-1 on error
 *
 */
static int
is_num(char *str)
{
	int i;
	char c;
	int colen_count = 0;

	if (str == NULL)
		return -1;

	for (i = 0; i < strlen(str) && isdigit(str[i]); i++)
		;

	/* is the string completly numeric */
	if (i == strlen(str))
		return 1;

	/* is the string a size */
	if (i == strlen(str) - 2) {
		c = tolower(str[i]);
		if (c == 'k' || c == 'm' || c == 'g' || c == 't') {
			c = tolower(str[i+1]);
			if (c == 'b' || c == 'w')
				return 1;
		}
	}
	else if (i == strlen(str) - 1)
		if (tolower(str[i]) == 'b' || tolower(str[i] == 'w'))
			return 1;

	/* make sure we didn't stop on a decmal point */
	if (str[i] == '.') {
		for (i++ ; i < strlen(str) && isdigit(str[i]); i++)
			;

		/* number is a float */
		if (i == strlen(str))
			return 1;
	}

	/* last but not least, maybe it's a time (XX:YY:ZZ) */
	if (str[i] == ':') {
		colen_count = 1;
		for (i++ ; i < strlen(str) && (isdigit(str[i]) || str[i] == ':'); i++)
			if (str[i] == ':')
				colen_count++;

		if (colen_count <= 2)
			return 1;		/* number is a time */
	}

	/* the string is not a number or a size or time */
	return 0;
}

/*
 *
 *	compare_numeric - do a comparison specified by token_code on the two
 *			  values as numbers
 *
 *	 1: comparison is true
 *	 0: comparison is false
 *	-1: error
 *
 */
static int
compare_numeric(int token_code, char *lval, char *rval)
{

	double val1, val2;

	if (lval == NULL || rval == NULL)
		return -1;

	val1 = res_to_num(lval);
	val2 = res_to_num(rval);

	switch (token_code) {
		case TOK_GT:
			return val1 > val2;

		case TOK_GE:
			return val1 >= val2;

		case TOK_LT:
			return val1 < val2;

		case TOK_LE:
			return val1 <= val2;

		case TOK_NE:
			return val1 != val2;

		case TOK_EQ_COMP:
			return val1 == val2;

		case TOK_EQ_ASSN:
			/* since we're doing an assignment, we need >= the number */
			return val1 >= val2;

		default:
			return -1;
	}
}

/*
 *
 *	compare_strings - do a comparison specified by token_code on the two
 *			  values as strings
 *
 *	 1: comparison is true
 *	 0: comparison is false
 *	-1: error
 *
 */
static int
compare_strings(int token_code, char *lval, char *rval)
{

	int cmp;

	if (lval == NULL || rval == NULL)
		return -1;

	cmp = strcmp(lval, rval);

	switch (token_code) {
		case TOK_GT:
			return cmp > 0;

		case TOK_GE:
			return cmp >= 0;

		case TOK_LT:
			return cmp < 0;

		case TOK_LE:
			return cmp <= 0;

		case TOK_NE:
			return cmp != 0;

		case TOK_EQ_COMP:
			return cmp == 0;

		case TOK_EQ_ASSN:
			return cmp == 0;

		default:
			return -1;	/* error */
	}
}

void rescspec_print_errors(int p) { print_errors = p; }
void pr(rescspec *rspec) { print_rescspec_tree(rspec, stdout); }

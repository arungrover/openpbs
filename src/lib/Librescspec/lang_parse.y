%{
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
#include <stdlib.h>
#include "pbs_rescspec.h"
#include "parser.h"
#define YYSTYPE rescspec *

int yylex();
int yyerror( char *msg );
%}

%left TOK_LP TOK_RP
%left TOK_NOT
%left TOK_AND TOK_OR
%left TOK_GT TOK_GE TOK_LT TOK_LE TOK_NE TOK_EQ_COMP
%right TOK_EQ_ASSN

%token TOK_RESC
%token TOK_STRING
%token TOK_TIME
%token TOK_INT
%token TOK_FLOAT
%token TOK_SIZE
%token TOK_GT TOK_GE TOK_LT TOK_LE TOK_NE TOK_EQ_COMP TOK_EQ_ASSN
%token TOK_LP TOK_RP
%token TOK_AND TOK_OR
%token TOK_NOT

%start start
%%
start	: exp				{ $$ = tree_finished($1); }
	;

exp	: TOK_RESC oper literal		{ $$ = make_tree( $2, $1, $3 ); }
	| TOK_RESC oper TOK_RESC	{ $$ = make_tree( $2, $1, $3 ); }
	| TOK_LP exp TOK_RP		{ $$ = make_tree( $1, $2, NULL ); 
					       free_rescspec($3);
					}
	| TOK_NOT exp			{ $$ = make_tree( $1, $2, NULL ); }
	| exp TOK_AND exp		{ $$ = make_tree( $2, $1, $3 ); }
	| exp TOK_OR exp		{ $$ = make_tree( $2, $1, $3 ); }
	;

oper	: TOK_GT			{ $$ = $1; } 
	| TOK_GE			{ $$ = $1; }
	| TOK_LT			{ $$ = $1; }
	| TOK_LE			{ $$ = $1; }
	| TOK_NE			{ $$ = $1; }
	| TOK_EQ_COMP			{ $$ = $1; }
	| TOK_EQ_ASSN			{ $$ = $1; }
	| TOK_AND			{ $$ = $1; }
	| TOK_OR			{ $$ = $1; }
	;


literal	: TOK_STRING			{ $$ = $1; }
	| TOK_TIME			{ $$ = $1; }
	| number			{ $$ = $1; }
	;

number	: TOK_INT			{ $$ = $1; }
	| TOK_FLOAT			{ $$ = $1; }
	| TOK_SIZE			{ $$ = $1; }
	;

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

    #include <string.h>
    #include "tok.h"
    extern int line_num;
    extern int cur_char;
    extern int print_errors;
    int yyerror( char *msg );
%}
DIGIT	[0-9]
STRING	[A-Za-z]+

%%

\n				{ line_num++; cur_char = 0; }
[\t ]+				{ cur_char += strlen(yytext); }

{DIGIT}+			{ return make_token( TOK_INT ); }
{DIGIT}+\.{DIGIT}+		{ return make_token( TOK_FLOAT ); }
{DIGIT}+[kKmMgGtT][BbWw]	{ return make_token( TOK_SIZE ); }
{DIGIT}+\:{DIGIT}+		{ return make_token( TOK_TIME ); }
{DIGIT}+\:{DIGIT}+\:{DIGIT}+	{ return make_token( TOK_TIME ); }

{STRING}			{ return make_token( TOK_RESC ); }

[\\]?"\""[^"]*"\""		{ return make_token( TOK_STRING ); }
[\\]?"\'"[^']*"\'"		{ return make_token( TOK_STRING ); }

">"				{ return make_token( TOK_GT ); }
">="				{ return make_token( TOK_GE ); }
"<"				{ return make_token( TOK_LT ); }
"<="				{ return make_token( TOK_LE ); }
"!="				{ return make_token( TOK_NE ); }
"=="				{ return make_token( TOK_EQ_COMP ); }
"="				{ return make_token( TOK_EQ_ASSN ); }
"("				{ return make_token( TOK_LP ); }
")"				{ return make_token( TOK_RP ); }
"&&"				{ return make_token( TOK_AND ); }
"||"				{ return make_token( TOK_OR ); }
"!"				{ return make_token( TOK_NOT ); }

.				{ yyerror("Invalid Character"); }
%%

int yywrap( )
{
  return 1;
}

int yyerror( char *msg )
{
  if( print_errors )
    fprintf(stderr, "%d.%d: %s\n", line_num, cur_char, msg);
  return 0;
}

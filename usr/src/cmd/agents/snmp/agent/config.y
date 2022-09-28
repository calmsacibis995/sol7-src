
%start configuration

%token OPENBRACKET
%token CLOSEBRACKET
%token EQUAL
%token COMA
%token COMMUNITIES
%token READONLY
%token READWRITE
%token IDENTIFIER
%token MANAGERS
%token TRAPCOMMUNITY
%token TRAPDESTINATORS

%{
/* Copyright 07/01/96 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)config.y	1.4 96/07/01 Sun Microsystems"

#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <string.h>

#include "impl.h"
#include "error.h"
#include "trace.h"
#include "pdu.h"
#include "trap.h"

#include "agent_msg.h"
#include "access.h"
#include "snmpd.h"


/***** DEFINES *****/
/*
#define DEBUG_YACC(string) printf("\t\tYACC: %s: %s at line %d\n", string, yytext, yylineno);
*/
#define DEBUG_YACC(string)


/***** STATIC VARIABLES *****/

/* lexinput points to the current focus point in the config file */
static char *lexinput;

static char *community_name = NULL;
static int community_type = 0;

static char *current_filename = NULL;


%}

%%


configuration :	communities managers trapcommunity trapdestinators
		{
			DEBUG_YACC("configuration")
		}


/***************/
/* communities */
/***************/

communities :	t_communities t_equal t_openbracket communities_list t_closebracket
		{
			DEBUG_YACC("communities")
		}


communities_list : /* empty */ | communities_list community_item
		{
			DEBUG_YACC("communities_list")
		}


community_item : t_identifier
		{
			DEBUG_YACC("community_item 1")

			if(community_name)
			{
				error("BUG: community_name is not NULL in community_item");
			}

			community_name = strdup(yytext);
			if(community_name == NULL)
			{
				error(ERR_MSG_ALLOC);
				YYERROR;
			}
		}
		communitytype
		{
			int res;

			DEBUG_YACC("community_item 2")

			if(community_name == NULL)
			{
				error("BUG: community_name is NULL in community_item");
			}

			res = community_add(community_name, community_type, error_label);
			switch(res)
			{
				case 0:
					break;

				case 1:
					error("error in %s at line %d: %s",
						current_filename? current_filename: "???",
						yylineno, error_label);
					break;

				default:
					error("fatal error in %s at line %d: %s",
						current_filename? current_filename: "???",
						yylineno, error_label);
					YYERROR;
			}

			free(community_name);
			community_name = NULL;
		}

communitytype : t_readonly | t_readwrite
		{
			DEBUG_YACC("community_type")
		}


/************/
/* managers */
/************/

managers :	t_managers t_equal t_openbracket managers_list t_closebracket
		{
			DEBUG_YACC("agents")
		}


managers_list :	/* empty */ | managers_list list_separator manager_item
		{
			DEBUG_YACC("managers_list")
		}


manager_item :	t_identifier
		{
			int res;

			DEBUG_YACC("manager_item")

			res = manager_add(yytext, error_label);
			switch(res)
			{
				case 0:
					break;

				case 1:
					error("error in %s at line %d: %s",
						current_filename? current_filename: "???",
						yylineno, error_label);
					break;

				default:
					error("fatal error in %s at line %d: %s",
						current_filename? current_filename: "???",
						yylineno, error_label);
					YYERROR;
			}
		}


/***************/
/* sysloglevel */
/***************/

trapcommunity :	t_trapcommunity t_equal t_identifier
		{
			DEBUG_YACC("trap_community")

			if(trap_community)
			{
				error("BUG: trap_community not NULL in trap_community");
			}

			trap_community = strdup(yytext);
			if(trap_community == NULL)
			{
				error(ERR_MSG_ALLOC);
				YYERROR;
			}
		}


/*******************/
/* trapdestinators */
/*******************/

trapdestinators : t_trapdestinators t_equal t_openbracket trapdestinators_list t_closebracket
		{
			DEBUG_YACC("trapdestinators")
		}


trapdestinators_list : /* empty */ | trapdestinators_list list_separator trapdestinator_item
		{
			DEBUG_YACC("trapdestinators_list")
		}


trapdestinator_item : t_identifier
		{
			int res;

			DEBUG_YACC("trapdestinator_item")

			res = trap_destinator_add(yytext, error_label);
			switch(res)
			{
				case 0:
					break;

				case 1:
					error("error in %s at line %d: %s",
						current_filename? current_filename: "???",
						yylineno, error_label);
					break;

				default:
					error("fatal error in %s at line %d: %s",
						current_filename? current_filename: "???",
						yylineno, error_label);
					YYERROR;
			}
		}


/******************/
/* list separator */
/******************/

list_separator : /* empty */ | t_coma
		{
			DEBUG_YACC("list_separator")
		}


/*******************/
/* terminal tokens */
/*******************/

t_equal :	EQUAL
		{
			DEBUG_YACC("t_equal")
		}

t_openbracket :	OPENBRACKET
		{
			DEBUG_YACC("t_openbracket")
		}

t_closebracket : CLOSEBRACKET
		{
			DEBUG_YACC("t_closebracket")
		}

t_coma :	COMA
		{
			DEBUG_YACC("t_coma")
		}

t_identifier :	IDENTIFIER
		{
			DEBUG_YACC("t_identifier")
		}


t_communities :	COMMUNITIES
		{
			DEBUG_YACC("t_communities")
		}

t_readonly :	READONLY
		{
			DEBUG_YACC("t_readonly")

			community_type = READ_ONLY;
		}

t_readwrite :	READWRITE
		{
			DEBUG_YACC("t_readwrite")

			community_type = READ_WRITE;
		}


t_managers :	MANAGERS
		{
			DEBUG_YACC("t_managers")
		}


t_trapcommunity : TRAPCOMMUNITY
		{
			DEBUG_YACC("t_trapcommunity")
		}


t_trapdestinators : TRAPDESTINATORS
		{
			DEBUG_YACC("t_trapdestinators")
		}

%%

#include "config.lex.c"

/****************************************************************/

int yyerror(char *s)
{
	error("%s at line %d: %s", s, yylineno, yytext);
}


/****************************************************************/

/* If we have a serious problem, this function will	*/
/* terminate (<==> exit) the program			*/

void config_init(char *filename)
{
	struct stat statb;
	char *fileaddr;
	int fd;


	delete_manager_list();
	delete_community_list();
	if(trap_community)
	{
		free(trap_community);
		trap_community = NULL;
	}
	delete_trap_destinator_list();


	yylineno = 1;

	if((fd = open(filename, O_RDONLY)) < 0)
	{
		error_exit(ERR_MSG_OPEN,
			filename, errno_string());
	}

	/* 
	 * get the size of the file
	 */
	if(fstat(fd, &statb) < 0)
	{
		error_exit(ERR_MSG_FSTAT,
			filename, errno_string());
	}

	/* 
	 * and map it into my address space
	 */
	if(statb.st_size)
	{
		if((fileaddr = (char *) mmap(0, statb.st_size, PROT_READ|PROT_WRITE,
			MAP_PRIVATE, fd, 0)) <= (char *) 0)
		{
			error_exit(ERR_MSG_MMAP,
				filename, errno_string());
		}

		/*
		 * set current lex focus on the file
		 */

		lexinput = fileaddr;

		/*
		 * and parse the file
		 */

		current_filename = filename;
		if(yyparse() == 1)
		{
			error_exit("parsing %s failed", filename);
		}
		current_filename = NULL;

		/*
		 * Parsing is finished
		 *
		 * unmap the file and close it
		 */

		if(munmap(fileaddr, statb.st_size) == -1)
		{
			error(ERR_MSG_MUNMAP,
				errno_string());
		}
	}
	else
	{
		/* empty file, ignore it */

		error_exit("empty configuration file %s", filename);
	}

	if(close(fd) == -1)
	{
		error(ERR_MSG_CLOSE, errno_string());
	}

	if(trace_level > 0)
	{
		trace("\n");
		trace_communities();
		trace_managers();
		trace("trap-community : %s\n\n",
			trap_community);
		trace_trap_destinators();
	}
}


/****************************************************************/



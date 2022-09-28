
%{

#include <unistd.h>
#include <stdio.h>
#include <iostream.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include <rw/cstring.h>
#include <rw/tvslist.h>

#include "dmi_db.hh"
#include "util.hh"
#include "search_util.hh"

/***** DEFINE *****/

/*
#define DMI_DB_DEBUG_YACC(string) printf("\t\tYACC: %s: %s at line %d\n", string, yytext, yylineno);
*/
#define DMI_DB_DEBUG_YACC(string) 


#if(defined(__cpluscplus))
extern "C" int yyerror(char*s);
#endif

/* macro for action stmt */
#define StrAssignment(src,tgt) \
	DmiString_t *str; \
	if(!(str=Rwstr2Dmistr((src)))) YYABORT; \
	(tgt) = str; \
	delete src;	

#define DateAssignment(src,tgt) \
  	DmiTimestamp *ts; \
	if(!(ts=Rwstr2DmiTimeStamp(*src))) YYABORT; \
	(tgt) = ts;\
	delete src;

static DmiString_t* Rwstr2Dmistr(RWCString *);
static RWTValSlist<int> *int_list = NULL;
static void print_enum_lists(RWTIsvSlist<DMI_DB_EnumList> *);

%}

%union {
	RWCString	*sval;	/* string value */
	int		ival;	/* intger value */
	char 		cval; 	/* char value */
	DmiAttributeIds_t *attrids;
	DmiAccessMode   amval; /* access mode */
	DmiStorageType  stval; /* storage type */
}

%token <ival> DECNUMBER
%token <ival> OCTNUMBER
%token <ival> HEXNUMBER
%token <sval> QUOTEDSTRING
%token EQUAL
%token OPENBRACE
%token CLOSEBRACE
%token OPENBRACKET
%token CLOSEBRACKET
%token WILDCHAR
%token IDENTIFIER
%token COMA
%token MINUS

%token LANGUAGE
%token START
%token END
%token COMPONENT
%token NAME
%token DIRECTINTERFACE
%token DESCRIPTION
%token PATH
%token DOS
%token MACOS
%token OS2
%token UNIX
%token WIN16
%token WIN32
%token WIN9X
%token WINNT
%token ENUM
%token TYPE
%token GROUP
%token CLASS
%token ID
%token KEY
%token PRAGMA
%token TABLE
%token ATTRIBUTE
%token ACCESS
%token READONLY
%token READWRITE
%token WRITEONLY
%token STORAGE
%token SPECIFIC
%token COMMON
%token UNSUPPORTED
%token UNKNOWN
%token COUNTER
%token COUNTER64
%token DATE
%token GAUGE
%token OCTETSTRING
%token DISPLAYSTRING
%token STRING
%token INTEGER64
%token INT64
%token INTEGER
%token INT
%token VALUE

%type <sval> t_lang t_comp_id t_description t_description_string
%type <sval> t_comp_pragma_stmt t_grp_name_stmt t_class_stmt t_grp_pragma_stmt
%type <sval> t_attr_name_stmt t_pragma_stmt t_tbl_name_stmt 
%type <ival> t_id_stmt t_mif_integer t_attr_id_stmt
%type <stval> t_storage_stmt t_storage_type
%type <attrids> t_grp_key_stmt
%type <amval> t_access_stmt t_access_type

%%
start : t_mif_src_file | t_tbl_def_list | t_grp_def
	{
		DMI_DB_DEBUG_YACC("start")
	}
	;

t_tbl_def_list : t_tbl_def_list t_tbl_def | t_tbl_def
       {
		DMI_DB_DEBUG_YACC("t_tbl_def_list")
	}

t_mif_src_file : t_lang	
	t_comp_def
	{
		RWTPtrSlist<DmiString> *list;
		DmiString_t *str;

		DMI_DB_DEBUG_YACC("t_mif_src_file")
		list = new RWTPtrSlist<DmiString>;
		if(!(str=Rwstr2Dmistr($1))) YYABORT;
		list->insert(str);

		DBComp->SetLanguages(list);
	
		delete $1;
	}
	;

t_lang : LANGUAGE EQUAL QUOTEDSTRING
	{
		DMI_DB_DEBUG_YACC("t_lang")
		if(db_parser.get_parse_state()==DMI_DB_Parser::PS_LangState){
			DmiString_t *str;
			if(!(str=Rwstr2Dmistr($3))) YYABORT;
			db_server.set_dmi_lang_str(str);
			return 0;
		}
		$$ = $3;
	}		
	;


t_comp_def : START COMPONENT
	{
		DMI_DB_DEBUG_YACC("t_comp_def:start component")

		Component *comp=new Component;
		if(!comp) YYABORT;

		DBSComp(comp);
		
		RWTIsvSlist<Group> *groups;
		if(!(groups=new RWTIsvSlist<Group>)) YYABORT;
		DBComp->SetGroups(groups);

		RWTPtrSlist<Table> *tables;
		if(!(tables=new RWTPtrSlist<Table>)) YYABORT;
		DBComp->SetTables(tables);

		RWTIsvSlist<DMI_DB_EnumList> *enumLists;
		if(!(enumLists=new RWTIsvSlist<DMI_DB_EnumList>)) YYABORT;
		DBSEnumLists(enumLists);
	
	}
	t_comp_id t_comp_body END COMPONENT 
	{
		DMI_DB_DEBUG_YACC("t_comp_def:end component")
		StrAssignment($4,DBComp->GetComponentInfo()->name)

		/* need work on building global enum lists in comp */
	     if(DBEnumLists && DBEnumLists->entries()>0){
		RWTPtrSlist<GlobalEnum> *gblEnum = new RWTPtrSlist<GlobalEnum>;
		DBComp->SetGlobalEnumList(gblEnum);
		for(int i=0;i<DBEnumLists->entries();i++){
		  DMI_DB_EnumList * aEnum = DBEnumLists->at(i);
		  if(!aEnum) continue;
		  GlobalEnum *gEnum = new GlobalEnum;
		  gEnum->name = Rwstr2Dmistr(&(aEnum->_name));
		  gEnum->enumList = new DmiEnumList;
		  gEnum->enumList->list.list_len = aEnum->_npList->entries();
		  gEnum->enumList->list.list_val =(DmiEnumInfo_t*)
			malloc(sizeof(DmiEnumInfo_t)*aEnum->_npList->entries());
		  
		  for(int j=0;j<aEnum->_npList->entries();j++){
		  	DMI_DB_NP *np = aEnum->_npList->at(j);
			if(!np) continue;
			gEnum->enumList->list.list_val[j].name = 
				Rwstr2Dmistr((RWCString*)&(np->getName()));
			gEnum->enumList->list.list_val[j].value = np->getValue();
		  }
		  gblEnum->insert(gEnum);
		}
	    }
	}
	;

t_comp_id : NAME EQUAL QUOTEDSTRING
	{
		DMI_DB_DEBUG_YACC("t_comp_id")
		$$ = $3;
	}
	;

t_comp_body : t_comp_body t_comp_body_item |
	{
		DMI_DB_DEBUG_YACC("t_comp_body")
	}
	;

t_comp_body_item :   	
		t_description 
	{
		DMI_DB_DEBUG_YACC("t_comp_body_item:desc")
		StrAssignment($1,DBComp->GetComponentInfo()->description)

	}
    	|	t_path_def 
	{
		DMI_DB_DEBUG_YACC("t_comp_body_item:path")
	}
	|	t_gbl_enum_def 
	{
		DMI_DB_DEBUG_YACC("t_comp_body_item:enum")
		/* insert each enum list */
	}
	|	t_grp_def 
	{
		DMI_DB_DEBUG_YACC("t_comp_body_item:grp")
	}
	|	t_tbl_def 
	{
		DMI_DB_DEBUG_YACC("t_comp_body_item:tbl")
	}
	|	t_comp_pragma_stmt 
	{
		DMI_DB_DEBUG_YACC("t_comp_body_item:prag")
		StrAssignment($1,DBComp->GetComponentInfo()->pragma)
	}
	;

t_description :	DESCRIPTION EQUAL t_description_string
	{
		DMI_DB_DEBUG_YACC("t_description")
		$$ = $3;
	}
	;

t_description_string : t_description_string QUOTEDSTRING
	{
		DMI_DB_DEBUG_YACC("t_description_string")
		/* append the string into the RWCString */
		*($1) += *($2);
		delete $2;
		$$ = $1;
	}
	| QUOTEDSTRING
	{
		DMI_DB_DEBUG_YACC("t_description_string")
		$$ = $1;
	}
	;

t_comp_pragma_stmt : PRAGMA EQUAL QUOTEDSTRING
	{
		DMI_DB_DEBUG_YACC("t_comp_pragma_stmt")
		$$ = $3;
	}
	;


t_path_def : START PATH t_path_id t_path_body END PATH
	{
		DMI_DB_DEBUG_YACC("t_path_def")
	}
	;

t_path_id : NAME EQUAL QUOTEDSTRING
        {
                DMI_DB_DEBUG_YACC("t_path_id")
        }
        ;
 
t_path_body : t_path_body t_path_stmt | t_path_stmt
        {
                DMI_DB_DEBUG_YACC("t_path_body")
        }
        ;
 
t_path_stmt : t_os_name EQUAL QUOTEDSTRING |
              t_os_name EQUAL DIRECTINTERFACE
        {
                DMI_DB_DEBUG_YACC("t_path_stmt")
        }
        ;
 
t_os_name : DOS | MACOS | OS2 | UNIX | WIN16 | WIN32 | WIN9X | WINNT
        {
                DMI_DB_DEBUG_YACC("t_os_name")
        }
        ;

t_gbl_enum_def : START ENUM 
	{
		DMI_DB_DEBUG_YACC("start enum")
		DMI_DB_EnumList *enumList;
		if(!(enumList= new DMI_DB_EnumList())) YYABORT;
	
		DBEnumLists->insert(enumList);
		DBSAEnum(enumList);

		RWTIsvSlist<DMI_DB_NP> *npList;
		if(!(npList=new RWTIsvSlist<DMI_DB_NP>)) YYABORT;
		DBAEnum->_npList = npList;
		

	}
	t_enum_id t_opt_enum_type t_enum_body END ENUM 
        {
                DMI_DB_DEBUG_YACC("t_gbl_enum_def")
		/* testint */
	
        }
        ;

t_enum_id : NAME EQUAL QUOTEDSTRING
        {
                DMI_DB_DEBUG_YACC("t_enum_id")
		DBAEnum->_name = *$3;
		delete $3;
        }
        ;

t_opt_enum_type : TYPE EQUAL INT | TYPE EQUAL INTEGER |
        {
                DMI_DB_DEBUG_YACC("t_opt_enum_type")
        }
        ;

t_enum_body : t_enum_body t_enum_stmt 
        {
                DMI_DB_DEBUG_YACC("t_enum_body")
        }
	| t_enum_stmt
        {
                DMI_DB_DEBUG_YACC("t_enum_body")
        }
        ;
 
t_enum_stmt : t_mif_integer EQUAL QUOTEDSTRING
        {
                DMI_DB_DEBUG_YACC("t_enum_stmt")
		DMI_DB_NP *np;
		if(!(np= new DMI_DB_NP(*$3,$1))) YYABORT;
		DBAEnum->_npList->insert(np);
        }
        ;

t_grp_def : START GROUP
        {
		/* it is possible for a group not belonging
		   to any component */
                DMI_DB_DEBUG_YACC("t_grp_def:start group")

		Group  *group;
		if(!(group=new Group())) YYABORT;

		if(db_parser.get_parse_state()!=DMI_DB_Parser::PS_GrpState)
/*			DBComp->GetGroups()->insert(group); */
			insertGroup(DBComp->GetGroups(), group); 
		   

		DBSGroup(group);

		RWTIsvSlist<Attribute> *attrs;
		if(!(attrs=new RWTIsvSlist<Attribute>)) YYABORT;
		DBGroup->SetAttributes(attrs);

        }
	t_grp_identifier t_grp_body_list END GROUP
	{
                DMI_DB_DEBUG_YACC("t_grp_def:end group")
	}
        ;

t_grp_identifier : t_grp_id_list
	{
                DMI_DB_DEBUG_YACC("t_grp_identifier")
	}
	;

t_grp_id_list : t_grp_id_list t_grp_id | t_grp_id
	{
                DMI_DB_DEBUG_YACC("t_grp_id_list")
	}
	;

t_grp_id : 	t_grp_name_stmt 
	{
                DMI_DB_DEBUG_YACC("t_grp_id:grp_name")
		StrAssignment($1,
		DBGroup->GetGroupInfo()->name)
	}
	|	t_class_stmt 
	{
                DMI_DB_DEBUG_YACC("t_grp_id:class")
		StrAssignment($1,
		DBGroup->GetGroupInfo()->className)
	}
	|	t_id_stmt
        {
                DMI_DB_DEBUG_YACC("t_grp_id:id")
		DBGroup->GetGroupInfo()->id = $1;
        }
        ;

t_grp_name_stmt : NAME EQUAL QUOTEDSTRING
        {
                DMI_DB_DEBUG_YACC("t_grp_name_stmt")
		$$ = $3;
        }
        ;

t_class_stmt : CLASS EQUAL QUOTEDSTRING
        {
                DMI_DB_DEBUG_YACC("t_class_stmt")
		$$ = $3;
        }
        ;
 
t_grp_body_list : t_grp_body_list t_grp_body |
	{
	}

t_grp_body :    
		t_description 
	{
                DMI_DB_DEBUG_YACC("t_grp_body:desc")
		StrAssignment($1,
		DBGroup->GetGroupInfo()->description)
	}
	|	t_grp_key_stmt 
	{
                DMI_DB_DEBUG_YACC("t_grp_body:key")
		DBGroup->GetGroupInfo()->keyList = $1;
	}
	|	t_grp_pragma_stmt 
	{
                DMI_DB_DEBUG_YACC("t_grp_body:pragma")
		StrAssignment($1,
		DBGroup->GetGroupInfo()->pragma)
	}
	|	t_grp_attr_def
        {
                DMI_DB_DEBUG_YACC("t_grp_body:attr_def")
        }
        ;

t_grp_key_stmt : KEY EQUAL 
	{
		DMI_DB_DEBUG_YACC("t_grp_key_stmt:key equal")
		if(int_list) free(int_list);
		int_list = new RWTValSlist<int>;
	}
		t_key_list
	{
		DMI_DB_DEBUG_YACC("t_grp_key_stmt")
		DmiAttributeIds *attrid = 
		  newDmiAttributeIds(int_list->entries());
		for(int i=0;i<int_list->entries();i++)
		  attrid->list.list_val[i] = int_list->at(i);
		if(int_list) free(int_list);
		int_list = NULL;
		$<attrids>$ = attrid;
	}
	;

t_key_list : t_key_list COMA t_mif_integer
	{
		DMI_DB_DEBUG_YACC("t_key_list:key,integer")
		if(int_list) int_list->insert($3);
	}
	| t_mif_integer
	{
		DMI_DB_DEBUG_YACC("t_key_list:integer")
		if(int_list) int_list->insert($1);
	}
	;

t_tbl_def : START TABLE 
	{
		DMI_DB_DEBUG_YACC("t_tbl_def:start table")
		Table *table;
		if(!(table=new Table())) YYABORT;

		if(db_parser.get_parse_state()!=DMI_DB_Parser::PS_TblState)
			DBComp->GetTables()->insert(table);

		DBSTable(table);

		RWTPtrSlist<DmiAttributeValues_t> *rows;
		if(!(rows=new RWTPtrSlist<DmiAttributeValues_t>)) YYABORT;
		DBTable->SetRows(rows);
	}
	t_tbl_id_list t_tbl_body END TABLE
	{
		DMI_DB_DEBUG_YACC("t_tbl_def:end table")
	}
	;

t_tbl_id_list : t_tbl_id_list t_tbl_id | t_tbl_id
	{
		DMI_DB_DEBUG_YACC("t_tbl_id_list")
	}
	;

t_tbl_id : t_tbl_name_stmt 
	{
		DMI_DB_DEBUG_YACC("t_tbl_id:name")
		StrAssignment($1,
		DBTable->GetTableInfo()->name)
	}
	| 	t_class_stmt 
	{
		DMI_DB_DEBUG_YACC("t_tbl_id:class")
		StrAssignment($1,
		DBTable->GetTableInfo()->className)
	}
	|	t_id_stmt
	{
		DMI_DB_DEBUG_YACC("t_tbl_id:id")
		DBTable->GetTableInfo()->id = $1;
	}
	;

t_id_stmt : ID EQUAL DECNUMBER 
        {
                DMI_DB_DEBUG_YACC("t_id_stmt")
		$$ = $3;
        }
        ;

t_tbl_name_stmt : NAME EQUAL QUOTEDSTRING
	{
		DMI_DB_DEBUG_YACC("t_tbl_name_stmt")
		$$ = $3;
	}
	;

t_tbl_body : t_tbl_body t_tbl_row | t_tbl_row
	{
		DMI_DB_DEBUG_YACC("t_tbl_body")
	}
	;

t_tbl_row : OPENBRACE
	{
		DMI_DB_DEBUG_YACC("t_tbl_row:OPENBRACE")

		if(!(DBTable->GetTableInfo()->className)) YYABORT;
		Group *group=db_find_group(DBTable->GetTableInfo()->className);
		DmiAttributeValues_t *attr_value = new_attr_value(group);
		if(!attr_value) YYABORT;
		DBTable->GetRows()->insert(attr_value);
		DBSRow(attr_value);
		DBSCol(0);
	}
	t_tbl_row_list CLOSEBRACE
	{
		DMI_DB_DEBUG_YACC("t_tbl_row:CLOSEBRACE")
	}
	;

t_tbl_row_list :  t_tbl_row_list COMA
	{
		DMI_DB_DEBUG_YACC("t_tbl_row_list, commalist")
		/* each coma bumps up the attr_index */
		DBIncCol;	
		
	}
	t_tbl_item
	{
		DMI_DB_DEBUG_YACC("t_tbl_row_list:list")
	}
	| t_tbl_item
	{
		DMI_DB_DEBUG_YACC("t_tbl_row_list:indv.")
	}
	;

t_tbl_item : t_tbl_const_expr
	{
		DMI_DB_DEBUG_YACC("t_tbl_item:t_tbl_const_expr")
	}
	|
	{
		DMI_DB_DEBUG_YACC("t_tbl_item:empty")
	}
	;

t_tbl_const_expr : 	QUOTEDSTRING
	{
		if(DBRow->list.list_val[DBCol].data.type==MIF_DATE){
		  DateAssignment($1,DBRow->list.list_val[DBCol].data.DmiDataUnion_u.date)
		}else{
		  StrAssignment($1,DBRow->list.list_val[DBCol].data.DmiDataUnion_u.str)
		}
		
		/* Date can be in this format */
	}
	|	WILDCHAR QUOTEDSTRING
	{
	}
	|	t_mif_integer
	{
		/* didn't handle 64bytes */
		switch(DBRow->list.list_val[DBCol].data.type){
		  case MIF_COUNTER:
		  	DBRow->list.list_val[DBCol].data.DmiDataUnion_u.counter
					= $1;
				break;
		  case MIF_GAUGE:
		  	DBRow->list.list_val[DBCol].data.DmiDataUnion_u.gauge
					= $1;
				break;
		  case MIF_INTEGER:
		  	DBRow->list.list_val[DBCol].data.DmiDataUnion_u.integer
					= $1;
				break;
		}
	}
	|	UNKNOWN
	{
	}
	|	UNSUPPORTED
	{
	}
	;


t_grp_pragma_stmt : PRAGMA EQUAL QUOTEDSTRING
	{
                DMI_DB_DEBUG_YACC("t_grp_pragma_stmt")
		$$ = $3;
        }
	;
 
t_grp_attr_def : START ATTRIBUTE 
        {
                DMI_DB_DEBUG_YACC("t_grp_attr_def:start attr")
		Attribute *attr;
		if(!(attr=new Attribute())) YYABORT;

		DBSAttr(attr);

		if(!DBGroup) YYABORT;
		DBGroup->GetAttributes()->insert(attr);
        }
	t_attr_definition t_attr_body_list END ATTRIBUTE
        {
                DMI_DB_DEBUG_YACC("t_grp_attr_def:end attr")
        }
        ;

t_attr_definition : t_attr_definition t_attr_def | t_attr_def
	{
	}


t_attr_def : t_attr_name_stmt 
	{
		DMI_DB_DEBUG_YACC("t_attr_def:name")
		StrAssignment($1,
		DBAttr->GetInfo()->name)
	}
	| t_attr_id_stmt
	{
		DMI_DB_DEBUG_YACC("t_attr_def:id")
		DBAttr->GetInfo()->id = $1;
	}
	;

t_attr_name_stmt : NAME EQUAL QUOTEDSTRING 
	{
		DMI_DB_DEBUG_YACC("t_attr_name_stmt")
		$$ = $3;
	}
	;

t_attr_id_stmt : ID EQUAL t_mif_integer
	{
		DMI_DB_DEBUG_YACC("t_attr_id_stmt")
		$$ = $3;
	}
	;

t_attr_body_list : t_attr_body_list t_attr_body |
	{
		DMI_DB_DEBUG_YACC("t_attr_body_list")
	}
	;


t_attr_body : 	t_description 
	{
		DMI_DB_DEBUG_YACC("t_opt_attr_body:description")
		StrAssignment($1,
		DBAttr->GetInfo()->description)
	}
	|	t_access_stmt 
	{
		DMI_DB_DEBUG_YACC("t_opt_attr_body:access")
		DBAttr->GetInfo()->access = $1;
	}
	|	t_storage_stmt
	{
		DMI_DB_DEBUG_YACC("t_opt_attr_body:storage")
		DBAttr->GetInfo()->storage = $1;
	}
	|	t_pragma_stmt
	{
		DMI_DB_DEBUG_YACC("t_opt_attr_body:pragma")
		StrAssignment($1,
		DBAttr->GetInfo()->pragma)
	}
	|	t_type_stmt
	{
		DMI_DB_DEBUG_YACC("t_opt_attr_body:type")
	}
	|	t_value_stmt
	{
		DMI_DB_DEBUG_YACC("t_opt_attr_body:value")
	}
	;

t_access_stmt :  ACCESS EQUAL t_access_type
	{
		DMI_DB_DEBUG_YACC("t_access_stmt")
		$$ = $3;
	}
	;

t_access_type : READONLY 
	{
		DMI_DB_DEBUG_YACC("t_access_type:readonly")
		$$ = MIF_READ_ONLY;
	}
	|	READWRITE 
	{
		DMI_DB_DEBUG_YACC("t_access_type:readwrite")
		$$ = MIF_READ_WRITE;
	}
	|	WRITEONLY
	{
		DMI_DB_DEBUG_YACC("t_access_type:writeonly")
		$$ = MIF_WRITE_ONLY;
	}
	;

t_storage_stmt : STORAGE EQUAL t_storage_type
	{
		DMI_DB_DEBUG_YACC("t_storage_type")
		$$ = $3;
	}
	;

t_storage_type : SPECIFIC 
	{
		DMI_DB_DEBUG_YACC("t_storage_type:specific")
		$$ = MIF_SPECIFIC;
	}
	| COMMON
	{
		DMI_DB_DEBUG_YACC("t_storage_type:common")
		$$ = MIF_COMMON;
	}
	;

t_pragma_stmt : PRAGMA EQUAL QUOTEDSTRING
	{
		DMI_DB_DEBUG_YACC("t_pragma_stmt")
		$$ = $3;
	}
	;


t_type_stmt : TYPE EQUAL t_attr_type
	{
		DMI_DB_DEBUG_YACC("t_type_Stmt")
	}
	;

t_attr_type : 	QUOTEDSTRING 
	{
		/* hack: some problem on attr type which is enum */
		DMI_DB_DEBUG_YACC("t_attr_type:quotedstring")
		/* set the enum_flag in attr, the type must be global
		  store the name into the enum_name */
		
		DBAttr->SetEnumFlag(TRUE);
		DBAttr->GetInfo()->type = MIF_INTEGER;
		DBAttr->GetData()->type = 
		DBAttr->GetInfo()->type;
		DmiString_t *str; 
		if(!(str=Rwstr2Dmistr(($1)))) YYABORT; 
		DBAttr->SetEnumName(str);
		delete $1;
	}
	|	t_local_enum_def 
	{
		/* hack: some problem on attr type which is enum */
		DMI_DB_DEBUG_YACC("t_attr_type:localenum")
	}
	|	COUNTER 
	{
		DMI_DB_DEBUG_YACC("t_attr_type:counter")
		DBAttr->GetInfo()->type = MIF_COUNTER;
		DBAttr->GetData()->type = 
		DBAttr->GetInfo()->type;
		
	}
	|	COUNTER64 
	{
		DMI_DB_DEBUG_YACC("t_attr_type:counter64")
		DBAttr->GetInfo()->type = MIF_COUNTER64;
		DBAttr->GetData()->type = 
		DBAttr->GetInfo()->type;
	}
	|	DATE 
	{
		DMI_DB_DEBUG_YACC("t_attr_type:date")
		DBAttr->GetInfo()->type = MIF_DATE;
		DBAttr->GetData()->type = 
		DBAttr->GetInfo()->type;
	}
	|	GAUGE 
	{
		DMI_DB_DEBUG_YACC("t_attr_type:gauge")
		DBAttr->GetInfo()->type = MIF_GAUGE;
		DBAttr->GetData()->type = 
		DBAttr->GetInfo()->type;
	}
	|	OCTETSTRING OPENBRACKET t_mif_integer CLOSEBRACKET 
	{
		DMI_DB_DEBUG_YACC("t_attr_type:oct")
		DBAttr->GetInfo()->type = MIF_OCTETSTRING;
		DBAttr->GetData()->type = 
		DBAttr->GetInfo()->type;
	}
	|	DISPLAYSTRING OPENBRACKET t_mif_integer CLOSEBRACKET 
	{
		DMI_DB_DEBUG_YACC("t_attr_type:displaystring")
		DBAttr->GetInfo()->type = MIF_DISPLAYSTRING;
		DBAttr->GetInfo()->maxSize = $3;
		DBAttr->GetData()->type = 
		DBAttr->GetInfo()->type;
	}
	|	STRING OPENBRACKET t_mif_integer CLOSEBRACKET 
	{
		DMI_DB_DEBUG_YACC("t_attr_type:string")
		DBAttr->GetInfo()->type = MIF_DISPLAYSTRING;
		DBAttr->GetInfo()->maxSize = $3;
		DBAttr->GetData()->type = 
		DBAttr->GetInfo()->type;
	}
	|	INT 
	{
		DMI_DB_DEBUG_YACC("t_attr_type:int")
		DBAttr->GetInfo()->type = MIF_INTEGER;
		DBAttr->GetData()->type = 
		DBAttr->GetInfo()->type;
	}
	|	INTEGER 
	{
		DMI_DB_DEBUG_YACC("t_attr_type:integer")
		DBAttr->GetInfo()->type = MIF_INTEGER;
		DBAttr->GetData()->type = 
		DBAttr->GetInfo()->type;
	}
	|	INTEGER64 
	{
		DMI_DB_DEBUG_YACC("t_attr_type:integer64")
		DBAttr->GetInfo()->type = MIF_INTEGER64;
		DBAttr->GetData()->type = 
		DBAttr->GetInfo()->type;
	}
	|	INT64
	{
		DMI_DB_DEBUG_YACC("t_attr_type:int64")
		DBAttr->GetInfo()->type = MIF_INTEGER64;

		DBAttr->GetData()->type = 
		DBAttr->GetInfo()->type;
	}
	;

t_local_enum_def : START ENUM
	{
		DMI_DB_DEBUG_YACC("start enum")
		DMI_DB_EnumList *enumList;
		if(!(enumList= new DMI_DB_EnumList())) YYABORT;

		DBAttr->SetEnumFlag(TRUE);
		DBAttr->GetInfo()->type = MIF_INTEGER;
		DBAttr->GetData()->type = 
		DBAttr->GetInfo()->type;
		DBAttr->GetInfo()->enumList = (DmiEnumList*)
			malloc(sizeof(DmiEnumList));
	
		DBSAEnum(enumList);

		RWTIsvSlist<DMI_DB_NP> *npList;
		if(!(npList=new RWTIsvSlist<DMI_DB_NP>)) YYABORT;
		DBAEnum->_npList = npList;
	}
	t_opt_enum_id t_opt_enum_type t_enum_body END ENUM
	{
		DMI_DB_DEBUG_YACC("t_local_enum_def")
		DBAttr->GetInfo()->enumList = new DmiEnumList;
		DBAttr->GetInfo()->enumList->list.list_val =(DmiEnumInfo_t*)
			malloc(sizeof(DmiEnumInfo_t)*DBAEnum->_npList->entries());
		DBAttr->GetInfo()->enumList->list.list_len =
			DBAEnum->_npList->entries();
		for(int j=0;j<DBAEnum->_npList->entries();j++){
			DMI_DB_NP *np = DBAEnum->_npList->at(j);
			if(!np) continue;
			DBAttr->GetInfo()->enumList->list.list_val[j].name =
			  Rwstr2Dmistr((RWCString*)&(np->getName()));
			DBAttr->GetInfo()->enumList->list.list_val[j].value =
			  np->getValue();
		}
	}
	;

t_opt_enum_id : t_enum_id 
	{
		if(DBAEnum && DBAEnum->_name.length()>0){
		  DBAttr->SetEnumName(Rwstr2Dmistr(&(DBAEnum->_name)));
		}
	}
	|
	{
	}
	;


t_value_stmt : VALUE EQUAL t_const_expr
	{
		DMI_DB_DEBUG_YACC("t_value_stmt")
		/* Unsupported and Unknown not supported */
	}
	;

t_const_expr : 	QUOTEDSTRING
	{

		/* if type is global or local enum,
		   do searching, set type to MIF_INTEGER,
		   store the value as integer value */	
		if(DBAttr->GetEnumFlag()==TRUE){
		  if(!(DBAttr->GetInfo()->enumList)){
			DmiString_t *enum_name = DBAttr->GetEnumName();
			static char buf[100];
			buf[0] ='\0';
			sprintf(buf,enum_name->body.body_val,enum_name->body.body_len);
			static RWCString str(buf);
			DMI_DB_EnumList *enumList = db_server.find_enum_list(str);
			if(!enumList) YYABORT;
			int num = enumList->find_value(*$1);
			if(num == -1) YYABORT;
			DBAttr->GetData()->DmiDataUnion_u.integer = num;
		  }else{
			/* local enum list */
			int num = DBAEnum->find_value(*$1);
			DBAttr->GetData()->DmiDataUnion_u.integer = num;
		  }
		}else if(DBAttr->GetInfo()->type==MIF_DATE){
		  DateAssignment($1,DBAttr->GetData()->DmiDataUnion_u.date)	
		}else{
		  StrAssignment($1,DBAttr->GetData()->DmiDataUnion_u.str)
		}
		
		/* Date can be in this format */
	}
	|	WILDCHAR QUOTEDSTRING
	{
	}
	|	t_mif_integer
	{
		/* didn't handle 64bytes */
		switch(DBAttr->GetInfo()->type){
		 	case MIF_COUNTER:
				DBAttr->GetData()->DmiDataUnion_u.counter
					= $1;
				break;
			case MIF_GAUGE:
				DBAttr->GetData()->DmiDataUnion_u.gauge 
					= $1;
				break;
			case MIF_INTEGER:
				DBAttr->GetData()->DmiDataUnion_u.integer
					= $1;
				break;
		}
	}
	|	UNKNOWN
	{
		if(DBAttr->GetInfo()->type == MIF_OCTETSTRING ||
		   DBAttr->GetInfo()->type == MIF_DISPLAYSTRING){
			RWCString *tmp= new RWCString("");
		  	StrAssignment(tmp,DBAttr->GetData()->DmiDataUnion_u.str);
		}
	}
	|	UNSUPPORTED
	{
	}
	;



t_mif_integer : DECNUMBER 
	{
                DMI_DB_DEBUG_YACC("t_mif_integer")
		$$ = $1;
	}
	|	OCTNUMBER
	{
                DMI_DB_DEBUG_YACC("t_mif_integer")
		$$ = $1;
	}
	|	HEXNUMBER
        {
                DMI_DB_DEBUG_YACC("t_mif_integer")
		$$ = $1;
        }
        ;



%%

#include "config.lex.cc"

/****************************************************************/

int yywrap()
{
  return 1;
}

void yyerror(char*)
{
/*
  cout << "BUG:" << db_parser.getFilename() << ":" << yylineno << ":" << yytext  << endl;
*/
 static char buf[256];
 buf[0]='\0';
 sprintf(buf,"Parsing Error in File %s on line %d with token %s\n",(char*)(const char*)(db_parser.getFilename()),yylineno,yytext);
 parseErr->setParseErr(buf);
}

static DmiString_t* Rwstr2Dmistr(RWCString *rwstr)
{
  return( newDmiString( (char*)(const char*)(*rwstr) ) );
}

static DmiTimestamp *Rwstr2DmiTimeStamp(RWCString rwstr)
{
  DmiTimestamp *ts;

  if(!(ts=(DmiTimestamp*)calloc(1,sizeof(DmiTimestamp)))) return NULL;
  for(int i=0;i<rwstr.length();i++){
    switch(i){
	case 0: case 1: case 2: case 3:
		if(isdigit(rwstr[i])) 
			ts->year[i] = rwstr[i];
		else{
			delete ts;
			return NULL;
		}
		break;
	case 4: case 5: 
		if(isdigit(rwstr[i])) ts->month[i-4] = rwstr[i];
		break;
	case 6: case 7:
		if(isdigit(rwstr[i])) ts->day[i-6] = rwstr[i];
		break;
	case 8: case 9:
		if(isdigit(rwstr[i])) ts->hour[i-8] = rwstr[i];
		break;
	case 10: case 11:
		if(isdigit(rwstr[i])) ts->minutes[i-10] = rwstr[i];
		break;
	case 12: case 13:
		if(isdigit(rwstr[i])) ts->seconds[i-12] = rwstr[i];
		break;
	case 14:
		if(rwstr[i]=='.') ts->dot = rwstr[i];
		break;
	case 15: case 16: case 17: case 18: case 19: case 20:
		if(isdigit(rwstr[i])) ts->microSeconds[i-15] = rwstr[i];
		break;
	case 21:
		if(rwstr[i]=='-' || rwstr[i]=='+') ts->plusOrMinus = rwstr[i];
		break;
	case 22: case 23: case 24:
		if(isdigit(rwstr[i])) ts->utcOffset[i-22] = rwstr[i];
    }
  }
  return ts;
}

Group* db_find_group(DmiString_t *name)
{
  RWTIsvSlist<Group> *groups = DBComp->GetGroups();

  if(!name) return NULL;
  if(!groups) return NULL;
  for(int i=0; i<groups->entries(); i++){
	Group *cur_group = groups->at(i);
	if(!cur_group) break;
	if(!cmpDmiString(cur_group->GetGroupInfo()->className,name)){
		return cur_group;
	}
  }
  return NULL;
}


/* create a default row instance */
/* find the group and calc # of attr */
/* create DmiAttributeValues_t with the # */
/* assign id from attr in group to attrvalue */
/* copy the type and default value */
DmiAttributeValues_t *new_attr_value(Group *group)
{
  DmiAttributeValues *attr_value;
  int num_of_attrs;

  if(!group) return NULL;
  num_of_attrs = group->GetAttributes()->entries();
  if(num_of_attrs == 0 ) return NULL;
  if(!(attr_value=(DmiAttributeValues*)calloc(1,sizeof(DmiAttributeValues)))) return NULL;
  attr_value->list.list_len = num_of_attrs;
  if(!(attr_value->list.list_val=(DmiAttributeData_t*)calloc(num_of_attrs,sizeof(DmiAttributeData_t))))
	return NULL;

  RWTIsvSlist<Attribute>* attrs=group->GetAttributes();
  Attribute *cur_attr;

  for(int i=0; i<num_of_attrs; i++){
	cur_attr = attrs->at(i);
	attr_value->list.list_val[i].id = cur_attr->GetInfo()->id ;
	copyDmiDataUnion(&(attr_value->list.list_val[i].data),cur_attr->GetData());
  }
  return attr_value;
}

static void print_enum_lists(RWTIsvSlist<DMI_DB_EnumList> *enumList)
{
  for(int i=0;i<enumList->entries();i++){
	DMI_DB_EnumList * aEnum=enumList->at(i);
	if(!aEnum) continue;
	cout << "ENUMERATION::" << aEnum->_name << endl;
	if(!aEnum->_npList) continue;
	for(int j=0;j<aEnum->_npList->entries();j++){
		DMI_DB_NP *np = aEnum->_npList->at(j);
		if(!np) continue;
		cout << "(" << np->getName() << "," << np->getValue() << ")" << endl;
	}
  }
}

void reset_yylineno()
{
	yylineno = 1;
}

/* Copyright 11/01/96 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)dmi_db.cc	1.9 96/11/01 Sun Microsystems"

#include "dmi_db.hh"
#include <unistd.h>
#include "util.hh"

extern int yyparse();
extern FILE *yyin;
extern void reset_yylineno();

DMI_DB_Server db_server;
DMI_DB_Parser db_parser;

RWCString DMI_DB_File_Service::_db_location=".";
RWCString DMI_DB_File_Service::_mif_location=".";

DMI_DB_Parser::DMI_DB_Parser()
{
  _enum_table = NULL;
  _lineno = 0;
  _parse_state = PS_NoState;
}

int DMI_DB_Parser::parse(char* filename,int isMifFile)
{
  if(!filename) return E_FILE_NOT_EXIST;
  _filename = filename;
  _fp = DMI_DB_File_Service::open_file(_filename,"r",isMifFile);
  if(!_fp) return E_FILE_CANT_OPEN;
  yyin = _fp;	//hack
  reset_yylineno();
  if(yyparse()==0) {
	  DMI_DB_File_Service::close_file(_fp);	  
	  return E_NO_ERROR;
  }
  else {
	  DMI_DB_File_Service::close_file(_fp);	  	  
	  return E_PARSING_ERROR;
  }
}

int DMI_DB_Parser::parse(RWCString filename,int isMifFile)
{

  if(filename.length()==0) return E_FILE_NOT_EXIST;
  _filename = filename;
  _fp = DMI_DB_File_Service::open_file(_filename,"r",isMifFile);
  if(!_fp) return E_FILE_CANT_OPEN;
  yyin = _fp;	//hack
  reset_yylineno();
  if(yyparse()==0) {
	  DMI_DB_File_Service::close_file(_fp);
	  return E_NO_ERROR;
  }
  else {
	  DMI_DB_File_Service::close_file(_fp);
	  return E_PARSING_ERROR;
  }
}


void DMI_DB_Parser::set_parse_state(ParseState ps)
{
  _parse_state = ps;
}

DMI_DB_File_Service::DMI_DB_File_Service(DMI_DB_Parser *p)
{
  _parser = p;
}

DMI_DB_File_Service::~DMI_DB_File_Service()
{
  _parser = NULL;
}

FILE* DMI_DB_File_Service::create_file(FileType ftype, RWCString file_base_name)
{
  static RWCString fullname;


  fullname = _db_location + "/" + file_base_name;
  switch(ftype){
	case MIF_FILE_TYPE:
		fullname += ".mif";
		break;
	case COMP_FILE_TYPE:
		fullname += ".comp";
		break;
	case TABLE_FILE_TYPE:
		fullname += ".tbl";
		break;
	case GROUP_FILE_TYPE:
		fullname += ".grp";
  }
  return fopen(fullname,"w");
}

void DMI_DB_File_Service::delete_file(FileType ftype, RWCString file_base_name)
{
  static RWCString fullname;

  fullname = _db_location + "/" + file_base_name;
  switch(ftype){
	case MIF_FILE_TYPE:
		fullname += ".mif";
		break;
	case COMP_FILE_TYPE:
		fullname += ".comp";
		break;
	case TABLE_FILE_TYPE:
		fullname += ".tbl";
		break;
	case GROUP_FILE_TYPE:
		fullname += ".grp";
  }
  ::unlink(fullname);
}

FILE* DMI_DB_File_Service::open_file(RWCString filename,char* mode,int isMifFile)
{
  static RWCString fullname;
  
  /* if the filename doesn't contain slash */
  if(filename(0)=='/'){
	fullname = filename;
  }else{
  	if(!isMifFile){
  		fullname = _db_location + "/" + filename;
  	}else{
  		fullname = _mif_location + "/" + filename;
  	}
  }
  return fopen(fullname,mode);
}

void DMI_DB_File_Service::close_file(FILE *fp)
{
  if(!fp) return;
  fclose(fp);
}


unsigned hashNP(const DMI_DB_NP&np){ return np.getName().hash(); }

int DMI_DB_EnumList::find_value(RWCString name)
{
  if(name.length()==0)  return -1;
  for( int i=0; i<_npList->entries(); i++){
	DMI_DB_NP * np;
	np = _npList->at(i);
	if(np->getName().compareTo(name) == 0) 
		return(np->getValue());
  }
  return -1;
}

RWCString DMI_DB_EnumList::find_name(int value)
{
  for( int i=0; i<_npList->entries(); i++){
	DMI_DB_NP * np;
	np = _npList->at(i);
	if(np->getValue() == value) 
		return(np->getName());
  }
  static RWCString str("???");
  return str;
}

DMI_DB_EnumList *DMI_DB_Server::find_enum_list(RWCString name)
{
  if(name.length()==0) return NULL;
  for(int i=0; i<_enumLists->entries(); i++){
	DMI_DB_EnumList *list;
	list = _enumLists->at(i);
	if(list->getName().compareTo(name) == 0)
		return(list);
  }
  return NULL;
}

void print_rwcstring(RWCString name)
{
	cout << "THE STRING: " << name << endl;
}

DmiString *db_find_value_from_global_enum_list(Component *comp,DmiString *enum_name,int value)
{ 
	if(!comp) return NULL;
	RWTPtrSlist<GlobalEnum> *gblEnumList = comp->GetGlobalEnumList();
	if(!gblEnumList) return NULL;
	for(int i=0;i<gblEnumList->entries();i++){
	  GlobalEnum *gblEnum = gblEnumList->at(i);
	  if(!cmpDmiString(gblEnum->name,enum_name)){
		DmiEnumList_t *elist=gblEnum->enumList;
		if(!elist) return NULL;
		for(int j=0;j<elist->list.list_len;j++){
			if(elist->list.list_val[j].value==value)
				return(elist->list.list_val[j].name);
		}
	  }
	}
	return(NULL);
} 

DmiString *db_find_value_from_local_enum_list(DmiEnumList_t *elist,DmiString *enum_name,int value)
{ 
	if(!elist) return NULL;
	for(int j=0;j<elist->list.list_len;j++){
		if(elist->list.list_val[j].value==value)
			return(elist->list.list_val[j].name);
	}
	return(NULL);
} 

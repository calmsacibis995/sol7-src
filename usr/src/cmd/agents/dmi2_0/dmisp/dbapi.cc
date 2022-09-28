/* Copyright 10/14/96 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)dbapi.cc	1.13 96/10/14 Sun Microsystems"

#include "dbapi.hh"
#include "dmi_db.hh"
#include "dmi_print.hh"
#include <dirent.h>
#include <stdlib.h>
#include "dmi_error.hh"
#include "util.hh"
#include "search_util.hh"

static RWCString* id_2_rwstr(DmiId_t compId)
{
  static char buf[100];
  buf[0]='\0';
  sprintf(buf,"%ul",compId);
  RWCString *basename = new RWCString(buf);
  return basename;
}

DmiString* GetLanguageFromMif(int &opRes,char* miffilename)
{
  opRes = DMIERR_NO_ERROR;
  db_server.reset();
  if(db_parser.get_parse_state()==DMI_DB_Parser::PS_NoState){ 
	opRes = DMIERR_DB_NOT_INITIALIZE;
	return NULL;
  }
  /* set the grp parsing mode */
  db_parser.set_parse_state(DMI_DB_Parser::PS_LangState);
  if(db_parser.parse(miffilename,1)){
	/* free the memory */
	 opRes = DMIERR_PARSING_ERROR;
	 return NULL;
  }
  db_parser.set_parse_state(DMI_DB_Parser::PS_InitState);
  return db_server.get_dmi_lang_str();
}

/* 
 * This function will only create the component,
 * It will not store the component into database,
 * user needs to store it by calling WriteComponentToDB
 */
Component* CreateComponentFromMif(int &opRes,char* miffilename, DmiId_t compId)
{
  opRes = DMIERR_NO_ERROR;
  db_server.reset();
  if(db_parser.get_parse_state()==DMI_DB_Parser::PS_NoState){ 
	opRes = DMIERR_DB_NOT_INITIALIZE;
	return NULL;
  }
  if(db_parser.parse(miffilename,1)){
	/* free the memory */
	 opRes = DMIERR_PARSING_ERROR;
	 return NULL;
  }
  /* set compId */
  db_server.get_comp()->GetComponentInfo()->id = compId;

  return db_server.get_comp();
}

Component* CreateComponentFromMif(char* miffilename, DmiId_t compId)
{
  int ec;

 return(CreateComponentFromMif(ec,miffilename,compId));
}

Group* CreateGroup(int &opRes,char* grpfilename, DmiId_t groupId)
{
  opRes = DMIERR_NO_ERROR;
  db_server.reset();
  if(db_parser.get_parse_state()==DMI_DB_Parser::PS_NoState){ 
	opRes = DMIERR_DB_NOT_INITIALIZE;
	return NULL;
  }
  /* set the grp parsing mode */
  db_parser.set_parse_state(DMI_DB_Parser::PS_GrpState);
  if(db_parser.parse(grpfilename,1)){
	/* free the memory */
	 opRes = DMIERR_PARSING_ERROR;
	 return NULL;
  }
  db_server.get_group()->GetGroupInfo()->id = groupId;
  db_parser.set_parse_state(DMI_DB_Parser::PS_InitState);
  return db_server.get_group();

}

Group* CreateGroup(char* grpfilename, DmiId_t groupId)
{
  int ec;
  return(CreateGroup(ec,grpfilename,groupId));
}

RWTIsvSlist<Component>* ReadAllComponentsFromDB(int &opRes)
{
  static RWCString rwstr;
  opRes = DMIERR_NO_ERROR;
  if(db_parser.get_parse_state()==DMI_DB_Parser::PS_NoState){
	opRes = DMIERR_DB_NOT_INITIALIZE;
	return NULL;
  }
  /* for all the files under the dir. read its comp & table */
  rwstr = DMI_DB_File_Service::default_db_location();
  const char* dirname = (const char*)rwstr;
  DIR *dirp;
  struct dirent *direntp;
  if(!(dirp=opendir(dirname))) return NULL;
  RWTIsvSlist<Component>* complist = new RWTIsvSlist<Component>;
  while((direntp = readdir(dirp))){
	int pos;
	pos = strlen(direntp->d_name) - strlen(".comp");
	if(pos<0 || strcmp(&(direntp->d_name[pos]),".comp")) continue;
	direntp->d_name[pos]='\0';
	DmiId_t compId = atoi(direntp->d_name);
	Component *comp=ReadComponentFormDB(opRes,compId);
//	if(comp) complist->insert(comp);
	insertComp(complist, comp); 
  }
  closedir(dirp); 
  return complist;
}

RWTIsvSlist<Component>* ReadAllComponentsFromDB()
{
  int ec;
  return(ReadAllComponentsFromDB(ec));
}

Component* ReadComponentFormDB(int &opRes,DmiId_t compId)
{
  RWCString *prefix;
  RWCString *filename = new RWCString;
  opRes = DMIERR_NO_ERROR;
  if(db_parser.get_parse_state()==DMI_DB_Parser::PS_NoState){
	opRes = DMIERR_DB_NOT_INITIALIZE;
	return NULL;
  }
  /* look for files with the compId: compId.comp, compId.tbl */ 
  /* read comp then table */
  prefix = id_2_rwstr(compId);
  db_server.reset();
  *filename = *prefix + ".comp";
  if(db_parser.parse(*filename)){
	/* clean up */
	 opRes = DMIERR_PARSING_ERROR;
	 delete prefix;
	 delete filename; 
 	return NULL;
  }
  *filename = *prefix + ".tbl";
  /* *.tbl may not exist, here only parsing erro is counted */
  if(db_parser.parse(*filename)==E_PARSING_ERROR){
	/* clean up */
	 opRes = DMIERR_PARSING_ERROR;
	 delete prefix;
	 delete filename; 
 	return NULL;
  }
  db_server.get_comp()->GetComponentInfo()->id = compId; 
	 delete prefix;
	 delete filename; 
  return db_server.get_comp();
}

Component* ReadComponentFormDB(DmiId_t compId)
{
  int ec;
  return(ReadComponentFormDB(ec,compId));
}

/*
 * flag:0, write both comp and table
 * flag:1, write comp only
 * flag:2, write table only
 */
bool_t WriteComponentToDB(int &opRes,Component *comp,DB_Comp_Update flag)
{
  RWCString *basename;
  opRes = DMIERR_NO_ERROR;
  if(!comp) return FALSE;
  FILE *fp1;
  basename = id_2_rwstr(comp->GetComponentInfo()->id);
  if(flag == DB_UPDATE_ALL || flag == DB_UPDATE_COMP_ONLY){
  	fp1 = DMI_DB_File_Service::create_file(DMI_DB_File_Service::COMP_FILE_TYPE,*basename);
  	if(!fp1){
		opRes = DMIERR_FILE_ERROR;
		return NULL;
	}
  	print_component_and_group_only(fp1,comp,0);
  	DMI_DB_File_Service::close_file(fp1);
  }
  if(flag == DB_UPDATE_ALL || flag == DB_UPDATE_TABLE_ONLY){
    if(comp != NULL &&
       comp->GetTables() != NULL &&
       comp->GetTables()->entries()>0 ){
  		fp1 = DMI_DB_File_Service::create_file(DMI_DB_File_Service::TABLE_FILE_TYPE,*basename);
  		if(!fp1){
			opRes = DMIERR_FILE_ERROR;
			return NULL;
		}
  		print_table_only(fp1,comp,0);
  		DMI_DB_File_Service::close_file(fp1);
    }else{
	/* no table => delete the *.tbl */
  	DMI_DB_File_Service::delete_file(DMI_DB_File_Service::TABLE_FILE_TYPE,*basename);
    }
  }
}

bool_t WriteComponentToDB(Component *comp,DB_Comp_Update flag)
{
  int ec;
  return(WriteComponentToDB(ec,comp,flag));
}

bool_t DeleteComponentFormDB(int &opRes,DmiId_t compId)
{
  RWCString *basename;
  opRes = DMIERR_NO_ERROR;
  if(db_parser.get_parse_state()==DMI_DB_Parser::PS_NoState){
	opRes = DMIERR_DB_NOT_INITIALIZE;
	return FALSE;
  }
  basename = id_2_rwstr(compId);
  DMI_DB_File_Service::delete_file(DMI_DB_File_Service::COMP_FILE_TYPE,*basename);
  DMI_DB_File_Service::delete_file(DMI_DB_File_Service::TABLE_FILE_TYPE,*basename);

  return TRUE;
}

bool_t DeleteComponentFormDB(DmiId_t compId)
{
  int ec;
  return(DeleteComponentFormDB(ec,compId));
}

bool_t InitDMIDBServer(int &opRes,char* dirname,char* MifDir)
{
  opRes = DMIERR_NO_ERROR;
  if(!dirname) return FALSE;
  DIR *dirp = opendir(dirname);
  if(!dirp) { 
	opRes = DMIERR_DB_DIR_NOT_EXIST;
	return FALSE;
  }
  closedir(dirp); 
  if(dirname) DMI_DB_File_Service::default_db_location(dirname);
  if(MifDir) DMI_DB_File_Service::default_mif_location(MifDir);
  db_server.reset();
  db_parser.set_parse_state(DMI_DB_Parser::PS_InitState);
  RWTPtrHashTable<DMI_DB_NP>* enumTbl = new RWTPtrHashTable<DMI_DB_NP>(hashNP);
  db_server.set_gbl_enum_tbl(enumTbl);
  return TRUE;
}

bool_t InitDMIDBServer(char* dirname,char* MifDir)
{
  int ec;
  return(InitDMIDBServer(ec,dirname,MifDir));
}

bool_t InitDMIDBServer(char* dirname)
{
  int ec;
  return(InitDMIDBServer(ec,dirname));
}


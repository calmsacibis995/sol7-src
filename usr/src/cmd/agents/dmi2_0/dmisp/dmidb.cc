#include <stdio.h>
#include <stdlib.h>
#include "dmi_db.hh"
#include "dmi.hh"
#include "dmi_print.hh"
#include "dbapi.hh"
#include "util.hh"



/* usage: dmidb srcfilename, basename, target file type
 */
main(int argc, char*argv[])
{
  int ec;

  if(argv[1][0] == '-' && argv[1][1] == 'h'){
	cout << "Usage: dmidb srcfilename basename tgtfiletype(mif:0,compgrp:1,tbl:2,grp:3,both1&2:4,deltecomp:5,deleteTbl:6,readComp:7,readall:8,print)" << endl;
  }else{

  InitDMIDBServer(ec,"/home/jynet/ws/project/dmi2.0/test/db",
	             "/home/jynet/ws/project/dmi2.0/test/mif");

  FILE *fp1, *fp2; 
  RWCString basename(argv[2]);
  int compId = atoi(argv[2]);
  RWCString parse_file(argv[1]);

  int compId2 = 0;
  if(argc ==7 ) compId2 = atoi(argv[6]);

  if(argv[3][0] == '0'){
  	db_parser.parse(parse_file);
  	fp1 = DMI_DB_File_Service::create_file(DMI_DB_File_Service::MIF_FILE_TYPE,basename);
  	print_mif(fp1,db_server.get_comp(),0);
  }else if(argv[3][0] == '1'){
  	fp1 = DMI_DB_File_Service::create_file(DMI_DB_File_Service::COMP_FILE_TYPE,basename);
  	print_component_and_group_only(fp1,db_server.get_comp(),0);
  }else if(argv[3][0] == '2'){
  	db_parser.parse(argv[1]);
  	fp1 = DMI_DB_File_Service::create_file(DMI_DB_File_Service::TABLE_FILE_TYPE,basename);
  	print_table_only(fp1,db_server.get_comp(),0);
  }else if(argv[3][0] == '3'){
	Group *grp = CreateGroup(ec,argv[1],0);
	if(argc > 4){
  		fp1 = DMI_DB_File_Service::create_file(DMI_DB_File_Service::GROUP_FILE_TYPE,basename);
  		print_group(fp1,grp,0);
	}
  }else if(argv[3][0]== '4'){
	CreateComponentFromMif(ec,argv[1],compId);
	if(argc > 4){
  		fp1 = DMI_DB_File_Service::create_file(DMI_DB_File_Service::COMP_FILE_TYPE,basename);
  		print_component_and_group_only(fp1,db_server.get_comp(),0);
  		fp1 = DMI_DB_File_Service::create_file(DMI_DB_File_Service::TABLE_FILE_TYPE,basename);
  		print_table_only(fp1,db_server.get_comp(),0);
	}
  	if(argc ==7 ) CreateComponentFromMif(ec,argv[5],compId2);
  }else if(argv[3][0]== '5'){
	/* delete the comp file */
	DeleteComponentFormDB(ec,compId);
  }else if(argv[3][0]== '6'){
	/* delete the tbl file */
	DMI_DB_File_Service::delete_file(DMI_DB_File_Service::TABLE_FILE_TYPE,basename);
  }else if(argv[3][0]== '7'){
	Component *comp=ReadComponentFormDB(ec,compId);
	if(argc > 4){
	WriteComponentToDB(ec,comp,DB_UPDATE_ALL);
	}
  }else if(argv[3][0]== '8'){
	RWTIsvSlist<Component> *complist;
	if( (complist=ReadAllComponentsFromDB(ec)) ){
		/* print backup */
  		InitDMIDBServer(ec,"/home/jynet/ws/project/dmi2.0/dmidb/db");
		for(int i=0;i<complist->entries();i++)
		  WriteComponentToDB(ec,complist->at(i),DB_UPDATE_ALL);
		  		
	}
	complist->clearAndDestroy();
  }else if(argv[3][0]== '9'){
	int opRes;
        DmiString* str = GetLanguageFromMif(opRes,argv[1]); 
	printDmiString(str);
  }
  }
}


/* Copyright 09/11/96 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)dmi_db.hh	1.7 96/09/11 Sun Microsystems"

#ifndef _DMI_DB_HH_
#define _DMI_DB_HH_

#include <stdio.h>
#include "dmi.hh"
#include <rw/cstring.h>
#include <rw/hashtab.h>
#include <rw/tphasht.h>

enum DMI_DB_ErrorType {
	E_NO_ERROR,
	E_FILE_NOT_EXIST,
 	E_FILE_CANT_OPEN,
	E_PARSING_ERROR
};

class DMI_DB_NP : public RWIsvSlink{
public:
	DMI_DB_NP(RWCString name,int value){
		_name = name; _value = value;
	}
	DMI_DB_NP(const DMI_DB_NP &np){ 
		_name = np.getName(); _value = np.getValue();
 	}
	RWBoolean operator==(const DMI_DB_NP &np){
	  if(np.getName() == _name) return TRUE;
	  else return FALSE;
	}
	virtual ~DMI_DB_NP(){}
	void setName(RWCString name){ _name = name; }
	const RWCString getName() const { return _name; }
	void setValue(int value){ _value = value; }
	const int getValue() const { return _value; }
private:
	RWCString _name;
	int _value;
};

class DMI_DB_EnumList : public RWIsvSlink {
public:
	DMI_DB_EnumList(){ _name = "unknown"; _npList=NULL; }
	DMI_DB_EnumList(RWCString name){ _name = name; _npList=NULL; }
	virtual ~DMI_DB_EnumList(){ if(_npList) _npList->clearAndDestroy(); }
	void setName(RWCString name){ _name = name; }
	RWCString getName(){ return _name; }
	int find_value(RWCString name);
	RWCString find_name(int value);
	
	RWCString _name;
	RWTIsvSlist<DMI_DB_NP> *_npList;
};

class DMI_DB_Parser
{
public:

  //Types
  enum ParseState {
	PS_NoState,
	PS_InitState,
	PS_GrpState,
	PS_TblState,
	PS_LangState
  };

  //Constructor
  DMI_DB_Parser();
  virtual ~DMI_DB_Parser(){}

  //Operations
  int parse(char* filename,int isMifFile=0);
  int parse(RWCString filename,int isMifFile=0);
  void set_parse_state(ParseState ps);
  ParseState get_parse_state(){ return _parse_state; }
  RWCString getFilename(){return _filename; }

private:
  DMI_DB_Parser(DMI_DB_Parser &); //not allowed
  void operator=(const DMI_DB_Parser&); //not allowed

  //Data
  
  RWHashTable		*_enum_table; //scoped tables
  long			_lineno;
  RWCString		_filename;
  ParseState		_parse_state;
  FILE*			_fp;
  //CodeGenerator

};


class DMI_DB_File_Service {
public:

  enum FileType {
	UNKNOW_FILE_TYPE,
	MIF_FILE_TYPE,
	COMP_FILE_TYPE,
	TABLE_FILE_TYPE,
	GROUP_FILE_TYPE,
	ASCII_FILE_TYPE,
	BINARY_FILE_TYPE
  };


  DMI_DB_File_Service(DMI_DB_Parser* p);
  virtual ~DMI_DB_File_Service();

  static FILE *create_file(FileType ftype, RWCString file_base_name);
  static void delete_file(FileType ftype,RWCString file_base_name);
  static FILE *open_file(RWCString filename,char* mode,int isMifFile=0);
  static void close_file(FILE *fp);
  static void default_db_location(RWCString dir){ _db_location = dir; }
  static RWCString default_db_location(){ return _db_location; }
  static void default_mif_location(RWCString dir){ _mif_location = dir; }
  static RWCString default_mif_location(){ return _mif_location; }
  int write(FILE* fp, void* data); // write data to file
  int read(FILE* fp, void* data); // read and parse the file

private:
  DMI_DB_Parser	*_parser;
  static RWCString _db_location;
  static RWCString _mif_location;
	
};

extern unsigned hashNP(const DMI_DB_NP&);

class DMI_DB_Server {
public:
  DMI_DB_Server(){
  _theComponent=0; 
  _theGroup=0;
  _theAttribute=0;
  _theTable=0;
  _theRow=0;
  _rowCol=0;
  _enumLists=0;
  _theEnum=0;
  _langStr = 0;
  }
  virtual ~ DMI_DB_Server(){ if(!_glbEnumTbl) delete _glbEnumTbl; }

  //load function for returning list of components
  //read from the database -- given the db dir, find all
  //files with prefix .tbl or .comp, the file name has to
  //be unique, otherwise db corruption.

  void set_comp(Component* comp){ _theComponent = comp; }
  Component* get_comp(){ return _theComponent; }

  void set_group(Group *grp){ _theGroup = grp; }
  Group* get_group(){ return _theGroup; }

  void set_attr(Attribute* attr){ _theAttribute = attr; }
  Attribute* get_attr(){ return _theAttribute; }

  void set_table(Table* table){ _theTable = table; }
  Table* get_table(){return _theTable; }

  void set_row(DmiAttributeValues_t* row){ _theRow = row; }
  DmiAttributeValues_t* get_row(){return _theRow;}

  void set_row_col(int rowcol){ _rowCol = rowcol; }
  void inc_row_col(){ _rowCol++; }
  int get_row_col(){return _rowCol;}


  void set_gbl_enum_tbl(RWTPtrHashTable<DMI_DB_NP>* tbl){
	_glbEnumTbl = tbl;
  }
  RWTPtrHashTable<DMI_DB_NP> *get_gbl_enum_tbl(){
	return _glbEnumTbl;
  }

  DMI_DB_EnumList *find_enum_list(RWCString name); 
  
  void reset(){
  _theComponent=0; 
  _theGroup=0;
  _theAttribute=0;
  _theTable=0;
  _theRow=0;
  _rowCol=0;
  _enumLists=0;
  _theEnum=0;
  _langStr = 0;
  }
  
  void set_enum_lists(RWTIsvSlist<DMI_DB_EnumList> *list){
	_enumLists = list;
  }
  RWTIsvSlist<DMI_DB_EnumList> *get_enum_lists(){
	return _enumLists;
  }

  void set_enum(DMI_DB_EnumList *aEnum){ _theEnum = aEnum; }
  DMI_DB_EnumList *get_enum(){ return _theEnum; }

  void set_dmi_lang_str(DmiString* str){ _langStr=str; }
  DmiString *get_dmi_lang_str(){ return _langStr; }

private:
	// working elements
  Component *_theComponent; 
  Group* _theGroup;
  Attribute* _theAttribute;
  Table* _theTable;
  DmiAttributeValues_t* _theRow;
  int _rowCol;
  RWTPtrHashTable<DMI_DB_NP> *_glbEnumTbl;
  RWTIsvSlist<DMI_DB_EnumList> *_enumLists;
  DMI_DB_EnumList *_theEnum;
  DmiString *_langStr;
};


#define DBComp (db_server.get_comp())
#define DBSComp(comp) (db_server.set_comp(comp))
#define DBGroup (db_server.get_group())
#define DBSGroup(group) (db_server.set_group(group))
#define DBAttr (db_server.get_attr())
#define DBSAttr(attr) (db_server.set_attr(attr))
#define DBTable (db_server.get_table())
#define DBSTable(table) (db_server.set_table(table))
#define DBRow (db_server.get_row())
#define DBSRow(row) (db_server.set_row(row))
#define DBCol (db_server.get_row_col())
#define DBSCol(idx) (db_server.set_row_col(idx))
#define DBIncCol (db_server.inc_row_col())
#define DBEnumLists (db_server.get_enum_lists())
#define DBSEnumLists(list) (db_server.set_enum_lists(list))
#define DBAEnum (db_server.get_enum())
#define DBSAEnum(aEnum)	(db_server.set_enum(aEnum))



extern DMI_DB_Server db_server;
extern DMI_DB_Parser db_parser;


// There must be a singleton for the database Server object
// which stores global information, it is NULL until the user
// called the api or call the connect api

extern DmiString *db_find_value_from_global_enum_list(Component *,DmiString *,int);
extern DmiString *db_find_value_from_local_enum_list(DmiEnumList_t*,DmiString *,int);


#endif

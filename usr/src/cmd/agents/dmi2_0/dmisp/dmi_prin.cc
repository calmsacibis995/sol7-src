/* Copyright 09/11/96 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)dmi_print.cc	1.5 96/09/11 Sun Microsystems"

#include "dmi.hh"
#include "dmi_db.hh"
#include "dmi_print.hh"

static Component *cur_comp=NULL;

#define PRT_STR(st,cxt) \
	fprintf(st,"%s",cxt);
/*
	printf("%s",cxt);
*/

#define PRT_CHAR(st,cxt) \
	fprintf(st,"%c",cxt);
/*
	printf("%c",cxt);
*/

#define PRT_INT(st,cxt)\
	fprintf(st,"%d",cxt);
/*
	printf("%d",cxt);
*/

#define PRT_FMT1(st,fmt,cxt)\
	fprintf(st,fmt,cxt);
/*
	printf(fmt,cxt);
*/

#define PRT_FMT2(st,fmt,cxt,cxt2)\
	fprintf(st,fmt,cxt,cxt2);
/*
	printf(fmt,cxt,cxt2);
*/

#define PRT_QUOTE(st)\
	fprintf(st,"\"");
/*
	printf("\"");
*/

#define PRT_NEWLINE(st)\
	fprintf(st,"\n");
/*
	printf("\n");
*/

#define PRT_OBRACE(st)\
	fprintf(st,"{");
/*
	printf("{");
*/

#define PRT_CBRACE(st)\
	fprintf(st,"}");
/*
	printf("}");
*/

#define PRT_COMA(st)\
	fprintf(st,",");
/*
	printf(",");
*/


void print_indent(StoreType st,int n)
{
  for(int i=0; i<n ; i++)
	PRT_STR(st,"\t");
}

void print_dmi_string(StoreType st,DmiString *str)
{
  if(str){
    PRT_QUOTE(st)
    for(int j= 0; j<str->body.body_len; j++){
  	PRT_CHAR(st,str->body.body_val[j])
    }
    PRT_QUOTE(st)
  }
}

void print_dmi_date(StoreType st,DmiTimestamp_t *timestamp)
{
  if(timestamp && timestamp->dot){
    PRT_QUOTE(st)
    for(int i=0; i<4; i++){
	PRT_CHAR(st,timestamp->year[i])
    }
    PRT_CHAR(st,timestamp->month[0])
    PRT_CHAR(st,timestamp->month[1])
    PRT_CHAR(st,timestamp->day[0])
    PRT_CHAR(st,timestamp->day[1])
    PRT_CHAR(st,timestamp->hour[0])
    PRT_CHAR(st,timestamp->hour[1])
    PRT_CHAR(st,timestamp->minutes[0])
    PRT_CHAR(st,timestamp->minutes[1])
    PRT_CHAR(st,timestamp->seconds[0])
    PRT_CHAR(st,timestamp->seconds[1])
    PRT_CHAR(st,timestamp->dot)
    PRT_CHAR(st,timestamp->microSeconds[0])
    PRT_CHAR(st,timestamp->microSeconds[1])
    PRT_CHAR(st,timestamp->microSeconds[2])
    PRT_CHAR(st,timestamp->microSeconds[3])
    PRT_CHAR(st,timestamp->microSeconds[4])
    PRT_CHAR(st,timestamp->microSeconds[5])
    PRT_CHAR(st,timestamp->plusOrMinus)
    PRT_CHAR(st,timestamp->utcOffset[0])
    PRT_CHAR(st,timestamp->utcOffset[1])
    PRT_CHAR(st,timestamp->utcOffset[2])
    PRT_QUOTE(st)
  }else{
    PRT_QUOTE(st)
    PRT_QUOTE(st)
  }
}

void print_name_pair(StoreType st,char* name,DmiString* str,int indent)
{
  print_indent(st,indent);
  PRT_FMT1(st,"%s = ",name);
  print_dmi_string(st,str);
  PRT_NEWLINE(st)
  
}

void print_rev_name_pair(StoreType st,int value,DmiString* str,int indent)
{
  print_indent(st,indent);
  PRT_FMT1(st,"%d = ",value);
  print_dmi_string(st,str);
  PRT_NEWLINE(st)
  
}

void print_name_pair(StoreType st,char* name,DmiId_t num,int indent)
{
  print_indent(st,indent);
  PRT_FMT2(st,"%s = %d",name,num);
  PRT_NEWLINE(st)
  
}

void print_name_pair(StoreType st,char* name,DmiAttributeIds *keyList,
			int indent)
{
  print_indent(st,indent);
  PRT_FMT1(st,"%s = ",name);
  if( (keyList->list).list_len > 0)
	PRT_FMT1(st,"%d",(keyList->list).list_val[0])
  for(int i=1; i< (keyList->list).list_len;i++)
	PRT_FMT1(st,", %d",(keyList->list).list_val[i])
  PRT_NEWLINE(st)
  
}

void print_name_pair(StoreType st,char* name,DmiAccessMode mode,
			int indent)
{
  print_indent(st,indent);
  PRT_FMT1(st,"%s = ",name);
  switch(mode){
	case MIF_READ_ONLY:
		PRT_FMT1(st,"%s","READ-ONLY")
		break;
	case MIF_READ_WRITE:
		PRT_FMT1(st,"%s","READ-WRITE")
		break;
	case MIF_WRITE_ONLY:
		PRT_FMT1(st,"%s","WRITE-ONLY")
		break;
  }
  PRT_NEWLINE(st)
  
}

void print_name_pair(StoreType st,char* name,DmiDataType_t datatype,
			DmiUnsigned_t size,
			int indent)
{
  print_indent(st,indent);
  PRT_FMT1(st,"%s = ",name);
  switch(datatype){
	case MIF_COUNTER:
		PRT_FMT1(st,"%s","Counter")
		break;
	case MIF_COUNTER64:
		PRT_FMT1(st,"%s","Counter64")
		break;
	case MIF_GAUGE:
		PRT_FMT1(st,"%s","Gauge")
		break;
	case MIF_INTEGER:
		/* if enum_flag is true,
		   print the type name from enum_name */
		PRT_FMT1(st,"%s","Integer")
		break;
	case MIF_INTEGER64:
		PRT_FMT1(st,"%s","Integer64")
		break;
	case MIF_OCTETSTRING:
		PRT_FMT1(st,"%s","OctetString")
		if(size>0)
			PRT_FMT1(st,"(%d)",size)
		break;
	case MIF_DISPLAYSTRING:
		PRT_FMT1(st,"%s","String")
		if(size>0)
			PRT_FMT1(st,"(%d)",size)
		break;
	case MIF_DATE:
		PRT_FMT1(st,"%s","Date")
		break;
  }
  PRT_NEWLINE(st)
  
}

void print_name_pair(StoreType st,char* name,DmiStorageType stype,
			int indent)
{
  print_indent(st,indent);
  PRT_FMT1(st,"%s = ",name);
  switch(stype){
	case MIF_COMMON:	
		PRT_FMT1(st,"%s","Common")
		break;
	case MIF_SPECIFIC:
		PRT_FMT1(st,"%s","Specific")
		break;
  }
  PRT_NEWLINE(st)
}

void print_name_pair(StoreType st, char* name, DmiDataUnion_t *du,
			int indent)
{
  switch(du->type){
	case MIF_COUNTER:
  		print_indent(st,indent);
  		PRT_FMT1(st,"%s = ",name)
		PRT_FMT1(st,"%d",du->DmiDataUnion_u.counter)
		break;
	case MIF_COUNTER64:
		break;
	case MIF_GAUGE:
  		print_indent(st,indent);
  		PRT_FMT1(st,"%s = ",name)
		PRT_FMT1(st,"%d",du->DmiDataUnion_u.gauge)
		break;
	case MIF_INTEGER:
		/* not yet taking care of enum type 
		   get the type name from enum_name,
		   value should convert to corresponding str */
  		print_indent(st,indent);
  		PRT_FMT1(st,"%s = ",name)
		PRT_FMT1(st,"%d",du->DmiDataUnion_u.integer)
		break;
	case MIF_INTEGER64:
		break;
	case MIF_OCTETSTRING:
		if(du->DmiDataUnion_u.octetstring){
  			print_indent(st,indent);
  			PRT_FMT1(st,"%s = ",name)
			print_dmi_string(st,(DmiString*)(du->DmiDataUnion_u.octetstring));
		}
		break;
	case MIF_DISPLAYSTRING:
		if(du->DmiDataUnion_u.str){
  			print_indent(st,indent);
  			PRT_FMT1(st,"%s = ",name)
			print_dmi_string(st,du->DmiDataUnion_u.str);
		}
		break;
	case MIF_DATE:
		if(du->DmiDataUnion_u.date){
  			print_indent(st,indent);
  			PRT_FMT1(st,"%s = ",name)
			print_dmi_date(st,du->DmiDataUnion_u.date);
		}
		break;
  }
  PRT_NEWLINE(st)
}

void print_data_union_value(StoreType st, DmiDataUnion_t *du,
			int indent)
{
  switch(du->type){
	case MIF_COUNTER:
		PRT_FMT1(st,"%d",du->DmiDataUnion_u.counter)
		break;
	case MIF_COUNTER64:
		break;
	case MIF_GAUGE:
		PRT_FMT1(st,"%d",du->DmiDataUnion_u.gauge)
		break;
	case MIF_INTEGER:
		PRT_FMT1(st,"%d",du->DmiDataUnion_u.integer)
		break;
	case MIF_INTEGER64:
		break;
	case MIF_OCTETSTRING:
		print_dmi_string(st,(DmiString*)(du->DmiDataUnion_u.octetstring));
		break;
	case MIF_DISPLAYSTRING:
		print_dmi_string(st,du->DmiDataUnion_u.str);
		break;
	case MIF_DATE:
		print_dmi_date(st,du->DmiDataUnion_u.date);
		break;
  }
}

void print_row(StoreType st,DmiAttributeValues_t *row,int indent)
{
  u_int num_of_col;
 
  if(!row) return;
  num_of_col = row->list.list_len;
  if(!num_of_col) return;
  print_indent(st,indent);
  PRT_OBRACE(st)
  print_data_union_value(st,&(row->list.list_val[0].data),indent);
  for(int i=1; i<num_of_col; i++){
	PRT_COMA(st)
	print_data_union_value(st,&(row->list.list_val[i].data),indent);
  }
  PRT_CBRACE(st)
}

void print_attr(StoreType st,Attribute *attr,int indent)
{
  DmiAttributeInfo_t *attr_info;
  DmiDataUnion_t *attr_data;

  if(!attr) return;
  if(!(attr_info=attr->GetInfo())) return;
  print_indent(st,indent);
  PRT_STR(st,"start attribute\n")
  indent++;
  if(attr_info->name)
	print_name_pair(st,"name",attr_info->name,indent);
  if(attr_info->id>0)
	print_name_pair(st,"id",attr_info->id,indent);
  if(attr_info->description)
	print_name_pair(st,"description",attr_info->description,indent);	
  if(attr_info->access == MIF_READ_ONLY ||
     attr_info->access == MIF_READ_WRITE ||
     attr_info->access == MIF_WRITE_ONLY)
	print_name_pair(st,"access",attr_info->access,indent);
  if(attr_info->storage == MIF_COMMON || attr_info->storage == MIF_SPECIFIC)
	print_name_pair(st,"storage",attr_info->storage,indent);

  if(attr->GetEnumFlag() && !attr_info->enumList){
	/* type is enum_name */
	print_name_pair(st,"type",attr->GetEnumName(),indent);
  }else if(attr->GetEnumFlag() && attr_info->enumList){
	/* print start enum ... */
	if(attr->GetEnumName())
	  print_name_pair(st,"type",attr->GetEnumName(),indent);
  	print_indent(st,indent);
	PRT_STR(st,"type = Start Enum");
  	PRT_NEWLINE(st)
	/* print the enum name */
	for(int j=0;j<attr_info->enumList->list.list_len;j++){
	  print_rev_name_pair(st,attr_info->enumList->list.list_val[j].value,
		attr_info->enumList->list.list_val[j].name,indent);
	}
	PRT_STR(st,"End Enum");
  	PRT_NEWLINE(st)
  }else if(attr_info->type > MIF_DATATYPE_0 && attr_info->type <= MIF_DATE){
	print_name_pair(st,"type",attr_info->type,attr_info->maxSize,indent);
  }
	
  if(attr_data=attr->GetData()){
    if(attr->GetEnumFlag() && !attr_info->enumList){
	DmiString *str;
	str = db_find_value_from_global_enum_list(cur_comp,attr->GetEnumName(),
	attr_data->DmiDataUnion_u.integer);
	if(str) print_name_pair(st,"value",str,indent);
    }else if(attr->GetEnumFlag() && attr_info->enumList){
	DmiString *str;
	str = db_find_value_from_local_enum_list(attr_info->enumList,attr->GetEnumName(),
	attr_data->DmiDataUnion_u.integer);
	if(str) print_name_pair(st,"value",str,indent);
    }else 
	print_name_pair(st,"value",attr_data,indent);
  }

  indent--;
  print_indent(st,indent);
  PRT_STR(st,"end attribute\n")
}

void print_table(StoreType st,Table *table,int indent)
{
  TableInfo *table_info;

  if(!table) return;
  if(!(table_info=table->GetTableInfo())) return;
  print_indent(st,indent);
  PRT_STR(st,"start table\n")
  indent++;
  if(table_info->name)
	print_name_pair(st,"Name",table_info->name,indent);
  if(table_info->className)
	print_name_pair(st,"class",table_info->className,indent);
  if(table_info->id>0)
	print_name_pair(st,"id",table_info->id,indent);

  /* print rows */
  PRT_NEWLINE(st)
  RWTPtrSlist<DmiAttributeValues_t> *rows = table->GetRows();
  if(rows && rows->entries()>0){
	DmiAttributeValues_t *row;
	for(int i=0; i<rows->entries(); i++){
		print_row(st,rows->at(i),indent);
  		PRT_NEWLINE(st)
	}
  }
  
  indent--;
  print_indent(st,indent);
  PRT_STR(st,"end table\n")
}

void print_group(StoreType st,Group *group,int indent)
{
  DmiGroupInfo_t *group_info;

  if(!group) return;
  if(!(group_info=group->GetGroupInfo())) return;
  print_indent(st,indent);
  PRT_STR(st,"start group\n")
  indent++;
  if(group_info->name)
	  print_name_pair(st,"Name",group_info->name,indent);
  if(group_info->id>0)
	  print_name_pair(st,"id",group_info->id,indent);
  if(group_info->className)
	  print_name_pair(st,"class",group_info->className,indent);
  if(group_info->description)
	  print_name_pair(st,"description",group_info->description,indent);
  if(group_info->keyList)
	  print_name_pair(st,"key",group_info->keyList,indent);
	
  /* print attr */
  PRT_NEWLINE(st)
  indent++;
  RWTIsvSlist<Attribute> *attrs = group->GetAttributes();
  if(attrs && attrs->entries()>0){
	Attribute *attr;
	for(int i=0; i<attrs->entries(); i++){
		print_attr(st,attrs->at(i),indent);
  		PRT_NEWLINE(st)
	}
  }
  indent--;
  
  indent--;
  print_indent(st,indent);
  PRT_STR(st,"end group\n")
}


void print_component_and_group_only(StoreType st,Component *comp,int indent)
{
  if(!comp) return;
  cur_comp = comp;
  RWTPtrSlist<DmiString> *comp_lang = comp->GetLanguages();

  if(comp_lang){
	DmiString *str;
	PRT_STR(st,"Language = ")
  	for(int i=0; i<comp_lang->entries(); i++){
		str = comp_lang->at(i);	
		print_dmi_string(st,str);
	}
	PRT_NEWLINE(st)
  }


  /* print enum list */
  

  DmiComponentInfo_t *comp_info = comp->GetComponentInfo();
  if(comp_info){
	PRT_STR(st,"Start Component\n");
	indent++;
	if(comp_info->name)
	  print_name_pair(st,"Name",comp_info->name,indent);
	if(comp_info->description)
	  print_name_pair(st,"Description",comp_info->description,indent);
	if(comp_info->pragma)
	  print_name_pair(st,"Pragma",comp_info->pragma,indent);
  }

  RWTPtrSlist<GlobalEnum> *glbEnumList = comp->GetGlobalEnumList();
  if(glbEnumList){
	for(int i=0;i<glbEnumList->entries();i++){
	  indent++;
	  PRT_STR(st,"Start Enum\n");
	  GlobalEnum *gEnum = glbEnumList->at(i);
	  if(gEnum->name)
	    print_name_pair(st,"Name",gEnum->name,indent);
	    for(int j=0;j<gEnum->enumList->list.list_len;j++){
		print_rev_name_pair(st,gEnum->enumList->list.list_val[j].value,
			gEnum->enumList->list.list_val[j].name,indent);
	    }
  	  PRT_STR(st,"End Enum\n")
	  indent--;
	}
  }

  PRT_NEWLINE(st)
  
  RWTIsvSlist<Group> *groups = comp->GetGroups();
  if(groups && groups->entries()>0){
	Group *group;
	for(int i=0; i<groups->entries(); i++){
		print_group(st,groups->at(i),indent);
	}
  }

  if(comp_info){
	indent--;
	PRT_STR(st,"END Component\n")
  }
}

void print_table_only(StoreType st,Component *comp,int indent)
{
  cur_comp = comp;
  RWTPtrSlist<Table> *tables = comp->GetTables();
  if(tables && tables->entries()>0){
	Table *table;
	for(int i=0; i<tables->entries(); i++){
		print_table(st,tables->at(i),indent);
	}
  }
}

void print_group_only(StoreType st,Component *comp,int indent)
{
  RWTIsvSlist<Group> *groups = comp->GetGroups();
  cur_comp = comp;
  if(groups && groups->entries()>0){
	Group *group;
	for(int i=0; i<groups->entries(); i++){
		print_group(st,groups->at(i),indent);
	}
  }
}

void print_mif(StoreType st,Component *comp,int indent)
{
  if(!comp) return;
  RWTPtrSlist<DmiString> *comp_lang = comp->GetLanguages();
  cur_comp = comp;

  if(comp_lang){
	DmiString *str;
	PRT_STR(st,"Language = ")
  	for(int i=0; i<comp_lang->entries(); i++){
		str = comp_lang->at(i);	
		print_dmi_string(st,str);
	}
	PRT_NEWLINE(st)
  }

  DmiComponentInfo_t *comp_info = comp->GetComponentInfo();
  if(comp_info){
	PRT_STR(st,"Start Component\n");
	indent++;
	if(comp_info->name)
	  print_name_pair(st,"Name",comp_info->name,indent);
	if(comp_info->description)
	  print_name_pair(st,"Description",comp_info->description,indent);
	if(comp_info->pragma)
	  print_name_pair(st,"Pragma",comp_info->pragma,indent);
  }

  PRT_NEWLINE(st)
  
  RWTIsvSlist<Group> *groups = comp->GetGroups();
  if(groups && groups->entries()>0){
	Group *group;
	for(int i=0; i<groups->entries(); i++){
		print_group(st,groups->at(i),indent);
	}
  }

  PRT_NEWLINE(st)

  RWTPtrSlist<Table> *tables = comp->GetTables();
  if(tables && tables->entries()>0){
	Table *table;
	for(int i=0; i<tables->entries(); i++){
		print_table(st,tables->at(i),indent);
	}
  }


  if(comp_info){
	indent--;
	PRT_STR(st,"END Component\n")
  }
}

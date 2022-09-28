
struct CiGetAttributeIN {
	DmiId_t		componentId; 
	DmiId_t		groupId; 
	DmiId_t		attributeId; 
	DmiString_t*		language; 
	DmiAttributeValues_t* 	keyList;
};

struct CiGetAttributeOUT {
	DmiErrorStatus_t error_status;
	DmiAttributeData_t*		data;
}; 

struct CiGetNextAttributeIN {
	DmiId_t		componentId; 
	DmiId_t		groupId; 
	DmiId_t		attributeId; 
	DmiString_t*		language; 
	DmiAttributeValues_t* 	keyList;
};
struct CiGetNextAttributeOUT {
	DmiErrorStatus_t error_status;
	DmiAttributeData_t*		data;
}; 

struct CiSetAttributeIN {
	DmiId_t		      componentId; 
	DmiId_t		      groupId; 
	DmiId_t		      attributeId; 
	DmiString_t*	  language; 
	DmiAttributeValues_t* 	keyList; 
	DmiAttributeData_t*		data;
}; 

struct CiReserveAttributeIN {
	DmiId_t		componentId; 
	DmiId_t		groupId; 
	DmiId_t		attributeId; 
	DmiAttributeValues_t* 	keyList; 
	DmiAttributeData_t*		data;
};

struct CiReleaseAttributeIN {
	DmiId_t		componentId; 
	DmiId_t		groupId; 
	DmiId_t		attributeId; 
	DmiAttributeValues_t* 	keyList; 
	DmiAttributeData_t*		data;
};

struct CiAddRowIN {
	DmiRowData_t*	rowData ;
};


struct CiDeleteRowIN {
	DmiRowData_t*		rowData ;
};


program DMI2_CI_CALLBACK {
	version DMI2_CI_CALLBACK_VERSION {
		CiGetAttributeOUT _CiGetAttribute (CiGetAttributeIN ) = 0x400;
		CiGetNextAttributeOUT  _CiGetNextAttribute ( CiGetNextAttributeIN ) = 0x401;
		DmiErrorStatus_t _CiSetAttribute ( CiSetAttributeIN ) = 0x402;
		DmiErrorStatus_t _CiReserveAttribute (CiReserveAttributeIN) = 0x403;
		DmiErrorStatus_t _CiReleaseAttribute (CiReleaseAttributeIN) = 0x404;
		DmiErrorStatus_t _CiAddRow (CiAddRowIN) = 0x405;
		DmiErrorStatus_t _CiDeleteRow (CiDeleteRowIN) = 0x406; 
	} = 0x1;
} = 0x20000000;

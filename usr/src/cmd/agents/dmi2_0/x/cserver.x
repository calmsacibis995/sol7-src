
struct DmiRegisterCiIN {
	DmiRegisterInfo_t*  regInfo;
/*	u_long prognum; */
};

struct DmiRegisterCiOUT {
	DmiErrorStatus_t error_status;
    DmiHandle_t* handle;
	DmiString_t* dmiSpecLevel; 
};


struct DmiUnregisterCiIN {
    DmiHandle_t  handle; 
};

struct DmiUnregisterCiOUT {
	DmiErrorStatus_t error_status;
};

struct DmiOriginateEventIN {
	DmiId_t		         compId; 
	DmiString_t*  		 language; 
	DmiTimestamp_t*		 timestamp; 
	DmiRowData_t*		 rowData; 
};

struct DmiOriginateEventOUT {
	DmiErrorStatus_t error_status;
};

program DMI2_CSERVER {
	version DMI2_CSERVER_VERSION {
		DmiRegisterCiOUT _DmiRegisterCi ( DmiRegisterCiIN ) = 0x300;
		DmiUnregisterCiOUT _DmiUnregisterCi ( DmiUnregisterCiIN ) = 0x301;
		DmiOriginateEventOUT _DmiOriginateEvent ( DmiOriginateEventIN ) = 0x302;
	} = 0x1;
} = 0x30000000;

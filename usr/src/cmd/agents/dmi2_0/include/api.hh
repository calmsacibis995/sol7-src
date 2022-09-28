#ifndef _API_HH
#define _API_HH

/* Copyright 07/23/96 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)api.hh	1.6 96/07/23 Sun Microsystems"

enum ServerType {
	MISERVER,
	CISERVER
};
typedef enum ServerType ServerType;

enum RpcType {
	DCE,
	ONC
};
typedef enum RpcType RpcType;

struct ConnectIN {
	char *host;
	const char *nettype;
	ServerType servertype;
	RpcType rpctype; 
}; 
typedef struct ConnectIN ConnectIN;

struct DmiRpcHandle {
	RpcType type;
	union {
		CLIENT *handle;
	} RpcHandle_u;
};
typedef struct DmiRpcHandle DmiRpcHandle; 

#ifdef __cplusplus
extern "C" {
#endif 	

extern bool_t ConnectToServer(ConnectIN *argp, DmiRpcHandle *dmi_handle); 
extern bool_t DisconnectToServer(DmiRpcHandle *dmi_handle); 

#ifdef __cplusplus
}
#endif 	
#endif

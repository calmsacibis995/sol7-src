/* Copyright 09/30/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)dpitosar.c	1.4 96/09/30 Sun Microsystems"

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <sys/socket.h>
/*#include <arpa/inet.h> */
#include <netdb.h> 
#include <netinet/in.h>
#include <snmp_dpi.h>

#include <sys/systeminfo.h>
#include <pdu.h>
#include <trap.h>
#include <pagent.h>
#include <subtree.h>
#include <snmp.h>
#include <asn1.h>
#include <syslog.h>


#include <dmisa.h> 
#include <dpitosar.h>
extern Address my_address;
extern char    error_label[1000];

int socket_handle;

int DPIconnect_to_agent_UDP(char *hostname_p , char *community_p ) {
    
int agentid;

     agentid = SSASubagentOpen(NUM_AGENTRELAY_RETRIES, DMI_SUBAGENT_NAME); 
   
     if (agentid == 0) {
         return DPI_RC_NO_CONNECTION;
     }
     return (agentid);
}

int agent_alive() {
     return (SSAGetTrapPort());
}

int subagent_register(Agent *subagent) {

     return(SSARegSubagent(subagent));
} 

int subagent_unregister(Agent *subagent) {

     subagent->agent_status  = SSA_OPER_STATUS_DESTROY;
     return(SSARegSubagent(subagent));
} 

int subtree_register(int agentid, char *oidstr, int treeindex, int timeout, 
                      long priority, char bulk_select, char view_select ) {
SSA_Subtree subtree;
char *oidstrd;
Oid  *oidp;
int len , ret;

       memset((char *) &subtree, 0, sizeof(subtree));
       subtree.regTreeIndex = treeindex;
       subtree.regTreeAgentID = agentid;
       subtree.regTreeStatus  = SSA_OPER_STATUS_ACTIVE;
       oidstrd = strdup(oidstr);
        
/*This is done to remove the trailing dot that is passed by the IBM code which is not required in the agent relay stuff */
       len = strlen(oidstrd);
       if (oidstrd[len-1] == '.') oidstrd[len-1] = 0;

       oidp = SSAOidStrToOid(oidstrd,error_label);
       
       if (!oidp) return 0;
       subtree.name = *oidp;
       free(oidstrd);
       ret= SSARegSubtree(&subtree);
       SSAOidFree(oidp);
       return( ret );
       
         
}

int subtree_unregister(int agentid, char *oidstr, int treeindex) {

SSA_Subtree subtree;

      memset((char *)&subtree, 0, sizeof(subtree));
      subtree.regTreeIndex = treeindex;
      subtree.regTreeAgentID = agentid;
      subtree.regTreeStatus = SSA_OPER_STATUS_NOT_IN_SERVICE;
      
      return( SSARegSubtree(&subtree));
}

int open_socket(struct sockaddr_in *in) {

int  port_num = SUBAGENT_PORT;
struct hostent  *hp;
struct hostent  hpent;
char hostname[100];
char buff[1024];
char errmsg[100];
int  herr;


     socket_handle = socket(AF_INET,SOCK_DGRAM,0) ;
     if (socket_handle < 0) {
        sprintf(errmsg, "Unable to open Datagram Socket error = %d",errno);
        syslog(LOG_ERR, errmsg);
        return 0;
     }
     in->sin_addr.s_addr = htonl(INADDR_ANY) ;
     in->sin_family = AF_INET;
     in->sin_port = htons(port_num);
     memset(in->sin_zero,0,8 );

     while(bind(socket_handle,(struct sockaddr *)in, sizeof(struct sockaddr))) {
           if (port_num <= (u_short)(SUBAGENT_PORT + 7000) ) {
                  port_num++; 
           }else {
                sprintf(errmsg, "Unable to bind to port error = %d",errno);
                syslog(LOG_ERR, errmsg);
                return 0;
           }
       
           in->sin_port = htons(port_num);
     }

     sysinfo(SI_HOSTNAME, hostname, sizeof(hostname));
     hp = gethostbyname_r(hostname, &hpent, buff, sizeof(buff), &herr);

     if (!hp) {
              sprintf(errmsg,"Failed on gethostbyname_r : hostname = %s  : errno = %d",hostname,herr);
              syslog(LOG_ERR, errmsg);
              return 0;
     }
     else
     {
         memcpy ((caddr_t)&in->sin_addr.s_addr,
                                (caddr_t)hp->h_addr, hp->h_length);
         in->sin_port = htons(port_num); 
         in->sin_family = AF_INET;
     }
     return 1;
}

snmp_dpi_hdr * pdu_to_dpihdr(SNMP_pdu *pdu) {

OidPrefix_t  *tmp=XlateList;
char  *tmpptr;
char  *ptr;
SNMP_variable *var;
snmp_dpi_hdr *dpihdr;
snmp_dpi_get_packet *getp;
snmp_dpi_set_packet *setp;

         dpihdr = (snmp_dpi_hdr *)calloc(1, sizeof(snmp_dpi_hdr));
         if (!dpihdr) 
             return snmp_dpi_hdr_NULL_p;
        
         dpihdr->packet_id = pdu->request_id;
         switch(pdu->type) {
          case GET_REQ_MSG:
          case GETNEXT_REQ_MSG:
                if (pdu->type == GET_REQ_MSG)
                    dpihdr->packet_type = SNMP_DPI_GET;
                else 
                    dpihdr->packet_type = SNMP_DPI_GETNEXT;
                   
                var = pdu->first_variable;
                if (!var) {
                    fDPIparse(dpihdr);
                    snmp_pdu_free(pdu);
                    return snmp_dpi_hdr_NULL_p;
                }
                getp=dpihdr->data_u.get_p = (snmp_dpi_get_packet *)calloc(1,
                                          sizeof(snmp_dpi_get_packet));
                if (!dpihdr->data_u.get_p) {
                    fDPIparse(dpihdr);
                    snmp_pdu_free(pdu);
                    return snmp_dpi_hdr_NULL_p;
                } 
                do {
                   tmp = XlateList;
                   getp->object_p = strdup(SSAOidString(&var->name));
                   while((tmp) &&    
                         (strncmp(getp->object_p , tmp->pOidPrefix,
                                  strlen(tmp->pOidPrefix)))) {
                      tmp = tmp->pNextOidPre;
                   }
                   if (!tmp) {
                      fDPIparse(dpihdr); snmp_pdu_free(pdu);
                      return snmp_dpi_hdr_NULL_p;
                   }
                   if (!strncmp(getp->object_p+strlen(tmp->pOidPrefix),
                                "1.1", 3) ) {
                   tmpptr = strdup(tmp->pOidPrefix);
                   tmpptr = strcat(tmpptr, "1.");
                   getp->group_p = strdup(tmpptr);
                   ptr = getp->object_p + strlen(tmpptr);
                   getp->instance_p = strdup(ptr);
                   free(tmpptr); 
                   }
                   else {
                   getp->group_p = strdup(tmp->pOidPrefix);
                   ptr = getp->object_p + strlen(tmp->pOidPrefix);
                   getp->instance_p = strdup(ptr);
                   }
#if 0
                   if (!strncmp(getp->object_p,SUN_ARCH_DMI_COMPMIB,    
                                            strlen(SUN_ARCH_DMI_COMPMIB)) )
                   {
                   getp->group_p = strdup(SUN_ARCH_DMI_1);
                   ptr = getp->object_p + strlen(SUN_ARCH_DMI_1);
                   getp->instance_p = strdup(ptr);
                   } else {
                   getp->group_p = strdup(SUN_ARCH_DMI);
                   ptr = getp->object_p + strlen(SUN_ARCH_DMI);
                   getp->instance_p = strdup(ptr);
                   }
#endif
                   getp->next_p = NULL;
                   var = var->next_variable;
                   if (var) {
                      getp->next_p = (snmp_dpi_get_packet *)calloc(1,
                                             sizeof(snmp_dpi_get_packet));
                      getp = getp->next_p; 
                   }
                } while(var); 
                snmp_pdu_free(pdu); 
                         break;

          case SET_REQ_MSG:
                dpihdr->packet_type = SNMP_DPI_SET;
                   
                var = pdu->first_variable;
                if (!var) {
                    fDPIparse(dpihdr);
                    snmp_pdu_free(pdu);
                    return snmp_dpi_hdr_NULL_p;
                }
                setp=dpihdr->data_u.set_p = (snmp_dpi_set_packet *)calloc(1,
                                          sizeof(snmp_dpi_set_packet));
                if (!dpihdr->data_u.set_p) {
                    fDPIparse(dpihdr);
                    snmp_pdu_free(pdu);
                    return snmp_dpi_hdr_NULL_p;
                } 
                do {
                   tmp = XlateList;
                   setp->object_p = strdup(SSAOidString(&var->name));
                   while((tmp) &&    
                         (strncmp(setp->object_p , tmp->pOidPrefix,
                                  strlen(tmp->pOidPrefix)))) {
                      tmp = tmp->pNextOidPre;
                   }
                   if (!tmp) {
                      fDPIparse(dpihdr); snmp_pdu_free(pdu);
                      return snmp_dpi_hdr_NULL_p;
                   }

                   if (!strncmp(setp->object_p+strlen(tmp->pOidPrefix),
                                "1.1", 3) ) {
                   tmpptr = strdup(tmp->pOidPrefix);
                   tmpptr = strcat(tmpptr, "1.");
                   setp->group_p = strdup(tmpptr);
                   ptr = setp->object_p + strlen(tmpptr);
                   setp->instance_p = strdup(ptr);
                   free(tmpptr); 
                   }
                   else {
                   setp->group_p = strdup(tmp->pOidPrefix);
                   ptr = setp->object_p + strlen(tmp->pOidPrefix);
                   setp->instance_p = strdup(ptr);
                   }

#if 0
                   if (!strncmp(setp->object_p,SUN_ARCH_DMI_COMPMIB,    
                                            strlen(SUN_ARCH_DMI_COMPMIB)) )
                   {
                   setp->group_p = strdup(SUN_ARCH_DMI_1);
                   ptr = setp->object_p + strlen(SUN_ARCH_DMI_1);
                   setp->instance_p = strdup(ptr);
                   } else {
                   setp->group_p = strdup(SUN_ARCH_DMI);
                   ptr = setp->object_p + strlen(SUN_ARCH_DMI);
                   setp->instance_p = strdup(ptr);
                   }
#endif
                   setp->value_len = var->val_len;
                   setp->value_p = (char *) calloc(1, var->val_len+1);
                   memcpy(setp->value_p, (char *)var->val.string, var->val_len);
                   switch(var->type) { 
                     case INTEGER:
                         setp->value_type = SNMP_TYPE_Integer32;
                         break;
                     case ASN_BOOLEAN:
                         setp->value_type = SNMP_TYPE_Integer32;
                         break;
                     case ASN_BIT_STR:
                         setp->value_type = SNMP_TYPE_BIT_STRING;
                         break;
                     case STRING:
                         setp->value_type = SNMP_TYPE_OCTET_STRING;
                         break;
                     case OBJID:
                         setp->value_type = SNMP_TYPE_OBJECT_IDENTIFIER;
                         break;
                     case NULLOBJ:
                         setp->value_type = SNMP_TYPE_NULL;
                         break;
                     case IPADDRESS:
                         setp->value_type = SNMP_TYPE_IpAddress;
                         break;
                     case COUNTER:
                         setp->value_type = SNMP_TYPE_Counter32;
                         break;
                     case GAUGE:
                         setp->value_type = SNMP_TYPE_Gauge32;
                         break;
                     case TIMETICKS:
                         setp->value_type = SNMP_TYPE_TimeTicks;
                         break;
                     case OPAQUE: 
                         setp->value_type = SNMP_TYPE_Opaque;
                         break;
                   }
                   setp->next_p = NULL;
                   var = var->next_variable;
                   if (var) {
                      setp->next_p = (snmp_dpi_set_packet *)calloc(1,
                                             sizeof(snmp_dpi_set_packet));
                      setp = setp->next_p; 
                   }
                } while(var); 
                snmp_pdu_free(pdu); 
                         break;

          default:
                 fDPIparse(dpihdr);
                 snmp_pdu_free(pdu);
                 return snmp_dpi_hdr_NULL_p;
                 
         }

        return(dpihdr); 
}


SNMP_pdu * dpihdr_to_pdu(snmp_dpi_hdr *dpihdr, snmp_dpi_set_packet *setp,
                                  u_long error_status, u_long error_index) {

SNMP_variable *var=NULL;
SNMP_pdu      *pdu;
Oid           *oidp;
snmp_dpi_get_packet *getp;
snmp_dpi_set_packet *setpn=setp;

         pdu = snmp_pdu_new(error_label); 
         if (!pdu) 
             return NULL;
        
         pdu->request_id = dpihdr->packet_id ;
         pdu->type = GET_RSP_MSG;
         if (error_status > SNMP_ERROR_genErr) {
         switch(error_status) {
           case SNMP_ERROR_noAccess:
           case SNMP_ERROR_resourceUnavailable:
           case SNMP_ERROR_inconsistentName:
               error_status = SNMP_ERROR_noSuchName;
               break;
           case SNMP_ERROR_wrongLength:
               error_status = SNMP_ERROR_tooBig;
               break;

           case SNMP_ERROR_wrongType:
           case SNMP_ERROR_wrongEncoding:
           case SNMP_ERROR_wrongValue:
           case SNMP_ERROR_inconsistentValue:
               error_status = SNMP_ERROR_badValue;
               break;
           case SNMP_ERROR_noCreation:
           case SNMP_ERROR_commitFailed:
           case SNMP_ERROR_undoFailed:
           case SNMP_ERROR_authorizationError:
               error_status = SNMP_ERROR_genErr;
               break;
           case SNMP_ERROR_notWritable:
               error_status = SNMP_ERROR_readOnly;
               break;
           default:
              error_status = SNMP_ERROR_genErr;
              break;
         }
         }
         pdu->version      = SNMP_VERSION_1; 
         pdu->community    = strdup("public");
         pdu->error_status = error_status;
         pdu->error_index = error_index;
         switch(dpihdr->packet_type) {
          case SNMP_DPI_GET:
          case SNMP_DPI_GETNEXT:
          case SNMP_DPI_SET:
          case SNMP_DPI_COMMIT:
                   
                var = pdu->first_variable = (SNMP_variable *)
                                  calloc(1, sizeof(SNMP_variable));
                if (!var) {
                    snmp_pdu_free(pdu);
                    return NULL;
                }

                if (!setpn)
                      break;    
                do {
                   var->next_variable = NULL;
                   oidp = SSAOidStrToOid(setpn->object_p,error_label);
                   var->name = *oidp;
                   free(oidp); 
                   if (error_status == SNMP_ERR_NOERROR) {
                   var->val_len = setpn->value_len;
                   var->val.string = (unsigned char *)calloc(1,
                                                  var->val_len+1);
                   memcpy((char *)var->val.string,
                          (char *)setpn->value_p,
                          var->val_len);
                   switch(setpn->value_type) { 
                     case SNMP_TYPE_Integer32:
                         var->type = INTEGER;
                         break;
                     case SNMP_TYPE_BIT_STRING:
                         var->type = ASN_BIT_STR;
                         break;
                     case SNMP_TYPE_OCTET_STRING:
                     case SNMP_TYPE_DisplayString:
                     case SNMP_TYPE_NsapAddress:
                         var->type= STRING;
                         break;
                     case SNMP_TYPE_OBJECT_IDENTIFIER:
                         var->type = OBJID;
                         break;
                     case SNMP_TYPE_NULL:
                         var->type = NULLOBJ;
                         break;
                     case SNMP_TYPE_IpAddress:
                         var->type = IPADDRESS;
                         break;
                     case SNMP_TYPE_Counter32:
                         var->type = COUNTER;
                         break;
                     case SNMP_TYPE_Gauge32:
                         var->type= GAUGE;
                         break;
                     case SNMP_TYPE_TimeTicks:
                         var->type = TIMETICKS;
                         break;
                     case SNMP_TYPE_Opaque: 
                         var->type = OPAQUE;
                         break;
                     case SNMP_TYPE_UInteger32: /*Currently not supported*/
                     case SNMP_TYPE_Counter64:
                     case SNMP_TYPE_noSuchObject:
                     case SNMP_TYPE_noSuchInstance:
                     case SNMP_TYPE_endOfMibView:
                         var->type = NULLOBJ;
                   }
                   }else {
                        var->type = NULLOBJ;
                        var->val_len = 0;
                        var->val.string = NULL;
                   }
                   setpn = setpn->next_p;
                   if (setpn) {
                      var->next_variable = (SNMP_variable *)calloc(1,
                                             sizeof(SNMP_variable));
                      var = var->next_variable; 
                      if (!var) {
                           snmp_pdu_free(pdu);
                           return NULL;
                      }
  
                   }
                } while(setpn); 
                         break;

          default:
                 snmp_pdu_free(pdu);
                 return NULL;
                 
         }

        return(pdu); 
}

int create_and_dispatch_trap(int generic, int specific, snmp_dpi_set_packet
                              *setp, char *enterprisep, char *error_label) {

SNMP_variable  *var, *first_var;
snmp_dpi_set_packet *setpn = setp;
Oid *entp = SSAOidStrToOid(enterprisep,error_label);
Oid *tmpoidp;
IPAddress dest_ipaddress;
unsigned long time_stamp;
int  trap_port = SSAGetTrapPort(); 
 
           
            memset((char *)&dest_ipaddress, 0, sizeof(IPAddress));
            memcpy((char *)&dest_ipaddress,(char *)&my_address.sin_addr,
                                         sizeof(IPAddress));
            if (!entp)
                return(0);
            time((time_t *)&time_stamp);
            if (trap_port == 0) {
                SSAOidFree(entp);
                return(0);
            }
            first_var = var = (SNMP_variable *)calloc(1,sizeof(SNMP_variable));
            if (!var) {
                SSAOidFree(entp);
                return(0);
            }
            if (setpn) {
                do {
                   var->next_variable = NULL;
                   tmpoidp = SSAOidStrToOid(setpn->object_p,error_label);
                   var->name = *tmpoidp;
                   free(tmpoidp);
                   var->val_len = setpn->value_len;
                   var->val.string = (unsigned char *)calloc(1, var->val_len+1);
                   memcpy((char *)var->val.string,
                     (char *)setpn->value_p,
                             var->val_len);
                   switch(setpn->value_type) { 
                     case SNMP_TYPE_Integer32:
                         var->type = INTEGER;
                         break;
                     case SNMP_TYPE_BIT_STRING:
                         var->type = ASN_BIT_STR;
                         break;
                     case SNMP_TYPE_DisplayString:
                     case SNMP_TYPE_OCTET_STRING:
                         var->type= STRING;
                         break;
                     case SNMP_TYPE_OBJECT_IDENTIFIER:
                         var->type = OBJID;
                         break;
                     case SNMP_TYPE_NULL:
                         var->type = NULLOBJ;
                         break;
                     case SNMP_TYPE_IpAddress:
                         var->type = IPADDRESS;
                         break;
                     case SNMP_TYPE_Counter32:
                         var->type = COUNTER;
                         break;
                     case SNMP_TYPE_Gauge32:
                         var->type= GAUGE;
                         break;
                     case SNMP_TYPE_TimeTicks:
                         var->type = TIMETICKS;
                         break;
                     case SNMP_TYPE_Opaque: 
                     default:
                         var->type = OPAQUE;
                         break;
                   }
                   setpn = setpn->next_p;
                   if (setpn) {
                      var->next_variable = (SNMP_variable *)calloc(1,
                                             sizeof(SNMP_variable));
                      var = var->next_variable; 
                      if (!var) {
                           snmp_variable_list_free(first_var);
                           return 0;
                      }
  
                   }
                } while(setpn); }
if (trap_send_with_more_para(&dest_ipaddress, my_address.sin_addr,
                             1, entp, generic, specific,
                             trap_port, time_stamp, first_var,
                             error_label) == -1) {
       snmp_variable_list_free(first_var); 
       SSAOidFree(entp);
       return 0; 
}
SSAOidFree(entp);
snmp_variable_list_free(first_var); 
return 1;

}
/*********************************************************************/
/* Function to free complete DPI parse tree                          */
/*********************************************************************/
void LINKAGE fDPIparse(snmp_dpi_hdr *hdr_p) /* free a DPI parse tree */
{
   if (hdr_p) {
      switch (hdr_p->packet_type) {
      case SNMP_DPI_GET:
           fDPIget_packet(hdr_p->data_u.get_p);
           break;
      case SNMP_DPI_GETNEXT:
           fDPInext_packet(hdr_p->data_u.next_p);
           break;
      case SNMP_DPI_RESPONSE:
           fDPIresp_packet(hdr_p->data_u.resp_p);
           break;
      case SNMP_DPI_SET:
      case SNMP_DPI_COMMIT:
      case SNMP_DPI_UNDO:
           fDPIset_packet(hdr_p->data_u.set_p);
           break;
      case SNMP_DPI_TRAP:
           fDPItrap_packet(hdr_p->data_u.trap_p);
           break;
      case SNMP_DPI_REGISTER:
           fDPIreg_packet(hdr_p->data_u.reg_p);
           break;
      case SNMP_DPI_UNREGISTER:
           fDPIureg_packet(hdr_p->data_u.ureg_p);
           break;
      case SNMP_DPI_OPEN:
           fDPIopen_packet(hdr_p->data_u.open_p);
           break;
      case SNMP_DPI_CLOSE:
           fDPIclose_packet(hdr_p->data_u.close_p);
           break;
      case SNMP_DPI_ARE_YOU_THERE:
           break;                         /* no packet data          */
      case SNMP_DPI_GETBULK:              /* TODO: implement         */
      case SNMP_DPI_TRAPV2:
      case SNMP_DPI_INFORM:
      default:                            /* should not occur        */
           break;
      } /* endswitch */
#ifdef SNMP_DPI_VIEW_SELECTION
      if (hdr_p->community_p) free(hdr_p->community_p);
#endif /* def SNMP_DPI_VIEW_SELECTION */
      free(hdr_p);
   }
}

/*********************************************************************/
/* Function to free DPI GET parse tree                               */
/*********************************************************************/
void fDPIget_packet(               /* free a GET structure    */
             snmp_dpi_get_packet *pack_p) /* ptr to structure        */
{
   snmp_dpi_get_packet *ret_p, *next_p;   /* ptr to GET structure    */

   next_p = pack_p;                       /* point to structure      */

   while (next_p) {                       /* do all GET structures   */
      ret_p = next_p;                     /* point to the structure  */
      if (ret_p->object_p)                /* free OIDstring if any   */
         free(ret_p->object_p);
      if (ret_p->group_p)                 /* free OIDstring if any   */
         free(ret_p->group_p);
      if (ret_p->instance_p)              /* free INSTANCE  if any   */
         free(ret_p->instance_p);
      next_p = ret_p->next_p;             /* pick up ptr to next one */
      free(ret_p);                        /* free GET structure      */
   } /* endwhile */
}

/*********************************************************************/
/* Function to free DPI GETNEXT parse tree                           */
/*********************************************************************/
void fDPInext_packet(              /* free GETNEXT structure  */
             snmp_dpi_next_packet *pack_p)/* ptr to structure        */
{
   snmp_dpi_next_packet *ret_p, *next_p;  /* ptr to GETNEXT structure*/

   next_p = pack_p;                       /* point to structure      */

   while (next_p) {                       /* do all GETNEXT structs  */
      ret_p = next_p;                     /* point to the structure  */
      if (ret_p->object_p)                /* free OIDstring if any   */
         free(ret_p->object_p);
      if (ret_p->group_p)                 /* free groupOID if any    */
         free(ret_p->group_p);
      if (ret_p->instance_p)              /* free INSTANCE  if any   */
         free(ret_p->instance_p);
      next_p = ret_p->next_p;             /* pick up ptr to next one */
      free(ret_p);                        /* free GETNEXT structure  */
   } /* endwhile */
}

/*********************************************************************/
/* Function to free DPI SET parse tree                               */
/*********************************************************************/
void fDPIset_packet(       /* free a SET structure    */
             snmp_dpi_set_packet *pack_p) /* ptr to SET structure    */
{
   snmp_dpi_set_packet *ret_p, *next_p;   /* ptr to SET structure    */

   next_p = pack_p;                       /* pick up ptr to SET      */

   while (next_p) {                       /* do all SET structures   */
      ret_p = next_p;                     /* point to the structure  */
      if (ret_p->object_p)                /* free OIDstring if any   */
         free(ret_p->object_p);
      if (ret_p->group_p)                 /* free OIDstring if any   */
         free(ret_p->group_p);
      if (ret_p->instance_p)              /* free INSTANCE  if any   */
         free(ret_p->instance_p);
      if (ret_p->value_p)                 /* free value if any       */
         free(ret_p->value_p);
      next_p = ret_p->next_p;             /* pick up ptr to next one */
      free(ret_p);                        /* free SET structure      */
   } /* endwhile */
}

/*********************************************************************/
/* Function to free DPI TRAP parse tree                              */
/*********************************************************************/
void fDPItrap_packet(              /* free DPI trap structure */
             snmp_dpi_trap_packet *pack_p)/* ptr to trap structure   */
{
   if (pack_p) {                          /* if valid ptr            */
      if (pack_p->enterprise_p)           /* free enterprise if any  */
          free(pack_p->enterprise_p);
      if (pack_p->varBind_p)              /* free varBind if any     */
         fDPIset_packet(pack_p->varBind_p);
      free(pack_p);                       /* free trap structure     */
   } /* endif */
}

/*********************************************************************/
/* Function to free DPI RESPONSE parse tree                          */
/*********************************************************************/
void fDPIresp_packet(snmp_dpi_resp_packet *pack_p)
{
   if (pack_p) {
      if (pack_p->varBind_p)              /* free varBind if any     */
         fDPIset_packet(pack_p->varBind_p);
      free(pack_p);                       /* free response structure */
   } /* endif */
}

/*********************************************************************/
/* Function to free DPI OPEN parse tree                              */
/*********************************************************************/
void fDPIopen_packet(snmp_dpi_open_packet *pack_p)
{
   if (pack_p) {
      if (pack_p->oid_p)                  /* free oid if any         */
          free(pack_p->oid_p);
      if (pack_p->description_p)          /* free description if any */
          free(pack_p->description_p);
      if (pack_p->password_p)             /* free password if any    */
          free(pack_p->password_p);
      free(pack_p);                       /* free open structure     */
   } /* endif */
}

/*********************************************************************/
/* Function to free DPI CLOSE parse tree                             */
/*********************************************************************/
void fDPIclose_packet(snmp_dpi_close_packet *pack_p)
{
   if (pack_p) {
      free(pack_p);                       /* free close structure    */
   } /* endif */
}

/*********************************************************************/
/* Function to free DPI REGISTER parse tree                          */
/*********************************************************************/
void fDPIreg_packet(snmp_dpi_reg_packet *pack_p)
{
   snmp_dpi_reg_packet *ret_p, *next_p;   /* ptr to REG structure    */

   next_p = pack_p;                       /* pick up ptr to REG      */

   while (next_p) {                       /* do all REG structures   */
      ret_p = next_p;                     /* point to the structure  */
      if (ret_p->group_p)                 /* free OIDstring if any   */
         free(ret_p->group_p);
      next_p = ret_p->next_p;             /* pick up ptr to next one */
      free(ret_p);                        /* free SET structure      */
   } /* endwhile */
}

/*********************************************************************/
/* Function to free DPI UNREGISTER parse tree                        */
/*********************************************************************/
void fDPIureg_packet(snmp_dpi_ureg_packet *pack_p)
{
   snmp_dpi_ureg_packet *ret_p, *next_p;  /* ptr to UREG structure   */

   next_p = pack_p;                       /* pick up ptr to REG      */

   while (next_p) {                       /* do all REG structures   */
      ret_p = next_p;                     /* point to the structure  */
      if (ret_p->group_p)                 /* free OIDstring if any   */
         free(ret_p->group_p);
      next_p = ret_p->next_p;             /* pick up ptr to next one */
      free(ret_p);                        /* free SET structure      */
   } /* endwhile */
}

/*********************************************************************/
/* function to Create or Extend a snmp_dpi_set_packet structure      */
/*********************************************************************/
snmp_dpi_set_packet  * LINKAGE mkDPIset_packet( /* Make DPIset packet*/
  snmp_dpi_set_packet  *packet_p,      /* ptr to SET structure       */
  char                 *group_p,       /* ptr to group ID (subtree)  */
  char                 *instance_p,    /* ptr to instance OID string */
  int                   value_type,    /* value type (SNMP_TYPE_xxx) */
  int                   value_len,     /* length of value            */
  void                 *value_p)       /* ptr to value               */
{
   snmp_dpi_set_packet  *new_p, *next_p;
   int                   l;

   if(group_or_instance_invalid(group_p,instance_p,"mkDPIset_packet"))
   {
      return(snmp_dpi_set_packet_NULL_p);   /* return NULL ptr if so */
   } /* endif */

   switch (value_type) {               /* validate the value type    */
   case SNMP_TYPE_Integer32:           /* 32-bit INTEGER             */
   case SNMP_TYPE_OCTET_STRING:        /* OCTET STRING               */
   case SNMP_TYPE_OBJECT_IDENTIFIER:   /* OBJECT IDENTIFIER          */
   case SNMP_TYPE_NULL:                /* NULL value                 */
   /*case SNMP_TYPE_DPI_1_INTERNET:       Old DPI 1.x type           */
   case SNMP_TYPE_Counter32:           /* 32-bit Counter (unsigned)  */
   case SNMP_TYPE_Gauge32:             /* 32-bit Gauge   (unsigned)  */
   case SNMP_TYPE_TimeTicks:           /* 32-bit TimeTicks (unsigned)*/
   case SNMP_TYPE_DisplayString:       /* Textual Convention         */
   case SNMP_TYPE_IpAddress:           /* IMPLICIT OCTET STRING (4)  */
   case SNMP_TYPE_BIT_STRING:          /* BIT STRING                 */
   case SNMP_TYPE_NsapAddress:         /* IMPLICIT OCTET STRING      */
   case SNMP_TYPE_UInteger32:          /* 32-bit INTEGER (unsigned)  */
   case SNMP_TYPE_Counter64:           /* 64-bit Counter (unsigned)  */
   case SNMP_TYPE_Opaque:              /* IMPLICIT OCTET STRING      */
   case SNMP_TYPE_noSuchObject:        /* IMPLICIT NULL              */
   case SNMP_TYPE_noSuchInstance:      /* IMPLICIT NULL              */
   case SNMP_TYPE_endOfMibView:        /* IMPLICIT NULL              */
     break;
   default:
     return(snmp_dpi_set_packet_NULL_p);  /* return a NULLptr        */
     /* break; */                         /* NEVER REACHED           */
   } /* endswitch */

   if ((value_len) && (value_p == ((char *)0)))/* Ensure we have a   */
   {                                           /* ptr if len != 0    */
      return(snmp_dpi_set_packet_NULL_p);      /* return a NULLptr   */
   } /* endif */

   if (value_len < 0) {
      return(snmp_dpi_set_packet_NULL_p);      /* return a NULLptr   */

   } /* endif */

   new_p = (snmp_dpi_set_packet *)calloc( /* allocate a new SET      */
           1,sizeof(snmp_dpi_set_packet));/* structure for new packet*/

   if (new_p) {                           /* if success              */
      new_p->next_p  = NULL;
      new_p->group_p = copy(group_p,&l);  /* callers groupID/subtree */
      if (instance_p && *instance_p) {    /* if more than a NULL,    */
         new_p->instance_p =              /* then also copy callers  */
                     copy(instance_p,&l); /* instance OID            */
      } /* endif */
      if (new_p->group_p &&               /* if both copies were     */
          new_p->instance_p)              /* successful then we      */
      {                                   /* can concatenate the     */
         new_p->object_p = concat(        /* group ID and instance ID*/
                    new_p->group_p,       /* into an object ID as    */
                    new_p->instance_p);   /* expected by DPI 1.x     */
      } else if (new_p->group_p) {        /* if no instance present  */
         new_p->object_p = copy(          /* then use the group ID   */
                           group_p,&l);   /* as the object ID also   */
      } /* endif */

      if (new_p->object_p) {              /* if still successful     */
         l = strlen(new_p->object_p)-1;   /* find possible trailing  */
         if (new_p->object_p[l] =='.') {  /* dot. If object ID has   */
            new_p->object_p[l] = '\0';    /* trailing dot, then      */
         } /* endif */                    /* remove it               */
         new_p->value_type = value_type;  /* set type in packet      */
         new_p->value_len  = value_len;   /* set length also         */
         if (value_len) {                 /* if value has a length   */
            new_p->value_p = (char *)     /* allocate memory for     */
                       malloc(value_len); /* the value itself        */
            if (new_p->value_p) {         /* if success,             */
                memcpy(new_p->value_p,    /* then copy the           */
                       value_p,value_len);/* value from caller       */
            } else {
               fDPIset_packet(new_p);     /* free allocated memory   */
               return(snmp_dpi_set_packet_NULL_p);/* return NULL ptr */
            } /* endif */
         } /* endif */
         if (packet_p) {                  /* caller passed existing  */
            next_p = packet_p;            /* chain, then find last   */
            while (next_p->next_p) {      /* entry in the chain      */
                 next_p = next_p->next_p; /* so we can add the new   */
            } /* endwhile */              /* structure at the end    */
            next_p->next_p = new_p;       /* of the chain.           */
            return(packet_p);             /* return ptr to first one */
         } /* endif */                    /* in the chain            */
         return(new_p);                   /* else new one IS the 1st */
      } /* endif */                       /* we failed on concat or  */
   } /* endif */                          /* allocating memory       */

   if (new_p) fDPIset_packet(new_p);      /* free memory (if any)    */
   return(snmp_dpi_set_packet_NULL_p);    /* and return NULL ptr     */
}

/*********************************************************************/
/*  Check groupID and instanceID to be correct.                      */
/*********************************************************************/
int group_or_instance_invalid(     /* validate arguments for  */
                        char *group_p,    /* this GROUP ID           */
                        char *instance_p, /* this INSTANCE           */
                        char *function_p) /* for thi function        */
{
#define DPI_TRUE   1
#define DPI_FALSE  0
   if (group_p == ((char *)0)) {                 /* must have one    */
      return(DPI_TRUE);                          /* invalid is TRUE  */
   } /* endif */

   if (oid_is_invalid(group_p,DPI_TRUE)) {       /* ensure valid one */
      return(DPI_TRUE);                          /* invalid is TRUE  */
   } /* endif */

   if (instance_p) {                             /* if instance is   */
      if (oid_is_invalid(instance_p,DPI_FALSE))  /* passed, do weak  */
      {                                          /* check on INSTANCE*/
       return(DPI_TRUE);                         /* invalid is TRUE  */
      } /* endif */

      if (*(instance_p+strlen(instance_p)) == '.')  /* a trailing    */
      {                                          /* dot is invalid   */
       return(DPI_TRUE);                         /* invalid is TRUE  */
      } /* endif */
   } /* endif */

   return(DPI_FALSE);                            /* all is OK        */
}

/*********************************************************************/
/*  The following code allocates memory for a null terminated string */
/*  copies the string and returns a ptr to the newly allocated area. */
/*  The callers length argument is set to the length of the string,  */
/*  including the terminating null character.                        */
/*  If a failure occurs, then a NULL ptr is returned, the callers    */
/*  length argument will still be set to the length of the string.   */
/*********************************************************************/
char *copy(char *cp, int *len)
{
   char *new_p;

   if (cp) {
      *len   = strlen(cp) + 1;               /* get length of string */
      new_p  = (char *) malloc(*len);        /* get memory for it    */
      if (new_p) {                           /* success, so copy it  */
         strcpy(new_p, cp);                  /* including '\0' char  */
      } else {                               /* else error           */
            return NULL;
      } /* endif */
   } else {
      *len  = 0;
      new_p = (char *)0;
   } /* endif */
   return(new_p);                            /* return ptr to caller */
}

/*********************************************************************/
/*  The following code allocates memory for a null terminated string */
/*  that will be built by concatenating the 2 null terminated strings*/
/*  beings passed (p1 and p2). Returns a ptr to newly allocated area.*/
/*  If a failure occurs, then a NULL ptr is returned.                */
/*********************************************************************/
char *concat(char *p1, char *p2)
{
   char *new_p;                              /* ptr to new memory    */
   int   len;

   if (p1) {                                 /* if ptr1 is valid     */
      len   = strlen(p1) + 1;                /* get length of string */
   } else {
      return((char *)0);                     /* return NULL ptr      */
   } /* endif */

   if (p2) len += strlen(p2) + 1;            /* add length of string */

   new_p  = (char *) malloc(len);            /* get memory for it    */

   if (new_p) {                              /* success, so now      */
      strcpy(new_p, p1);                     /* copy first string    */
      if (p2) strcat(new_p, p2);             /* append second string */
   } else {                                  /* else error           */
         return NULL;
   } /* endif */
   return(new_p);                            /* return ptr to caller */
}


/*********************************************************************/
/*  An OID or INSTANCE or GROUPID must be dot separated numbers      */
/*  with a value that is containable in an unsigned 32 bit integer.  */
/*  We're not checking that it fits in 32 bit integer yet            */
/*  If strong checking is wanted, we also check valid start of OID   */
/*********************************************************************/
int oid_is_invalid(char *oid_p,    /* validate OID string     */
                          int   strong)   /* DPI_TRUE or DPI_FALSE   */
                                          /* for string checking     */
{
  unsigned long  ulong;
  char          *cp;
  int            rc = DPI_RC_OK;

  if (oid_p == ((char *)0)) {             /* if no ptr, then return  */
     return(DPI_TRUE);                    /* invalid is TRUE         */
  } /* endif */

  if (strong == DPI_TRUE) {               /* this code ensures that  */
     ulong = strtoul(oid_p, &cp, 10);     /* an OID starts with      */
     switch (ulong) {                     /* valid first 2 subIDs    */
     case 0:                              /* 0.xx.yy    (xx<40)      */
     case 1:                              /* 1.xx.yy    (xx<40)      */
        if (*cp != '.') {
           rc = DPI_RC_NOK;
           break;
        } /* endif */
        cp++;
        ulong = strtoul(cp, &cp, 10);
        if (ulong>39) {
           rc = DPI_RC_NOK;
        } /* endif */
        break;
     case 2:                              /* 2.yy    (any yy)        */
        break;
     default:                             /* x.      (x>2 = invalid  */
        rc = DPI_RC_NOK;
        break;
     } /* endswitch */
  } /* endif */

  if (rc != DPI_RC_OK) {
     return(DPI_TRUE);                    /* then invalid is TRUE    */
  } /* endif */

  while (*oid_p) {                        /* do all bytes till \0    */
      if (isdigit(*oid_p)) oid_p++;       /* bump ptr if digit       */
      else if (*oid_p == '.') {           /* if it is a dot instead  */
         oid_p++;                         /* bump the ptr also, then */
         if (*oid_p == '.') {             /* if next one is also dot */
            return(DPI_TRUE);             /* then invalid is TRUE    */
         } /* endif */
      } else {                            /* not a digit nor a dot.  */
            return(DPI_TRUE);             /* so.. invalid is TRUE    */
      } /* endif */
  } /* endwhile */
                                          /* all OK, digits and dots */
  return(DPI_FALSE);                      /* invalid is FALSE        */
}



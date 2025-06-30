#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <signal.h>

#define REBOOT_IMMEDIATE    1
#define REBOOT_GRACEFUL     2
#define REBOOT_CANCEL       3

// Global variable to track reboot state
static int reboot_pending = 0;
static int reboot_type = 0;

int get_reboot_status(void) {
    return reboot_pending;
}

int set_reboot_action(int action) {
    int result;
    const char *command;
    
    switch (action) {
        case REBOOT_IMMEDIATE:
            // Immediate reboot - no delay
            command = "shutdown -r now 'System reboot requested via SNMP' &";
            result = system(command);
            if (result == 0) {
                reboot_pending = 1;
                reboot_type = REBOOT_IMMEDIATE;
                printf("Immediate reboot initiated via SNMP\n");
                return 0;
            }
            return -1; // Failed to execute reboot command
            
        default:
            return -3; // Invalid action
    }
}

int reboot_handler(netsnmp_mib_handler *handler,
                   netsnmp_handler_registration *reginfo,
                   netsnmp_agent_request_info *reqinfo,
                   netsnmp_request_info *requests) {
    int reboot_action;
    int result;
    long *long_val;
    
    switch (reqinfo->mode) {
        case MODE_GET:
            // Return current reboot status
            snmp_set_var_typed_integer(requests->requestvb, ASN_INTEGER, 
                                       get_reboot_status());
            break;
            
        case MODE_SET_RESERVE1:
            // Check if the value is of correct type
            if (requests->requestvb->type != ASN_INTEGER) {
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_WRONGTYPE);
                return SNMP_ERR_WRONGTYPE;
            }
            
            // Check if the value is in valid range (only 1 for immediate reboot)
            long_val = (long*)requests->requestvb->val.integer;
            if (*long_val != 1) {
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_BADVALUE);
                return SNMP_ERR_BADVALUE;
            }
            break;
        
        case MODE_SET_RESERVE2:
            // Additional validation can be done here
            break;
        
        case MODE_SET_ACTION:
            // Get the reboot action value
            reboot_action = *(requests->requestvb->val.integer);
            
            // Attempt to perform the reboot action
            result = set_reboot_action(reboot_action);
            
            switch (result) {
                case 0:
                    // Success
                    break;
                case -1:
                    netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_RESOURCEUNAVAILABLE);
                    return SNMP_ERR_RESOURCEUNAVAILABLE;
                case -3:
                    netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_BADVALUE);
                    return SNMP_ERR_BADVALUE;
                default:
                    netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_GENERR);
                    return SNMP_ERR_GENERR;
            }
            break;
        
        case MODE_SET_COMMIT:
        case MODE_SET_FREE:
        case MODE_SET_UNDO:
            // Nothing special needed for these phases
            break;
            
        default:
            return SNMP_ERR_GENERR;
    }
    return SNMP_ERR_NOERROR;
}

void init_reboot(void) {
    const oid reboot_oid[] = { 1, 3, 6, 1, 4, 1, 112233, 20 };
    
    printf("Init REBOOT\n");

    netsnmp_register_scalar(
        netsnmp_create_handler_registration("rebootOID",
                                            reboot_handler,
                                            reboot_oid,
                                            OID_LENGTH(reboot_oid),
                                            HANDLER_CAN_RWRITE));
}
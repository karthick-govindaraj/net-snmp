#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>

#define MAX_HOSTNAME_LEN 255

char* get_system_hostname(void) {
    static char hostname[HOST_NAME_MAX + 1];
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        perror("gethostname");
        return NULL;
    }
    return hostname;
}

int set_system_hostname(const char* new_hostname) {
    FILE *fp;
    char command[512];
    int sys_result;
    const char *p;  /* Moved declaration to top */
    
    // Validate hostname (basic checks)
    if (!new_hostname || strlen(new_hostname) == 0 || strlen(new_hostname) > MAX_HOSTNAME_LEN) {
        return -1; // Invalid hostname
    }
    
    // Check for invalid characters (basic validation)
    // Hostname should only contain alphanumeric characters, hyphens, and dots
    for (p = new_hostname; *p; p++) {
        if (!(*p >= 'a' && *p <= 'z') && 
            !(*p >= 'A' && *p <= 'Z') && 
            !(*p >= '0' && *p <= '9') && 
            *p != '-' && *p != '.') {
            return -2; // Invalid characters
        }
    }
    
    // Set the hostname using sethostname system call
    if (sethostname(new_hostname, strlen(new_hostname)) != 0) {
        return -3; // Failed to set hostname
    }
    
    // Update /etc/hostname file
    fp = fopen("/etc/hostname", "w");
    if (fp != NULL) {
        fprintf(fp, "%s\n", new_hostname);
        fclose(fp);
    }
    
    // Update /etc/hosts file (optional, for localhost mapping)
    fp = fopen("/etc/hosts", "r+");
    if (fp != NULL) {
        fclose(fp); // For now, just ensure we can access it
    }
    
    // Use hostnamectl if available (systemd systems)
    snprintf(command, sizeof(command), "hostnamectl set-hostname '%s' 2>/dev/null", new_hostname);
    sys_result = system(command);
    (void)sys_result; /* Suppress unused result warning */
    
    return 0; // Success
}

int custom_handler(netsnmp_mib_handler *handler,
                   netsnmp_handler_registration *reginfo,
                   netsnmp_agent_request_info *reqinfo,
                   netsnmp_request_info *requests) {
    char hostname[255];
    char new_hostname[MAX_HOSTNAME_LEN + 1];
    int result;
    
    switch (reqinfo->mode) {
        case MODE_GET:
            snprintf(hostname, sizeof(hostname), "%s", get_system_hostname());
            snmp_set_var_typed_value(requests->requestvb, ASN_OCTET_STR,
                                     hostname, strlen(hostname));
            break;
            
        case MODE_SET_RESERVE1:
            // Check if the value is of correct type and size
            if (requests->requestvb->type != ASN_OCTET_STR) {
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_WRONGTYPE);
                return SNMP_ERR_WRONGTYPE;
            }
            
            if (requests->requestvb->val_len == 0 || 
                requests->requestvb->val_len > MAX_HOSTNAME_LEN) {
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_WRONGLENGTH);
                return SNMP_ERR_WRONGLENGTH;
            }
            break;
        
        case MODE_SET_RESERVE2:
            // Additional validation can be done here
            break;
        
        case MODE_SET_ACTION:
            // Copy the new hostname value
            memcpy(new_hostname, requests->requestvb->val.string, 
                   requests->requestvb->val_len);
            new_hostname[requests->requestvb->val_len] = '\0';
            
            // Attempt to set the hostname
            result = set_system_hostname(new_hostname);
            
            switch (result) {
                case 0:
                    // Success
                    break;
                case -1:
                    netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_WRONGLENGTH);
                    return SNMP_ERR_WRONGLENGTH;
                case -2:
                    netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_BADVALUE);
                    return SNMP_ERR_BADVALUE;
                case -3:
                    netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_RESOURCEUNAVAILABLE);
                    return SNMP_ERR_RESOURCEUNAVAILABLE;
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

void init_hostname(void) {
    const oid custom_oid[] = { 1, 3, 6, 1, 4, 1, 112233, 1 };
    
    printf("Init HOSTNAME\n");

    netsnmp_register_scalar(
        netsnmp_create_handler_registration("customOID",
                                            custom_handler,
                                            custom_oid,
                                            OID_LENGTH(custom_oid),
                                            HANDLER_CAN_RWRITE));
}
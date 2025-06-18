#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#include <stdio.h>
#include <string.h>

char* get_system_hostname(void) {
    static char hostname[HOST_NAME_MAX + 1];
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        perror("gethostname");
        return NULL;
    }
    return hostname;
}

int custom_handler(netsnmp_mib_handler *handler,
                   netsnmp_handler_registration *reginfo,
                   netsnmp_agent_request_info *reqinfo,
                   netsnmp_request_info *requests) {
    switch (reqinfo->mode) {
        case MODE_GET: {
            char hostname[255];
            snprintf(hostname, sizeof(hostname), "%s", get_system_hostname());
            snmp_set_var_typed_value(requests->requestvb, ASN_OCTET_STR,
                                     hostname, strlen(hostname));
            break;
        }
        default:
            return SNMP_ERR_GENERR;
    }
    return SNMP_ERR_NOERROR;
}

void init_hostname(void) {
    printf("Init HOSTNAME\n");
    const oid custom_oid[] = { 1, 3, 6, 1, 4, 1, 112233, 1 };

    netsnmp_register_scalar(
        netsnmp_create_handler_registration("customOID",
                                            custom_handler,
                                            custom_oid,
                                            OID_LENGTH(custom_oid),
                                            HANDLER_CAN_RONLY));
}


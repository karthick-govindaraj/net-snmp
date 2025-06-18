#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <stdio.h>
#include <string.h>
#include <sys/utsname.h>

char* get_system_kernel_name(void) {
    static struct utsname uts;
    if (uname(&uts) != 0) {
        perror("uname");
        return NULL;
    }
    return uts.sysname;
}

int kernel_name_handler(netsnmp_mib_handler *handler,
                   netsnmp_handler_registration *reginfo,
                   netsnmp_agent_request_info *reqinfo,
                   netsnmp_request_info *requests) {
    switch (reqinfo->mode) {
        case MODE_GET: {
            char kernel_name[255];
            snprintf(kernel_name, sizeof(kernel_name), "%s", get_system_kernel_name());
            snmp_set_var_typed_value(requests->requestvb, ASN_OCTET_STR,
                                     kernel_name, strlen(kernel_name));
            break;
        }
        default:
            return SNMP_ERR_GENERR;
    }
    return SNMP_ERR_NOERROR;
}

void init_kernel_name(void) {
    printf("Init KERNEL_NAME\n");
    const oid kernel_name_oid[] = { 1, 3, 6, 1, 4, 1, 112233, 3 };

    netsnmp_register_scalar(
        netsnmp_create_handler_registration("kernelNameOID",
                                            kernel_name_handler,
                                            kernel_name_oid,
                                            OID_LENGTH(kernel_name_oid),
                                            HANDLER_CAN_RONLY));
} 
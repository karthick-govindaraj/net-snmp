#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <unistd.h>

#include "custom_cpu.h"

static oid totalCPUs_oid[] = { 1, 3, 6, 1, 4, 1, 112233, 2 };

int handle_totalCPUs(netsnmp_mib_handler *handler,
                     netsnmp_handler_registration *reginfo,
                     netsnmp_agent_request_info   *reqinfo,
                     netsnmp_request_info         *requests) {
    switch (reqinfo->mode) {
        case MODE_GET: {
            long cpu_count = sysconf(_SC_NPROCESSORS_ONLN);
            snmp_set_var_typed_value(requests->requestvb, ASN_INTEGER,
                                     (u_char *)&cpu_count,
                                     sizeof(cpu_count));
            break;
        }
        default:
            return SNMP_ERR_GENERR;
    }
    return SNMP_ERR_NOERROR;
}

void init_custom_cpu(void) {
    printf("Registering totalCPUs OID\n");
    netsnmp_register_scalar(
        netsnmp_create_handler_registration("totalCPUs",
                                            handle_totalCPUs,
                                            totalCPUs_oid,
                                            OID_LENGTH(totalCPUs_oid),
                                            HANDLER_CAN_RONLY));
}


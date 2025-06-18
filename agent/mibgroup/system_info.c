#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <sys/statvfs.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <time.h>
#include <dirent.h>
#include <limits.h>
#include <errno.h>
#include "system_info.h"
static oid system_info_oid[] = { 1, 3, 6, 1, 4, 1, 112233 };
static char custom_timezone[64] = "";
char* get_os_name(void) {
    static struct utsname uts;
    if (uname(&uts) != 0) {
        perror("uname");
        return strdup("Unknown OS");
    }
    return strdup(uts.sysname);
}
char* get_architecture(void) {
    static struct utsname uts;
    if (uname(&uts) != 0) {
        perror("uname");
        return strdup("Unknown Architecture");
    }
    return strdup(uts.machine);
}
long get_system_uptime(void) {
    struct sysinfo s_info;
    if (sysinfo(&s_info) != 0) {
        perror("sysinfo");
        return -1;
    }
    return s_info.uptime;
}
u_char* get_mac_address(size_t *mac_len) {
    struct ifaddrs *ifaddr, *ifa;
    u_char* mac = (u_char*) calloc(1, 6);
    *mac_len = 0;
    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        return mac;
    }
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) continue;
        if ((ifa->ifa_flags & IFF_UP) && !(ifa->ifa_flags & IFF_LOOPBACK)) {
             if (strcmp(ifa->ifa_name, "eth0") == 0 || strcmp(ifa->ifa_name, "enp0s3") == 0) {
                 mac[0] = 0x00; mac[1] = 0x1A; mac[2] = 0x2B;
                 mac[3] = 0x3C; mac[4] = 0x4D; mac[5] = 0x5E;
                 *mac_len = 6;
                 break;
             }
        }
    }
    freeifaddrs(ifaddr);
    return mac;
}
char* get_ip_addresses(void) {
    struct ifaddrs *ifaddr, *ifa;
    char addr_str[NI_MAXHOST];
    char* ip_list = strdup("");
    size_t current_len = 0;
    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        return strdup("Unknown IP Addresses");
    }
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) continue;
        int family = ifa->ifa_addr->sa_family;
        if ((family == AF_INET || family == AF_INET6) && !(ifa->ifa_flags & IFF_LOOPBACK)) {
            int s = getnameinfo(ifa->ifa_addr, (family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6),
                                addr_str, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
            if (s != 0) {
                fprintf(stderr, "getnameinfo() failed: %s\n", gai_strerror(s));
                continue;
            }
            size_t addr_len = strlen(addr_str);
            size_t needed_len = current_len + addr_len + (current_len > 0 ? 1 : 0) + 1;
            char* temp = realloc(ip_list, needed_len);
            if (temp == NULL) {
                perror("realloc");
                free(ip_list);
                freeifaddrs(ifaddr);
                return strdup("Error fetching IP Addresses");
            }
            ip_list = temp;
            if (current_len > 0) {
                strcat(ip_list, ",");
            }
            strcat(ip_list, addr_str);
            current_len = strlen(ip_list);
        }
    }
    freeifaddrs(ifaddr);
    if (current_len == 0) {
        return strdup("No active network interfaces");
    }
    return ip_list;
}
int get_mem_info(const char* key, long* value) {
    FILE* f = fopen("/proc/meminfo", "r");
    char line[256];
    if (!f) {
        perror("fopen /proc/meminfo");
        return -1;
    }
    while (fgets(line, sizeof(line), f)) {
        long val;
        if (sscanf(line, "%*s %ld", &val) == 1) {
            if (strstr(line, key) == line) {
                *value = val * 1024;
                fclose(f);
                return 0;
            }
        }
    }
    fclose(f);
    return -1;
}
int get_mem_used(void) {
    long total = 0, free = 0, buffers = 0, cached = 0;
    if (get_mem_info("MemTotal:", &total) == 0 &&
        get_mem_info("MemFree:", &free) == 0 &&
        get_mem_info("Buffers:", &buffers) == 0 &&
        get_mem_info("Cached:", &cached) == 0) {
        return (int)(total - free - buffers - cached);
    }
    return -1;
}
int get_mem_available(void) {
    long available = 0;
    if (get_mem_info("MemAvailable:", &available) == 0) {
        return (int)available;
    }
    long total = 0, free = 0, buffers = 0, cached = 0;
     if (get_mem_info("MemTotal:", &total) == 0 &&
        get_mem_info("MemFree:", &free) == 0 &&
        get_mem_info("Buffers:", &buffers) == 0 &&
        get_mem_info("Cached:", &cached) == 0) {
        return (int)(free + buffers + cached);
    }
    return -1;
}
int get_storage_info(const char* path, long long* total, long long* free) {
    struct statvfs vfs;
    if (statvfs(path, &vfs) != 0) {
        perror("statvfs");
        return -1;
    }
    *total = (long long)vfs.f_blocks * vfs.f_frsize;
    *free = (long long)vfs.f_bavail * vfs.f_frsize;
    return 0;
}
int get_storage_used(void) {
    long long total = 0, free = 0;
    if (get_storage_info("/", &total, &free) == 0) {
        return (int)(total - free);
    }
    return -1;
}
int get_storage_available(void) {
    long long total = 0, free = 0;
    if (get_storage_info("/", &total, &free) == 0) {
        return (int)free;
    }
    return -1;
}
char* get_timezone(void) {
    if (custom_timezone[0] != '\0') {
        return strdup(custom_timezone);
    }
    tzset();
    if (daylight) {
        return strdup(tzname[1]);
    } else {
        return strdup(tzname[0]);
    }
}
int get_temperature(void) {
    return -1;
}
int get_cpu_usage(void) {
    return -1;
}
int get_processor_speed(void) {
    FILE* f = fopen("/proc/cpuinfo", "r");
    char line[256];
    if (!f) {
        perror("fopen /proc/cpuinfo");
        return -1;
    }
    while (fgets(line, sizeof(line), f)) {
        double speed_mhz;
        if (sscanf(line, "cpu MHz : %lf", &speed_mhz) == 1) {
            fclose(f);
            return (int)speed_mhz;
        }
    }
    fclose(f);
    return -1;
}
char* get_os_info(void) {
     FILE* f = fopen("/etc/os-release", "r");
    char line[256];
    char* pretty_name = NULL;
    if (!f) {
        perror("fopen /etc/os-release");
        return strdup("Unknown OS Info");
    }
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, "PRETTY_NAME=") == line) {
            char* start = strchr(line, '\"');
            char* end = strrchr(line, '\"');
            if (start && end && start < end) {
                size_t len = end - start - 1;
                pretty_name = (char*) malloc(len + 1);
                if (pretty_name) {
                    strncpy(pretty_name, start + 1, len);
                    pretty_name[len] = '\0';
                }
            }
            break;
        }
    }
    fclose(f);
    if (pretty_name) {
        return pretty_name;
    } else {
         static struct utsname uts;
        if (uname(&uts) != 0) {
            perror("uname for fallback OS Info");
            return strdup("Unknown OS Info");
        }
        char fallback_info[512];
        snprintf(fallback_info, sizeof(fallback_info), "%s %s (%s)", uts.sysname, uts.release, uts.machine);
        return strdup(fallback_info);
    }
}
char* get_kernel_version(void) {
    static struct utsname uts;
    if (uname(&uts) != 0) {
        perror("uname");
        return strdup("Unknown Kernel Version");
    }
    return strdup(uts.release);
}
char* get_location(void) {
    return strdup("Location not configured");
}
int handle_system_info(netsnmp_mib_handler *handler,
                       netsnmp_handler_registration *reginfo,
                       netsnmp_agent_request_info *reqinfo,
                       netsnmp_request_info *requests) {
    switch (reqinfo->mode) {
        case MODE_GET: {
            netsnmp_variable_list *var = requests->requestvb;
            long value_long;
            int value_int;
            char* value_str;
            u_char* value_bytes;
            size_t value_len;
            oid subid = var->name[OID_LENGTH(system_info_oid)];
            switch (subid) {
                case 4:
                    value_str = get_os_name();
                    if (value_str) {
                         snmp_set_var_typed_value(var, ASN_OCTET_STR, (u_char*)value_str, strlen(value_str));
                         free(value_str);
                    } else {
                         snmp_set_var_typed_value(var, ASN_OCTET_STR, (const u_char*)"Error", strlen("Error"));
                    }
                    break;
                case 5:
                    value_str = get_architecture();
                     if (value_str) {
                         snmp_set_var_typed_value(var, ASN_OCTET_STR, (u_char*)value_str, strlen(value_str));
                         free(value_str);
                    } else {
                         snmp_set_var_typed_value(var, ASN_OCTET_STR, (const u_char*)"Error", strlen("Error"));
                    }
                    break;
                case 6:
                    value_long = get_system_uptime();
                    if (value_long != -1) {
                        snmp_set_var_typed_value(var, ASN_INTEGER, (u_char*)&value_long, sizeof(value_long));
                    } else {
                         netsnmp_set_request_error(reqinfo, requests, SNMP_NOSUCHINSTANCE);
                    }
                    break;
                case 7:
                    value_bytes = get_mac_address(&value_len);
                     if (value_bytes && value_len > 0) {
                         snmp_set_var_typed_value(var, ASN_OCTET_STR, value_bytes, value_len);
                         free(value_bytes);
                     } else {
                         free(value_bytes);
                          netsnmp_set_request_error(reqinfo, requests, SNMP_NOSUCHINSTANCE);
                     }
                    break;
                case 8:
                    value_str = get_ip_addresses();
                     if (value_str) {
                         snmp_set_var_typed_value(var, ASN_OCTET_STR, (u_char*)value_str, strlen(value_str));
                         free(value_str);
                    } else {
                         snmp_set_var_typed_value(var, ASN_OCTET_STR, (const u_char*)"Error", strlen("Error"));
                    }
                    break;
                case 9:
                    value_int = get_mem_used();
                     if (value_int != -1) {
                        snmp_set_var_typed_value(var, ASN_INTEGER, (u_char*)&value_int, sizeof(value_int));
                    } else {
                         netsnmp_set_request_error(reqinfo, requests, SNMP_NOSUCHINSTANCE);
                    }
                    break;
                 case 10:
                    value_int = get_mem_available();
                     if (value_int != -1) {
                        snmp_set_var_typed_value(var, ASN_INTEGER, (u_char*)&value_int, sizeof(value_int));
                    } else {
                         netsnmp_set_request_error(reqinfo, requests, SNMP_NOSUCHINSTANCE);
                    }
                    break;
                case 11:
                    value_int = get_storage_used();
                     if (value_int != -1) {
                        snmp_set_var_typed_value(var, ASN_INTEGER, (u_char*)&value_int, sizeof(value_int));
                    } else {
                         netsnmp_set_request_error(reqinfo, requests, SNMP_NOSUCHINSTANCE);
                    }
                    break;
                case 12:
                    value_int = get_storage_available();
                     if (value_int != -1) {
                        snmp_set_var_typed_value(var, ASN_INTEGER, (u_char*)&value_int, sizeof(value_int));
                    } else {
                         netsnmp_set_request_error(reqinfo, requests, SNMP_NOSUCHINSTANCE);
                    }
                    break;
                case 13:
                    switch (reqinfo->mode) {
                        case MODE_GET:
                            value_str = get_timezone();
                            if (value_str) {
                                snmp_set_var_typed_value(var, ASN_OCTET_STR, (u_char*)value_str, strlen(value_str));
                                free(value_str);
                            } else {
                                snmp_set_var_typed_value(var, ASN_OCTET_STR, (const u_char*)"Error", strlen("Error"));
                            }
                            break;
                        case MODE_SET_RESERVE1:
                            if (requests->requestvb->type != ASN_OCTET_STR) {
                                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_WRONGTYPE);
                            }
                            break;
                        case MODE_SET_ACTION:
                            {
                                size_t len = requests->requestvb->val_len;
                                if (len >= sizeof(custom_timezone)) len = sizeof(custom_timezone) - 1;
                                memcpy(custom_timezone, requests->requestvb->val.string, len);
                                custom_timezone[len] = '\0';
                            }
                            break;
                        default:
                            return SNMP_ERR_GENERR;
                    }
                    break;
                case 14:
                     value_int = get_temperature();
                     if (value_int != -1) {
                        snmp_set_var_typed_value(var, ASN_INTEGER, (u_char*)&value_int, sizeof(value_int));
                    } else {
                         netsnmp_set_request_error(reqinfo, requests, SNMP_NOSUCHINSTANCE);
                    }
                    break;
                case 15:
                     value_int = get_cpu_usage();
                      if (value_int != -1) {
                        snmp_set_var_typed_value(var, ASN_INTEGER, (u_char*)&value_int, sizeof(value_int));
                    } else {
                         netsnmp_set_request_error(reqinfo, requests, SNMP_NOSUCHINSTANCE);
                    }
                    break;
                case 16:
                     value_int = get_processor_speed();
                      if (value_int != -1) {
                        snmp_set_var_typed_value(var, ASN_INTEGER, (u_char*)&value_int, sizeof(value_int));
                    } else {
                         netsnmp_set_request_error(reqinfo, requests, SNMP_NOSUCHINSTANCE);
                    }
                    break;
                 case 17:
                     value_str = get_os_info();
                     if (value_str) {
                         snmp_set_var_typed_value(var, ASN_OCTET_STR, (u_char*)value_str, strlen(value_str));
                         free(value_str);
                     } else {
                          snmp_set_var_typed_value(var, ASN_OCTET_STR, (const u_char*)"Error", strlen("Error"));
                     }
                    break;
                case 18:
                     value_str = get_kernel_version();
                     if (value_str) {
                         snmp_set_var_typed_value(var, ASN_OCTET_STR, (u_char*)value_str, strlen(value_str));
                         free(value_str);
                     } else {
                          snmp_set_var_typed_value(var, ASN_OCTET_STR, (const u_char*)"Error", strlen("Error"));
                     }
                    break;
                case 19:
                     value_str = get_location();
                     if (value_str) {
                         snmp_set_var_typed_value(var, ASN_OCTET_STR, (u_char*)value_str, strlen(value_str));
                         free(value_str);
                     } else {
                          snmp_set_var_typed_value(var, ASN_OCTET_STR, (const u_char*)"Error", strlen("Error"));
                     }
                    break;
                default:
                    netsnmp_set_request_error(reqinfo, requests, SNMP_NOSUCHOBJECT);
                    break;
            }
            break;
        }
        default:
            return SNMP_ERR_GENERR;
    }
    return SNMP_ERR_NOERROR;
}
void init_system_info(void) {
    printf("Init SYSTEM_INFO\n");
    const oid osName_oid[] = { 1, 3, 6, 1, 4, 1, 112233, 4 };
    netsnmp_register_scalar(
        netsnmp_create_handler_registration("osName", handle_system_info, osName_oid, OID_LENGTH(osName_oid), HANDLER_CAN_RONLY)
    );
    const oid architecture_oid[] = { 1, 3, 6, 1, 4, 1, 112233, 5 };
    netsnmp_register_scalar(
        netsnmp_create_handler_registration("architecture", handle_system_info, architecture_oid, OID_LENGTH(architecture_oid), HANDLER_CAN_RONLY)
    );
    const oid uptime_oid[] = { 1, 3, 6, 1, 4, 1, 112233, 6 };
    netsnmp_register_scalar(
        netsnmp_create_handler_registration("uptime", handle_system_info, uptime_oid, OID_LENGTH(uptime_oid), HANDLER_CAN_RONLY)
    );
    const oid macAddress_oid[] = { 1, 3, 6, 1, 4, 1, 112233, 7 };
    netsnmp_register_scalar(
        netsnmp_create_handler_registration("macAddress", handle_system_info, macAddress_oid, OID_LENGTH(macAddress_oid), HANDLER_CAN_RONLY)
    );
    const oid ipAddresses_oid[] = { 1, 3, 6, 1, 4, 1, 112233, 8 };
    netsnmp_register_scalar(
        netsnmp_create_handler_registration("ipAddresses", handle_system_info, ipAddresses_oid, OID_LENGTH(ipAddresses_oid), HANDLER_CAN_RONLY)
    );
    const oid memUsed_oid[] = { 1, 3, 6, 1, 4, 1, 112233, 9 };
    netsnmp_register_scalar(
        netsnmp_create_handler_registration("memUsed", handle_system_info, memUsed_oid, OID_LENGTH(memUsed_oid), HANDLER_CAN_RONLY)
    );
    const oid memAvailable_oid[] = { 1, 3, 6, 1, 4, 1, 112233, 10 };
    netsnmp_register_scalar(
        netsnmp_create_handler_registration("memAvailable", handle_system_info, memAvailable_oid, OID_LENGTH(memAvailable_oid), HANDLER_CAN_RONLY)
    );
    const oid storageUsed_oid[] = { 1, 3, 6, 1, 4, 1, 112233, 11 };
    netsnmp_register_scalar(
        netsnmp_create_handler_registration("storageUsed", handle_system_info, storageUsed_oid, OID_LENGTH(storageUsed_oid), HANDLER_CAN_RONLY)
    );
    const oid storageAvailable_oid[] = { 1, 3, 6, 1, 4, 1, 112233, 12 };
    netsnmp_register_scalar(
        netsnmp_create_handler_registration("storageAvailable", handle_system_info, storageAvailable_oid, OID_LENGTH(storageAvailable_oid), HANDLER_CAN_RONLY)
    );
    const oid timezone_oid[] = { 1, 3, 6, 1, 4, 1, 112233, 13 };
    netsnmp_register_scalar(
        netsnmp_create_handler_registration("timezone", handle_system_info, timezone_oid, OID_LENGTH(timezone_oid), HANDLER_CAN_RWRITE)
    );
    const oid temperature_oid[] = { 1, 3, 6, 1, 4, 1, 112233, 14 };
    netsnmp_register_scalar(
        netsnmp_create_handler_registration("temperature", handle_system_info, temperature_oid, OID_LENGTH(temperature_oid), HANDLER_CAN_RONLY)
    );
    const oid cpuUsage_oid[] = { 1, 3, 6, 1, 4, 1, 112233, 15 };
    netsnmp_register_scalar(
        netsnmp_create_handler_registration("cpuUsage", handle_system_info, cpuUsage_oid, OID_LENGTH(cpuUsage_oid), HANDLER_CAN_RONLY)
    );
    const oid processorSpeed_oid[] = { 1, 3, 6, 1, 4, 1, 112233, 16 };
    netsnmp_register_scalar(
        netsnmp_create_handler_registration("processorSpeed", handle_system_info, processorSpeed_oid, OID_LENGTH(processorSpeed_oid), HANDLER_CAN_RONLY)
    );
    const oid osInfo_oid[] = { 1, 3, 6, 1, 4, 1, 112233, 17 };
     netsnmp_register_scalar(
        netsnmp_create_handler_registration("osInfo", handle_system_info, osInfo_oid, OID_LENGTH(osInfo_oid), HANDLER_CAN_RONLY)
    );
    const oid kernelVersion_oid[] = { 1, 3, 6, 1, 4, 1, 112233, 18 };
     netsnmp_register_scalar(
        netsnmp_create_handler_registration("kernelVersion", handle_system_info, kernelVersion_oid, OID_LENGTH(kernelVersion_oid), HANDLER_CAN_RONLY)
    );
    const oid location_oid[] = { 1, 3, 6, 1, 4, 1, 112233, 19 };
     netsnmp_register_scalar(
        netsnmp_create_handler_registration("location", handle_system_info, location_oid, OID_LENGTH(location_oid), HANDLER_CAN_RONLY)
    );
}

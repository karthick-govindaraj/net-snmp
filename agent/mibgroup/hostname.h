#ifndef HOSTNAME_H
#define HOSTNAME_H
char* get_system_hostname(void);
int set_system_hostname(const char* new_hostname);
void init_hostname(void);
#endif 
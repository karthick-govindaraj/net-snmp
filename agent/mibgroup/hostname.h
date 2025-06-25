#ifndef HOSTNAME_H
#define HOSTNAME_H

/**
 * Get the current system hostname
 * @return Pointer to hostname string, NULL on error
 */
char* get_system_hostname(void);

/**
 * Set the system hostname
 * @param new_hostname New hostname to set
 * @return 0 on success, negative value on error:
 *         -1: Invalid hostname (NULL, empty, or too long)
 *         -2: Invalid characters in hostname
 *         -3: Failed to set hostname (permission issue)
 */
int set_system_hostname(const char* new_hostname);

/**
 * Initialize the hostname MIB module
 */
void init_hostname(void);

#endif /* HOSTNAME_H */
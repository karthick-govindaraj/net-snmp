#ifndef HOSTNAME_H
#define HOSTNAME_H


/**
 * Initializes the custom hostname scalar OID handler.
 * This function must be called during agent initialization.
 */
char* get_system_hostname(void);
void init_hostname(void);

/**
 * Optional: Declare get_system_hostname if it's a custom function.
 * If you are using a system-provided function, you can omit this.
 */
// const char* get_system_hostname(void);

#endif /* HOSTNAME_H */


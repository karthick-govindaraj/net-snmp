#ifndef REBOOT_H
#define REBOOT_H

// Reboot action constants
#define REBOOT_IMMEDIATE    1

// Function prototypes
int get_reboot_status(void);
int set_reboot_action(int action);
void init_reboot(void);

#endif /* REBOOT_H */
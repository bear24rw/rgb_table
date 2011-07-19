#ifndef SERIAL_H
#define SERIAL_H

#define SERIAL_DEV      "/dev/ttyUSB0"
#define SERIAL_BAUD     B2400

int init_serial(void);
int send_serial(void);

#endif

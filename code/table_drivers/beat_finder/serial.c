#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h> 

#include "main.h"
#include "draw.h"
#include "table.h"
#include "serial.h"

int serial_file;

int init_serial( void )
{
    serial_file = open(SERIAL_DEV, O_RDWR | O_NOCTTY | O_NDELAY);
    if (serial_file == -1)
    {
        printf("Could not open serial port\n");
        return 1;
    }

    fcntl(serial_file, F_SETFL, 0);

    struct termios options;

    tcgetattr(serial_file, &options);

    cfsetispeed(&options, SERIAL_BAUD);
    cfsetospeed(&options, SERIAL_BAUD);
    options.c_cflag |= (CLOCAL | CREAD);

    tcsetattr(serial_file, TCSANOW, &options);

    return 0;
}

int send_serial( void )
{
    unsigned char data = 0;

    if (lights[0].state) data += 1;
    if (lights[2].state) data += 2;
    if (lights[4].state) data += 4;
    if (lights[6].state) data += 8;

    if (lights[1].state) data += 16;
    if (lights[3].state) data += 32;
    if (lights[5].state) data += 64;
    if (lights[7].state) data += 128;

    int n = write(serial_file, &data, 1);
    if (n < 0)
    {
        printf("write() failed!\n");
        return 1;    
    }

    return 0;
}

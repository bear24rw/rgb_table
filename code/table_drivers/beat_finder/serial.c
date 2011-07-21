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
    static int x, y = 0;

    static unsigned char start_byte = 255;

    // send start byte
    if (write(serial_file, &start_byte, 1) != 1) return 1;

    // send table data
    for (y=0; y<TABLE_HEIGHT; y++)
    {
        for (x=0; x<TABLE_WIDTH; x++)
        {
            if (write(serial_file, &table[x][y].r, 1) != 1) return 1;
            if (write(serial_file, &table[x][y].g, 1) != 1) return 1;
            if (write(serial_file, &table[x][y].b, 1) != 1) return 1;
        }
    }

    return 0;
}

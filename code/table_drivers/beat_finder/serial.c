#include <stdio.h>
#include <stdlib.h>
#include <ftdi.h>

#include "main.h"
#include "draw.h"
#include "table.h"
#include "serial.h"

struct ftdi_context ftdic;

unsigned char flat_table[TABLE_HEIGHT*TABLE_WIDTH*3];

int init_serial( void )
{
    if (ftdi_init(&ftdic) < 0)
    {   
        fprintf(stderr, "ftdi_init failed\n");
        return 1;
    }  

    ftdi_set_interface(&ftdic, INTERFACE_ANY);

    int f = ftdi_usb_open(&ftdic, 0x0403, 0x6001);
    if (f < 0)
    {   
        fprintf(stderr, "unable to open ftdi device: %d (%s)\n", f, ftdi_get_error_string(&ftdic));
        return 1;
    }   

    f = ftdi_set_baudrate(&ftdic, BAUD);
    if (f < 0)
    {   
        fprintf(stderr, "unable to set baudrate: %d (%s)\n", f, ftdi_get_error_string(&ftdic));
        return 1;  
    }   
            
    return 0;
}

int send_serial( void )
{
    static int x, y = 0;

    static unsigned char start_byte = 255;
    ftdi_write_data(&ftdic, &start_byte, 1);

    // index into the flat_table array
    int index = 0;

    // flatten table into a 1D array 
    for (y=0; y<TABLE_HEIGHT; y++)
    {
        for (x=0; x<TABLE_WIDTH; x++)
        {
            // make sure we don't try to send a start byte 
            if (table[x][y].r > 254 || table[x][y].g > 254 || table[x][y].b > 254) 
                printf("[ERROR] Found start byte in data array!\n");

            // half the rows are wired backwards
            if (y>=TABLE_HEIGHT/2)
            {
                flat_table[index] = table[x][y].r; index++;
                flat_table[index] = table[x][y].g; index++;
                flat_table[index] = table[x][y].b; index++;
            }
            else
            {
                flat_table[index] = table[TABLE_WIDTH-x-1][y].r; index++;
                flat_table[index] = table[TABLE_WIDTH-x-1][y].g; index++;
                flat_table[index] = table[TABLE_WIDTH-x-1][y].b; index++;
            }
        }
    }

    // send the flat array
    ftdi_write_data(&ftdic, flat_table, TABLE_WIDTH*TABLE_HEIGHT*3);

    return 0;
}

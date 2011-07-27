#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <ftdi.h>

#define BAUD 115200
struct ftdi_context ftdic;

#define WIDTH   16
#define HEIGHT  8

unsigned char table[WIDTH][HEIGHT][3];

void send(void);

int init_serial(void)
{
    int vid = 0x0403;
    int pid = 0x6001;
    int interface = INTERFACE_ANY;
    int f;

    if (ftdi_init(&ftdic) < 0)
    {
        fprintf(stderr, "ftdi_init failed\n");
        return 1;
    }

    ftdi_set_interface(&ftdic, interface);

    f = ftdi_usb_open(&ftdic, vid, pid);
    if (f < 0)
    {
        fprintf(stderr, "unable to open ftdi device: %d (%s)\n", f, ftdi_get_error_string(&ftdic));
        return 1;
    }

    // Set baudrate
    f = ftdi_set_baudrate(&ftdic, BAUD);
    if (f < 0)
    {
        fprintf(stderr, "unable to set baudrate: %d (%s)\n", f, ftdi_get_error_string(&ftdic));
        return 1; 
    }

    return 0;
}

int send_serial(unsigned char data)
{
    ftdi_write_data(&ftdic, &data, 1);
    return 1;
}

void send(void)
{
    int x,y;

    // send out start byte
    send_serial(0xff);

    // send out table data
    for (y=0; y<HEIGHT; y++)
    {
        for (x=0; x<WIDTH; x++)
        {
            if (y>=HEIGHT/2)
            {
                send_serial(table[x][y][0]);
                send_serial(table[x][y][1]);
                send_serial(table[x][y][2]);
            }
            else
            {
                send_serial(table[WIDTH-x-1][y][0]);
                send_serial(table[WIDTH-x-1][y][1]);
                send_serial(table[WIDTH-x-1][y][2]);
            }
        }
    }

}

void set_all(unsigned char r, unsigned char g, unsigned char b)
{
    int x,y;
    for (x=0; x<WIDTH; x++)
    {
        for (y=0; y<HEIGHT; y++)
        {
            table[x][y][0] = r;
            table[x][y][1] = g;
            table[x][y][2] = b;
        }
    }

    send();
}

void clear(void)
{
    int x,y;
    for (x=0; x<WIDTH; x++)
    {
        for (y=0; y<HEIGHT; y++)
        {
            table[x][y][0] = 0;
            table[x][y][1] = 0;
            table[x][y][2] = 0;
        }
    }

    send();
}

void walk(void)
{
    clear();

    int x,y;
    for (y=0; y<HEIGHT; y++)
    {
        for (x=0; x<WIDTH; x++)
        {
            table[x][y][0] = 254;
            table[x][y][1] = 254;
            table[x][y][2] = 254;

            send();

            sleep(.25);
        }
    }
}

void loop_rows(void)
{
    clear();

    int x,y;
    for (y=0; y<HEIGHT; y++)
    {
        clear();

        for (x=0; x<WIDTH; x++)
        {
            table[x][y][0] = 254;
            table[x][y][1] = 0;
            table[x][y][2] = 0;
        }

        send();

        sleep(1);
    }
}

void loop_cols(void)
{
    clear();

    int x,y;
    for (x=0; x<WIDTH; x++)
    {
        clear();

        for (y=0; y<HEIGHT; y++)
        {
            table[x][y][0] = 254;
            table[x][y][1] = 0;
            table[x][y][2] = 0;
        }

        send();

        sleep(1);
    }
}

void fade(void)
{
    clear();

    int fade=0;
    for (fade=0; fade<255; fade++)
    {
        set_all(fade,fade,fade);
        sleep(1);
    }
    for (fade=254; fade>-1; fade--)
    {
        set_all(fade, fade,fade);
        sleep(1);
    }
}


int main(void)
{
    if (! init_serial())
    {
        printf("Could not open serial port!\n");
        return 1;
    }

    int count = 0;

    int SLEEP = 3;

    while (1)
    {
        set_all(10,10,10);
        continue;

        printf("Walk\n");
        walk();

        printf("Set all red\n");
        set_all(254,0,0);
        sleep(SLEEP);

        printf("Set all green\n");
        set_all(0,254,0);
        sleep(SLEEP);

        printf("Set all blue\n");
        set_all(0,0,254);
        sleep(SLEEP);

        printf("Set all red/blue\n");
        set_all(254,0,254);
        sleep(SLEEP);

        printf("Set all red/green\n");
        set_all(254,254,0);
        sleep(SLEEP);

        printf("Set all green/blue\n");
        set_all(0,254,254);
        sleep(SLEEP);

        printf("Set all white\n");
        set_all(254,254,254);
        sleep(SLEEP);

        printf("Loop cols\n");
        loop_cols();

        printf("Loop rows\n");
        loop_rows();

        printf("Fade\n");
        clear();
        fade();

        printf("Clear\n");
        clear();
        sleep(SLEEP);
    }

    return 0;
}

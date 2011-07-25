#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "main.h"
#include "table.h"

struct pixel table[TABLE_WIDTH][TABLE_HEIGHT];
struct pixel tmp_table[TABLE_WIDTH][TABLE_HEIGHT];

struct pulse pulses[NUM_LIGHTS];

unsigned char offset_circle = 0;
unsigned char first_assigned = 1;
unsigned char pulse_pulses = 1;

int i,x,y;

void init_table(void)
{
    // randomize all the pulses
    for (i=0; i<NUM_LIGHTS; i++)
    {
        pulses[i].x = rand() % TABLE_WIDTH;
        pulses[i].y = rand() % TABLE_HEIGHT; 
        pulses[i].decay = 0;
        pulses[i].radius = PULSE_RADIUS;
    }

    clear_table();
    clear_tmp_table();
}

void clear_table(void)
{
    for (x=0; x<TABLE_WIDTH; x++)
    {
        for (y=0; y<TABLE_HEIGHT; y++)
        {
            table[x][y].r = 0;
            table[x][y].g = 0;
            table[x][y].b = 0;
        }
    }
}

void clear_tmp_table(void)
{
    for (x=0; x<TABLE_WIDTH; x++)
    {
        for (y=0; y<TABLE_HEIGHT; y++)
        {
            tmp_table[x][y].r = 0;
            tmp_table[x][y].g = 0;
            tmp_table[x][y].b = 0;
        }
    }
}


void draw_pulse(int i)
{
    clear_tmp_table();

    double r;
    double angle;

    int radius = PULSE_RADIUS;
    
    if (clipped & pulse_pulses) radius *= PULSE_CLIP_SCALE;

    for (r=0; r<=radius; r+=0.1)
    {
        for (angle=0; angle<6.28318; angle+=0.05)
        {
            int x,y;

            if (offset_circle)
            {
                x = floor(pulses[i].x + 0.5 + cos(angle)*r);
                y = floor(pulses[i].y + 0.5 + sin(angle)*r);
            }
            else
            {
                x = floor(pulses[i].x + cos(angle)*r);
                y = floor(pulses[i].y + sin(angle)*r);
            }

            if (x > TABLE_WIDTH - 1) continue;
            if (y > TABLE_HEIGHT - 1) continue;
            if (x < 0) continue;
            if (y < 0) continue;

            double decay_percent = ((float)pulses[i].decay / (float)LIGHT_DECAY);
            double radius_percent = 1.0 - ((float)r / (float)radius);
            //double radius_percent = log(-1*r+radius+1)/log(radius+1);
            
            int r = ( pulses[i].r * radius_percent * decay_percent);
            int g = ( pulses[i].g * radius_percent * decay_percent);
            int b = ( pulses[i].b * radius_percent * decay_percent);

            if (r < 0) r = 0;
            if (g < 0) g = 0;
            if (b < 0) b = 0;
            if (r > 254) r = 254;
            if (g > 254) g = 254;
            if (b > 254) b = 254;

            // only change the color we haven't assigned a color already
            if (first_assigned && tmp_table[x][y].r == 0 && tmp_table[x][y].g == 0 && tmp_table[x][y].b == 0)
            {
              tmp_table[x][y].r = r;
              tmp_table[x][y].g = g;
              tmp_table[x][y].b = b;
            }
            else if (!first_assigned)
            {
              tmp_table[x][y].r = r;
              tmp_table[x][y].g = g;
              tmp_table[x][y].b = b;
            }
        }
    }

    for (x=0; x<TABLE_WIDTH; x++)
    {
        for (y=0; y<TABLE_HEIGHT; y++)
        {
            if (table[x][y].r + tmp_table[x][y].r > 254) table[x][y].r = 254; else table[x][y].r += tmp_table[x][y].r;
            if (table[x][y].g + tmp_table[x][y].g > 254) table[x][y].g = 254; else table[x][y].g += tmp_table[x][y].g;
            if (table[x][y].b + tmp_table[x][y].b > 254) table[x][y].b = 254; else table[x][y].b += tmp_table[x][y].b;
        }
    }
}

// draw the history buffer as the table background
void table_draw_hist_bg(double perc)
{
    for (x=0; x<TABLE_WIDTH; x++)
    {
        for (y=0; y<TABLE_HEIGHT; y++)
        {
            float r = (254*fft_bin[i].hist[y+(HIST_SIZE-TABLE_HEIGHT)])/fft_global_hist_mag_max;
            float b = (254*fft_bin[i].hist_std)/(fft_global_hist_std_max);
            float g = 0;
            
            // if this was a beat, color it white
            //if (fft_bin_triggered_hist[x][y+(HIST_SIZE-TABLE_HEIGHT)]) {r = 255; g = 255; b = 255;}

            // scale intensity down
            r *= perc;
            g *= perc;
            b *= perc;

            if (r > 254) r = 254;
            if (g > 254) g = 254;
            if (b > 254) b = 254;

            table[x][y].r = r;
            table[x][y].b = b;
            table[x][y].g = g;
        }
    }
}

void assign_cells(void)
{
    clear_table();

    //table_draw_hist_bg(1.0);

    for (i=0; i<NUM_LIGHTS; i++)
    {
        draw_pulse(i);
    }


}


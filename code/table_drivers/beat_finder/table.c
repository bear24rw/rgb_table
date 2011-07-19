#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "main.h"
#include "table.h"

struct pixel table[TABLE_WIDTH][TABLE_HEIGHT];
struct pixel tmp_table[TABLE_WIDTH][TABLE_HEIGHT];

struct pulse pulses[NUM_LIGHTS];

int i,x,y;

void init_pulses(void)
{
    for (i=0; i<NUM_LIGHTS; i++)
    {
        pulses[i].x = rand() % TABLE_WIDTH;
        pulses[i].y = rand() % TABLE_HEIGHT; 
        pulses[i].decay = 0;
    }

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


void draw_circle(int x, int y, int radius, int decay, int r, int g, int b)
{
    clear_tmp_table();

    double rr;
    for (rr=0; rr<=radius; rr+=0.1)
    {
        double angle;
        for (angle=0; angle<6.28318; angle+=0.05)
        {
            int xx = floor(x + cos(angle)*rr);
            int yy = floor(y + sin(angle)*rr);

            if (xx > TABLE_WIDTH - 1) continue;
            if (yy > TABLE_HEIGHT - 1) continue;
            if (xx < 0) continue;
            if (yy < 0) continue;

            tmp_table[xx][yy].r = ( r - 255.0 * (float)rr / (float)radius - 255.0 * (1.0 - (float)decay / (float)LIGHT_DECAY )) ;
            tmp_table[xx][yy].g = ( g - 255.0 * (float)rr / (float)radius - 255.0 * (1.0 - (float)decay / (float)LIGHT_DECAY )) ;
            tmp_table[xx][yy].b = ( b - 255.0 * (float)rr / (float)radius - 255.0 * (1.0 - (float)decay / (float)LIGHT_DECAY )) ;

            if (tmp_table[xx][yy].r < 0) tmp_table[xx][yy].r = 0;
            if (tmp_table[xx][yy].g < 0) tmp_table[xx][yy].g = 0;
            if (tmp_table[xx][yy].b < 0) tmp_table[xx][yy].b = 0;
        }
    }

    for (x=0; x<TABLE_WIDTH; x++)
    {
        for (y=0; y<TABLE_HEIGHT; y++)
        {
            table[x][y].r += tmp_table[x][y].r;
            table[x][y].g += tmp_table[x][y].g;
            table[x][y].b += tmp_table[x][y].b;

            if (table[x][y].r > 255) table[x][y].r = 255;
            if (table[x][y].g > 255) table[x][y].g = 255;
            if (table[x][y].b > 255) table[x][y].b = 255;
        }
    }
}

void increase_table_bg(float percent)
{
    for (x=0; x<TABLE_WIDTH; x++)
    {
        for (y=0; y<TABLE_HEIGHT; y++)
        {
            table[x][y].r += percent;
            table[x][y].b += percent;
            table[x][y].g += percent;
            
            if (table[x][y].r > 255) table[x][y].r = 255;
            if (table[x][y].g > 255) table[x][y].g = 255;
            if (table[x][y].b > 255) table[x][y].b = 255;
        }
    }
}

void draw_table_bg(void)
{
    int x,y;

    for (x=0; x<TABLE_WIDTH; x++)
    {
        for (y=0; y<TABLE_HEIGHT; y++)
        {
			float r = (255*fft_bin[i].hist[y+(HIST_SIZE-TABLE_HEIGHT)])/fft_global_hist_mag_max;
			float b = (255*fft_bin[i].hist_std)/(fft_global_hist_std_max);
			float g = 0;
			
            // if this was a beat, color it white
            //if (fft_bin_triggered_hist[x][y+(HIST_SIZE-TABLE_HEIGHT)]) {r = 255; g = 255; b = 255;}

            table[x][y].r = r * 0.1;
            table[x][y].b = b * 0.1;
            table[x][y].g = g * 0.1;

            if (table[x][y].r > 255) table[x][y].r = 255;
            if (table[x][y].g > 255) table[x][y].g = 255;
            if (table[x][y].b > 255) table[x][y].b = 255;
        }
    }
}

void assign_cells(void)
{
    clear_table();

    //draw_table_bg();

    //if (clipped) increase_table_bg(150);
   
    int radius = 4;
    if (clipped) radius *= 1.5;

    for (i=0; i<NUM_LIGHTS; i++)
    {
        draw_circle(pulses[i].x, pulses[i].y, radius, pulses[i].decay, pulses[i].r, pulses[i].g, pulses[i].b);
    }
}


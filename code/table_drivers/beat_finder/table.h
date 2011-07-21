#ifndef TABLE_H
#define TABLE_H

#define TABLE_WIDTH     16
#define TABLE_HEIGHT    8
#define CELL_SIZE       25

struct pixel
{
    unsigned char r,g,b;
    int h,s,v;
}; 

struct pulse
{
    int x,y,radius;
    int r,g,b;
    int decay;
};

extern struct pixel table[TABLE_WIDTH][TABLE_HEIGHT];
extern struct pixel tmp_table[TABLE_WIDTH][TABLE_HEIGHT];

extern struct pulse table_pulses[NUM_LIGHTS];

void init_table(void);
void clear_table(void);
void clear_tmp_table(void);
void draw_circle(int, int, int, int , int, int, int);
void increase_table_bg(float);
void draw_table_bg(void);
void assign_cells(void);

#endif

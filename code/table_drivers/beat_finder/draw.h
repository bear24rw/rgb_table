#ifndef DRAW_H
#define DRAW_H

#include <GL/gl.h>
#include <GL/glu.h>
#include <SDL.h>

#define SCREEN_BPP 16

extern unsigned char done;

void init_gl(void);
int init_sdl(void);
void resize_window(int, int);
void service_keys(void);
void draw_table(int, int);
void draw_real_img_plot(float, float);
void draw_mag(int, float, float, float);
void draw_mag_hist_avg(int, float, float);
void draw_mag_hist_var(int, float, float);
void draw_mag_hist(int, float, float);
void draw_lines_to_lights(void);
void draw_lights(void);
int draw_all(void);
int handle_sdl_events(void);

#endif

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <GL/gl.h>
#include <GL/glu.h>
#include <SDL.h>

#include "main.h"
#include "draw.h"
#include "table.h"

int SCREEN_WIDTH = 1024;
int SCREEN_HEIGHT = 768;

int videoFlags;

// camera position
GLfloat xpos = 25;
GLfloat ypos = 50;
GLfloat zpos = 0;

// how fast the camera moves
GLfloat movement = 5;

int mouse_x = 0;
int mouse_y = 0;

SDL_Event event;
SDL_Surface *surface;    

unsigned char done = FALSE;

int i,j,k = 0;

void init_gl(void)
{
    glShadeModel( GL_SMOOTH );         

    glClearColor( 0,0,0, 0.0f );     

    gluOrtho2D(0.0, SCREEN_WIDTH, 0.0, SCREEN_HEIGHT);

    glDisable(GL_DEPTH_TEST);

    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);    
    glEnable(GL_BLEND);                            
}

int init_sdl(void)
{
    /* this holds some info about our display */
    const SDL_VideoInfo *videoInfo;

    /* initialize SDL */
    if ( SDL_Init( SDL_INIT_VIDEO ) < 0 )
    {
        fprintf( stderr, "Video initialization failed: %s\n", SDL_GetError( ) );
        return 1;
    }

    /* Fetch the video info */
    videoInfo = SDL_GetVideoInfo( );

    if ( !videoInfo )
    {
        fprintf( stderr, "Video query failed: %s\n", SDL_GetError( ) );
        return 1;
    }

    /* the flags to pass to SDL_SetVideoMode */
    videoFlags  = SDL_OPENGL;          /* Enable OpenGL in SDL */
    videoFlags |= SDL_GL_DOUBLEBUFFER; /* Enable double buffering */
    videoFlags |= SDL_HWPALETTE;       /* Store the palette in hardware */
    videoFlags |= SDL_RESIZABLE;       /* Enable window resizing */

    /* This checks to see if surfaces can be stored in memory */
    if ( videoInfo->hw_available )
        videoFlags |= SDL_HWSURFACE;
    else
        videoFlags |= SDL_SWSURFACE;

    /* This checks if hardware blits can be done */
    if ( videoInfo->blit_hw ) videoFlags |= SDL_HWACCEL;

    /* Sets up OpenGL double buffering */
    SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );

    /* get a SDL surface */
    surface = SDL_SetVideoMode( SCREEN_WIDTH, SCREEN_HEIGHT, SCREEN_BPP, videoFlags );

    /* Verify there is a surface */
    if ( !surface )
    {
        fprintf( stderr,  "Video mode set failed: %s\n", SDL_GetError( ) );
        return 1;
    }

    resize_window( SCREEN_WIDTH, SCREEN_HEIGHT );

    return 0;
}


// reset viewport after a window resize
void resize_window( int width, int height )
{
    glViewport( 0, 0, ( GLint )width, ( GLint )height );

    glMatrixMode( GL_PROJECTION );
    glLoadIdentity( );

    gluOrtho2D(0.0, SCREEN_WIDTH, 0.0, SCREEN_HEIGHT);

    glMatrixMode( GL_MODELVIEW );
    glLoadIdentity( );
}


void draw_table( int off_x, int off_y)
{
    off_x = (SCREEN_WIDTH / 2) - ((TABLE_WIDTH * CELL_SIZE) / 2);
    off_y = SCREEN_HEIGHT - TABLE_HEIGHT*CELL_SIZE - 4*LIGHT_SIZE;

    int x,y = 0;
    for (x=0; x<TABLE_WIDTH; x++)
    {
        for (y=0; y<TABLE_HEIGHT; y++)
        {
            // draw outline of cell
            glBegin(GL_LINE_LOOP);
            glColor3ub( 100, 100, 100 );
            glVertex2f( off_x + x*CELL_SIZE         , off_y + y*CELL_SIZE );     // BL
            glVertex2f( off_x + x*CELL_SIZE         , off_y + (y+1)*CELL_SIZE ); // TL
            glVertex2f( off_x + (x+1)*CELL_SIZE     , off_y + (y+1)*CELL_SIZE ); // TR
            glVertex2f( off_x + (x+1)*CELL_SIZE     , off_y + y*CELL_SIZE );     // BR
            glEnd();

            // draw cell color
            glBegin(GL_QUADS);
            glColor3ub( table[x][y].r, table[x][y].g, table[x][y].b );
            glVertex2f( off_x + x*CELL_SIZE + 1     , off_y + y*CELL_SIZE + 1);     // BL
            glVertex2f( off_x + x*CELL_SIZE + 1     , off_y + (y+1)*CELL_SIZE -1);  // TL
            glVertex2f( off_x + (x+1)*CELL_SIZE - 1 , off_y + (y+1)*CELL_SIZE -1);  // TR
            glVertex2f( off_x + (x+1)*CELL_SIZE - 1 , off_y + y*CELL_SIZE + 1);     // BR
            glEnd();
        }
    }
}

// draw the fft bins on a real vs imaginary plot
void draw_real_img_plot( float off_x, float off_y)
{
    glBegin(GL_POINTS);

    for (i=0; i<FFT_NUM_BINS; i++)
    {
        if (fft_bin[i].triggered)
            glColor3ub( 255, 255, 255);
        else
            glColor3ub( 255, 0, 255);

        glVertex2f( off_x + fft_out[i][0]/5000, off_y + fft_out[i][1]/5000);
    }

    glEnd();
}

// draw a magnitude bar
void draw_mag(int i, float height, float off_x, float off_y )
{
    if (fft_bin[i].triggered)
        glColor3ub( 255, 255, 255);
    else
        glColor3ub( 255, 0, 0 );

    glBegin(GL_LINE_LOOP);

    glVertex2f( off_x + i*FFT_BIN_WIDTH         , off_y );           // BL
    glVertex2f( off_x + i*FFT_BIN_WIDTH         , off_y + height );  // TL
    glVertex2f( off_x + (i+1)*FFT_BIN_WIDTH     , off_y + height);   // TR
    glVertex2f( off_x + (i+1)*FFT_BIN_WIDTH     , off_y );

    glEnd();
}

// draw an average line
void draw_mag_hist_avg(int i, float off_x, float off_y)
{
    glBegin(GL_LINES);
    glColor3ub( 0, 255, 0 );
    glVertex2f( off_x + i*FFT_BIN_WIDTH     , off_y + fft_bin[i].hist_avg );
    glVertex2f( off_x + (i+1)*FFT_BIN_WIDTH    , off_y + fft_bin[i].hist_avg );
    glEnd();
}

// draw a variance line
void draw_mag_hist_var(int i, float off_x, float off_y)
{
    glBegin(GL_LINES);
    glColor3ub( 255, 255, 0);
    glVertex2f( off_x + i*FFT_BIN_WIDTH     , off_y + fft_bin[i].hist_std );
    glVertex2f( off_x + (i+1)*FFT_BIN_WIDTH    , off_y + fft_bin[i].hist_std );
    glEnd();
}

// draw magnitude history
void draw_mag_hist(int i, float off_x, float off_y)
{
    glBegin(GL_QUADS);

    for (k = 0; k < HIST_SIZE; k++)
    {
        float r = 255*fft_bin[i].hist[k]/fft_global_hist_mag_max;
        float b = 255*fft_bin[i].hist_std/fft_global_hist_std_max;
        float g = 0;

        // if this was a beat, color it white
        if (fft_bin[i].trigger_hist[k]) {r = 255; g = 255; b = 255;}

        glColor3ub( r, g, b );

        glVertex2f( off_x + (i+1)*FFT_BIN_WIDTH , off_y + k*FFT_BIN_WIDTH );        // TL
        glVertex2f( off_x + (i+1)*FFT_BIN_WIDTH , off_y + (k+1)*FFT_BIN_WIDTH );    // TR
        glVertex2f( off_x + i*FFT_BIN_WIDTH     , off_y + (k+1)*FFT_BIN_WIDTH );    // BR
        glVertex2f( off_x + i*FFT_BIN_WIDTH     , off_y + k*FFT_BIN_WIDTH );        // BL 
    }

    glEnd();

    // draw bar at bottom of history
    glBegin(GL_LINES);
    glColor3ub( 255, 0, 0 );
    glVertex2f(off_x, off_y);
    glVertex2f(off_x + FFT_NUM_BINS*FFT_BIN_WIDTH, off_y);
    glEnd();
}

// draws lines from the light boxes to the bin that it is currently assigned to
// line fades according to decay time
void draw_lines_to_lights( void )
{
    double x = (SCREEN_WIDTH / 2) - ((NUM_LIGHTS * LIGHT_SIZE + (NUM_LIGHTS/4-1)*LIGHT_SPACING) / 2);
    double y = SCREEN_HEIGHT - 2*LIGHT_SIZE;

    double s = 0;    // spacing

    for (i = 0; i < NUM_LIGHTS; i++)
    {
        // i > 1 so we dont put space in front of the first group
        if (i > 1 && i%4==0) s += LIGHT_SPACING;

        if (lights[i].decay > 0)
        {
            glBegin(GL_LINES);
            unsigned char c = 255 * lights[i].decay / LIGHT_DECAY;
            glColor3ub( c, c, c );
            glVertex2f( x + s + i*LIGHT_SIZE+(LIGHT_SIZE/2)                          , y ); // BL
            glVertex2f( xpos + lights[i].last_bin*FFT_BIN_WIDTH+(FFT_BIN_WIDTH/2)    , ypos + HIST_SIZE*FFT_BIN_WIDTH);
            glEnd();
        }
    }
}

void draw_lights( void )
{
    double x = (SCREEN_WIDTH / 2) - ((NUM_LIGHTS * LIGHT_SIZE + (NUM_LIGHTS/4-1)*LIGHT_SPACING) / 2);
    double y = SCREEN_HEIGHT - 2*LIGHT_SIZE;

    double s = 0;    // spacing

    for (i = 0; i < NUM_LIGHTS; i++)
    {
        // space out lights in groups of 4
        // i > 1 so we dont put space in front of the first group
        if (i > 1 && i%4==0) s += LIGHT_SPACING;

        // draw outline of lights
        glBegin(GL_LINE_LOOP);
        glColor3ub( 100, 100, 100 );
        glVertex2f( x + s + i*LIGHT_SIZE        , y );                  // BL
        glVertex2f( x + s + i*LIGHT_SIZE        , y + LIGHT_SIZE );     // TL
        glVertex2f( x + s + (i+1)*LIGHT_SIZE    , y + LIGHT_SIZE);      // TR
        glVertex2f( x + s + (i+1)*LIGHT_SIZE    , y );
        glEnd();

        // draw light if its on
        if (lights[i].state)
        {
            glBegin(GL_QUADS);
            glColor3ub( 25, 0, 255 );
            glVertex2f( x + s + i*LIGHT_SIZE + 5        , y + 5);               // BL
            glVertex2f( x + s + i*LIGHT_SIZE + 5        , y + LIGHT_SIZE - 5);  // TL
            glVertex2f( x + s + (i+1)*LIGHT_SIZE - 5    , y + LIGHT_SIZE - 5);  // TR
            glVertex2f( x + s + (i+1)*LIGHT_SIZE - 5    , y + 5 );              // BR
            glEnd();
        }
    }
}

int draw_all(void)
{
    glClear( GL_COLOR_BUFFER_BIT );

    glMatrixMode( GL_MODELVIEW );
    glLoadIdentity( );

    glTranslatef( xpos, ypos, zpos);

    // draw each bins mag, hist, hist_avg, hist_var
    for (i=0; i< FFT_NUM_BINS; i++)
    {
        draw_mag(i, fft_bin[i].mag , 0, HIST_SIZE*FFT_BIN_WIDTH);
        //draw_mag(i, fft_bin[i].diff , 0, HIST_SIZE*FFT_BIN_WIDTH);    // draw diff
        //draw_mag_hist_avg(i, 0, HIST_SIZE*FFT_BIN_WIDTH);
        //draw_mag_hist_var(i, 0, HIST_SIZE*FFT_BIN_WIDTH);
        draw_mag_hist(i, 0, 0);
    }

    /*
    // draw average var line
    glBegin(GL_LINES);
    glColor3ub( 255, 255, 0);
    glVertex2f( 0                , HIST_SIZE*FFT_BIN_WIDTH + fft_global_hist_std_avg);
    glVertex2f( FFT_NUM_BINS*FFT_BIN_WIDTH    , HIST_SIZE*FFT_BIN_WIDTH + fft_global_hist_std_avg);
    glEnd();

    // draw history average average line
    glBegin(GL_LINES);
    glColor3ub( 0, 255, 0);
    glVertex2f( 0                 , HIST_SIZE*FFT_BIN_WIDTH + fft_global_hist_avg);
    glVertex2f( FFT_NUM_BINS*FFT_BIN_WIDTH    , HIST_SIZE*FFT_BIN_WIDTH + fft_global_hist_avg);
    glEnd();
    */

    // draw mag clip
    if (USE_CLIP)
    {    
        glBegin(GL_LINES);
        if (clipped)
            glColor3ub(100,100,255);
        else
            glColor3ub(100,100,100);
        glVertex2f(0,                           HIST_SIZE*FFT_BIN_WIDTH + clip_mag);
        glVertex2f(FFT_NUM_BINS*FFT_BIN_WIDTH,  HIST_SIZE*FFT_BIN_WIDTH + clip_mag);
        glEnd();
    }

    // reset the view so we draw lights in center
    glTranslatef( -xpos, -ypos, -zpos);

    // draw lights
    draw_lights();

    // draw lines from the light to corresponding bin
    draw_lines_to_lights();

    draw_table(0,0);

    // draw the real vs img plot
    //draw_real_img_plot( SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);

    // draw raw signal
    glBegin(GL_LINE_STRIP);
    glColor3ub(100,100,100);
    for (i=0; i < SAMPLE_SIZE; i++)
        glVertex2f(i*SCREEN_WIDTH/SAMPLE_SIZE, 25 + fft_input[i] / 1000);
    glEnd();


    // draw test gradient
    /*
       for (i=0; i<360; i++)
       {
       int r,g,b;
       HSVtoRGB(&r,&g,&b,i,255,255);
       glBegin(GL_LINE_STRIP);
       glColor3ub(r,g,b);
       glVertex2f(i,200);
       glVertex2f(i,300);
       glEnd();
       }
       */



    // flip it to the screen
    SDL_GL_SwapBuffers( );

    return TRUE;
}

// handle key press events
void service_keys(void)
{
    Uint8 *keys = SDL_GetKeyState(0);

    if (keys[SDLK_ESCAPE])    done=TRUE;

    if (keys[SDLK_F1])    SDL_WM_ToggleFullScreen( surface );

    if (keys[SDLK_d])    xpos += (float)movement;
    if (keys[SDLK_a])    xpos -= (float)movement;
    if (keys[SDLK_w])    ypos -= (float)movement;
    if (keys[SDLK_s])    ypos += (float)movement;

    if (keys[SDLK_o])    { MAG_TRIGGER += 0.01; printf("Mag: %f\tVar: %f\n", MAG_TRIGGER, VAR_TRIGGER); }
    if (keys[SDLK_l])    { MAG_TRIGGER -= 0.01; printf("Mag: %f\tVar: %f\n", MAG_TRIGGER, VAR_TRIGGER); }
    if (keys[SDLK_i])    { VAR_TRIGGER += 0.01; printf("Mag: %f\tVar: %f\n", MAG_TRIGGER, VAR_TRIGGER); }
    if (keys[SDLK_k])    { VAR_TRIGGER -= 0.01; printf("Mag: %f\tVar: %f\n", MAG_TRIGGER, VAR_TRIGGER); }
}

int handle_sdl_events(void)
{
    while ( SDL_PollEvent( &event ) )
    {
        switch( event.type )
        {
            case SDL_VIDEORESIZE:

                surface = SDL_SetVideoMode( event.resize.w,event.resize.h, SCREEN_BPP, videoFlags );

                if ( !surface )
                {
                    fprintf( stderr, "Could not get a surface after resize: %s\n", SDL_GetError( ) );
                    return 1;
                }

                SCREEN_WIDTH = event.resize.w;
                SCREEN_HEIGHT = event.resize.h;

                resize_window( event.resize.w, event.resize.h );

                break;

            case SDL_KEYDOWN:
            case SDL_KEYUP:

                service_keys();
                break;
                /*
                   case SDL_MOUSEMOTION:

                   mouse_x = event.motion.x - xpos;
                   mouse_y = SCREEN_HEIGHT - event.motion.y - ypos;

                   printf("x: %4d\t y:%4d\n", mouse_x, mouse_y);

                   break;
                   */
            case SDL_QUIT:
                done = TRUE;
                break;

            default:
                break;

        }
    }

    return 0;
}

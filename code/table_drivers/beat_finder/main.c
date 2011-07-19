// http://stackoverflow.com/questions/604453/analyze-audio-using-fast-fourier-transform
// http://www.gamedev.net/reference/programming/features/beatdetection/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h> 

#include <GL/gl.h>
#include <GL/glu.h>
#include <SDL.h>

#include <fftw3.h>

#define TRUE  			1
#define FALSE 			0

#define SERIAL_DEV		"/dev/ttyUSB0"
#define SERIAL_BAUD		B2400
int serial_file;

#define SAMPLE_SIZE		1024
#define SAMPLE_RATE		44100
#define FFT_SIZE		(SAMPLE_SIZE / 2)
#define MAX_FREQ		8000
#define HIST_SIZE		13
#define FREQ_PER_BIN	(SAMPLE_RATE / SAMPLE_SIZE)
#define FFT_NUM_BINS	(MAX_FREQ/FREQ_PER_BIN)
#define FFT_BIN_WIDTH	10 // pixel width of bin

// magnitude clip
#define USE_CLIP				TRUE
#define USE_CLIP_DYN			TRUE		// 1 = use dynamic clip, 0 = use static
#define CLIP_STATIC_MAG			800			// static clip magnitude
double  clip_mag		=		0;			// dynamic magnitude clip
double	clip_mag_decay	=		0;			// dynamio clip decreases at rate of some function, this indexes the function
char	clipped 		=	 	0;			// 1 = we clipped a bin this loop

#define MAG_SCALE	10000
//Mag: 0.170000 Var: 0.220000
// default trigger levels for detecting beats
double MAG_TRIGGER	=	.58; //.36
double VAR_TRIGGER	=	.54; //.36

double fft_bin[FFT_NUM_BINS];
double fft_bin_avg;
double fft_bin_last[FFT_NUM_BINS];
double fft_bin_diff[FFT_NUM_BINS];

double fft_bin_hist[FFT_NUM_BINS][HIST_SIZE];

double fft_bin_hist_avg[FFT_NUM_BINS];		// bin history average
double fft_bin_hist_global_avg;				// average of all the bin history averages
double fft_bin_hist_global_max;				// max value of global history

double fft_bin_hist_std_idv[FFT_NUM_BINS][HIST_SIZE];
double fft_bin_hist_std[FFT_NUM_BINS];		// avg of the sum of the difference in energy from average
double fft_bin_hist_std_avg;				// avg of all the std deviations
double fft_bin_hist_std_max;				// max of all the std deviations

char fft_bin_triggered[FFT_NUM_BINS];
char fft_bin_triggered_hist[FFT_NUM_BINS][HIST_SIZE];
char fft_bin_pulse[FFT_NUM_BINS];


#define NUM_LIGHTS		(3*4)				// how many lights to display
#define LIGHT_DECAY		(HIST_SIZE*2)		// cycles until light is clear to trigger again
#define LIGHT_SIZE		50					// pixel size
#define LIGHT_SPACING	20					// pixels between groups

char lights[NUM_LIGHTS];					// 1 = On
int lights_last_bin[NUM_LIGHTS];			// keep track of which bin this light was assigned to
int lights_time_decay[NUM_LIGHTS];			// keep track of how long its been since light was triggered

#define TABLE_WIDTH     16
#define TABLE_HEIGHT    8
#define CELL_SIZE       25

struct pixel
{
    int r,g,b;
    int h,s,v;
} table[TABLE_WIDTH][TABLE_HEIGHT];

struct pixel2
{
    int r,g,b;
    int h,s,v;
} tmp_table[TABLE_WIDTH][TABLE_HEIGHT];

struct _pulses
{
    int x,y,radius;
    int r,g,b;
    int decay;
} pulses[NUM_LIGHTS];




double *fft_input;
fftw_complex *fft_out;
fftw_plan fft_plan;

int i = 0;

#define FIFO_FILE		"/tmp/mpd.fifo"
FILE *fifo_file;


int done = FALSE;		// main program loop

int SCREEN_WIDTH	=	1024;
int SCREEN_HEIGHT	=	768;
#define SCREEN_BPP		16
int videoFlags;

SDL_Surface *surface;	// surface to draw on

// camera position
GLfloat xpos = 25;
GLfloat ypos = 50;
GLfloat zpos = 0;

// how fast the camera moves
GLfloat movement = 5;

int mouse_x = 0;
int mouse_y = 0;

void HSVtoRGB( int h, int s, int v, int *r, int *g, int *b )
{
    int f;
    long p, q, t;

    if( s == 0 )
    {
        *r = *g = *b = v;
        return;
    }

    f = ((h%60)*255)/60;
    h /= 60;

    p = (v * (256 - s))/256;
    q = (v * ( 256 - (s * f)/256 ))/256;
    t = (v * ( 256 - (s * ( 256 - f ))/256))/256;

    switch( h ) {
        case 0:
            *r = v;
            *g = t;
            *b = p;
            break;
        case 1:
            *r = q;
            *g = v;
            *b = p;
            break;
        case 2:
            *r = p;
            *g = v;
            *b = t;
            break;
        case 3:
            *r = p;
            *g = q;
            *b = v;
            break;
        case 4:
            *r = t;
            *g = p;
            *b = v;
            break;
        default:
            *r = v;
            *g = p;
            *b = q;
            break;
    }
}

void init_pulses( void )
{
    for (i=0; i<NUM_LIGHTS; i++)
    {
        pulses[i].x = rand() % TABLE_WIDTH;
        pulses[i].y = rand() % TABLE_HEIGHT; 
        pulses[i].decay = 0;
    }

}

void clear_table( void )
{
    int x,y = 0;

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

void clear_tmp_table( void )
{
    int x,y = 0;

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
    int x,y;

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

void draw_table_bg()
{
    int x,y;

    for (x=0; x<TABLE_WIDTH; x++)
    {
        for (y=0; y<TABLE_HEIGHT; y++)
        {
			float r = (255*fft_bin_hist[x][y+(HIST_SIZE-TABLE_HEIGHT)])/fft_bin_hist_global_max;
			float b = (255*fft_bin_hist_std[x])/(fft_bin_hist_std_max);
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

void assign_cells()
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
                glVertex2f( off_x + x*CELL_SIZE		, off_y + y*CELL_SIZE );				// BL
                glVertex2f( off_x + x*CELL_SIZE		, off_y + (y+1)*CELL_SIZE );	// TL
                glVertex2f( off_x + (x+1)*CELL_SIZE	, off_y + (y+1)*CELL_SIZE ); 	// TR
                glVertex2f( off_x + (x+1)*CELL_SIZE	, off_y + y*CELL_SIZE ); // BR
            glEnd();

            // draw cell color
            glBegin(GL_QUADS);
                glColor3ub( table[x][y].r, table[x][y].g, table[x][y].b );
                glVertex2f( off_x + x*CELL_SIZE + 1		, off_y + y*CELL_SIZE + 1);				// BL
                glVertex2f( off_x + x*CELL_SIZE	+ 1	    , off_y + (y+1)*CELL_SIZE -1);	// TL
                glVertex2f( off_x + (x+1)*CELL_SIZE - 1	, off_y + (y+1)*CELL_SIZE -1); 	// TR
                glVertex2f( off_x + (x+1)*CELL_SIZE	- 1 , off_y + y*CELL_SIZE + 1); // BR
            glEnd();

        }
    }
    
}

int init_serial( void )
{
	serial_file = open(SERIAL_DEV, O_RDWR | O_NOCTTY | O_NDELAY);
	if (serial_file == -1)
	{
		printf("Could not open serial port\n");
		return 0;
	}

	fcntl(serial_file, F_SETFL, 0);

	struct termios options;

	tcgetattr(serial_file, &options);

	cfsetispeed(&options, SERIAL_BAUD);
	cfsetospeed(&options, SERIAL_BAUD);
	options.c_cflag |= (CLOCAL | CREAD);

	tcsetattr(serial_file, TCSANOW, &options);

	return TRUE;
}

int send_serial( void )
{
	unsigned char data = 0;

	if (lights[0]) data += 1;
	if (lights[2]) data += 2;
	if (lights[4]) data += 4;
	if (lights[6]) data += 8;

	if (lights[1]) data += 16;
	if (lights[3]) data += 32;
	if (lights[5]) data += 64;
	if (lights[7]) data += 128;

	int n = write(serial_file, &data, 1);
	if (n < 0)
	{
		printf("write() failed!\n");
		return FALSE;	
	}
	
	return TRUE;
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

// handle key press events
void service_keys( )
{
	Uint8 *keys = SDL_GetKeyState(0);

	if (keys[SDLK_ESCAPE])	done=TRUE;

	if (keys[SDLK_F1])	SDL_WM_ToggleFullScreen( surface );

	if (keys[SDLK_d])	xpos += (float)movement;
	if (keys[SDLK_a])	xpos -= (float)movement;
	if (keys[SDLK_w])	ypos -= (float)movement;
   	if (keys[SDLK_s])	ypos += (float)movement;

   	if (keys[SDLK_o])	{ MAG_TRIGGER += 0.01; printf("Mag: %f\tVar: %f\n", MAG_TRIGGER, VAR_TRIGGER); }
   	if (keys[SDLK_l])	{ MAG_TRIGGER -= 0.01; printf("Mag: %f\tVar: %f\n", MAG_TRIGGER, VAR_TRIGGER); }
   	if (keys[SDLK_i])	{ VAR_TRIGGER += 0.01; printf("Mag: %f\tVar: %f\n", MAG_TRIGGER, VAR_TRIGGER); }
   	if (keys[SDLK_k])	{ VAR_TRIGGER -= 0.01; printf("Mag: %f\tVar: %f\n", MAG_TRIGGER, VAR_TRIGGER); }
}

void detect_beats( void )
{
	for (int i = 0; i < FFT_NUM_BINS; i++)
	{
		// shift trigger history left
		for (int k=1; k < HIST_SIZE; k++)
		{
			fft_bin_triggered_hist[i][k-1] = fft_bin_triggered_hist[i][k];
		}

		// see if we detect a beat
		if (fft_bin[i]/fft_bin_hist_global_max > MAG_TRIGGER && fft_bin_hist_std[i]/fft_bin_hist_std_max > VAR_TRIGGER)
			fft_bin_triggered[i] = 1;
		else
			fft_bin_triggered[i] = 0;
		
		// if this bin is decreasing from last time it is no longer a beat
		//if (fft_bin_diff[i] <= 0)
		//	fft_bin_triggered[i] = 0;

		// add current trigger state to hist buffer
		fft_bin_triggered_hist[i][HIST_SIZE-1] = fft_bin_triggered[i];
	}
}

// draw the fft bins on a real vs imaginary plot
void draw_real_img_plot( float off_x, float off_y)
{
	glBegin(GL_POINTS);

		for (int i=0; i<FFT_NUM_BINS; i++)
		{

			if (fft_bin_triggered[i])
				glColor3ub( 255, 255, 255);
			else
				glColor3ub( 255, 0, 255);

			glVertex2f( off_x + fft_out[i][0]/5000, off_y + fft_out[i][1]/5000);
		}

	glEnd();
}

// draw the magnitude bar
void draw_mag( int i, float height, float off_x, float off_y )
{
	if (fft_bin_triggered[i])
		glColor3ub( 255, 255, 255);
	else
		glColor3ub( 255, 0, 0 );

	glBegin(GL_LINE_LOOP);

		glVertex2f( off_x + i*FFT_BIN_WIDTH		, off_y );			// BL
		glVertex2f( off_x + i*FFT_BIN_WIDTH		, off_y + height );	// TL
		glVertex2f( off_x + (i+1)*FFT_BIN_WIDTH	, off_y + height);	// TR
		glVertex2f( off_x + (i+1)*FFT_BIN_WIDTH	, off_y );

	glEnd();
}

// draw average line
void draw_mag_hist_avg( int i, float off_x, float off_y)
{
	glBegin(GL_LINES);
		glColor3ub( 0, 255, 0 );

		glVertex2f( off_x + i*FFT_BIN_WIDTH 	, off_y + fft_bin_hist_avg[i] );
		glVertex2f( off_x + (i+1)*FFT_BIN_WIDTH	, off_y + fft_bin_hist_avg[i] );
	glEnd();
}

// draw variance line
void draw_mag_hist_var( int i, float off_x, float off_y)
{
	glBegin(GL_LINES);
		glColor3ub( 255, 255, 0);

		glVertex2f( off_x + i*FFT_BIN_WIDTH 	, off_y + fft_bin_hist_std[i] );
		glVertex2f( off_x + (i+1)*FFT_BIN_WIDTH	, off_y + fft_bin_hist_std[i] );
	glEnd();
}

// draw magnitude history
void draw_mag_hist( int i, float off_x, float off_y)
{
	glBegin(GL_QUADS);

		for (int k = 0; k < HIST_SIZE; k++)
		{
			float r = (255*fft_bin_hist[i][k])/fft_bin_hist_global_max;
			float b = (255*fft_bin_hist_std[i])/(fft_bin_hist_std_max);
			float g = 0;

			// if this was a beat, color it white
			if (fft_bin_triggered_hist[i][k]) {r = 255; g = 255; b = 255;}

			glColor3ub( r, g, b );

			glVertex2f( off_x + (i+1)*FFT_BIN_WIDTH	, off_y + k*FFT_BIN_WIDTH ); 		// TL
			glVertex2f( off_x + (i+1)*FFT_BIN_WIDTH , off_y + (k+1)*FFT_BIN_WIDTH ); 	// TR
			glVertex2f( off_x + i*FFT_BIN_WIDTH		, off_y + (k+1)*FFT_BIN_WIDTH );	// BR
			glVertex2f( off_x + i*FFT_BIN_WIDTH		, off_y + k*FFT_BIN_WIDTH );		// BL 
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

	double s = 0;	// spacing

	for (int i = 0; i < NUM_LIGHTS; i++)
	{
		// i > 1 so we dont put space in front of the first group
		if (i > 1 && i%4==0) s += LIGHT_SPACING;

		if (lights_time_decay[i] > 0)
		{
			glBegin(GL_LINES);
				unsigned char c = 255 * lights_time_decay[i] / LIGHT_DECAY;
				glColor3ub( c, c, c );
				glVertex2f( x + s + i*LIGHT_SIZE+(LIGHT_SIZE/2)							, y );	// BL
				glVertex2f( xpos + lights_last_bin[i]*FFT_BIN_WIDTH+(FFT_BIN_WIDTH/2)	, ypos + HIST_SIZE*FFT_BIN_WIDTH);
			glEnd();
		}
	}
}

void draw_lights( void )
{
	double x = (SCREEN_WIDTH / 2) - ((NUM_LIGHTS * LIGHT_SIZE + (NUM_LIGHTS/4-1)*LIGHT_SPACING) / 2);
	double y = SCREEN_HEIGHT - 2*LIGHT_SIZE;

	double s = 0;	// spacing

	for (int i = 0; i < NUM_LIGHTS; i++)
	{

		// space out lights in groups of 4
		// i > 1 so we dont put space in front of the first group
		if (i > 1 && i%4==0) s += LIGHT_SPACING;

		// draw outline of lights
		glBegin(GL_LINE_LOOP);
			glColor3ub( 100, 100, 100 );
			glVertex2f( x + s + i*LIGHT_SIZE		, y );				// BL
			glVertex2f( x + s + i*LIGHT_SIZE		, y + LIGHT_SIZE );	// TL
			glVertex2f( x + s + (i+1)*LIGHT_SIZE	, y + LIGHT_SIZE); 	// TR
			glVertex2f( x + s + (i+1)*LIGHT_SIZE	, y );
		glEnd();

		// draw light if its on
		if (lights[i])
		{
			glBegin(GL_QUADS);
				glColor3ub( 25, 0, 255 );
				glVertex2f( x + s + i*LIGHT_SIZE + 5		, y + 5);				// BL
				glVertex2f( x + s + i*LIGHT_SIZE + 5		, y + LIGHT_SIZE - 5);	// TL
				glVertex2f( x + s + (i+1)*LIGHT_SIZE - 5	, y + LIGHT_SIZE - 5); 	// TR
				glVertex2f( x + s + (i+1)*LIGHT_SIZE - 5	, y + 5 );				// BR
			glEnd();

		}

	}

}


void assign_lights( void )
{
	int pulse_count = 0;
	int center_of_pulse = 0;

	// finds how many groups of pulses there are
	// marks the center of them
	for (int i=1; i<FFT_NUM_BINS; i++)
	{
		// not a pulse until proved otherwise		
		fft_bin_pulse[i] = 0;

		// if this one is triggered and the previous one isn't we found start of pulse
		if (fft_bin_triggered[i] && !fft_bin_triggered[i-1])
		{
			// got start
			pulse_count++;
			center_of_pulse = i;

		}
		// if it is not triggered but the last one is we found end of pulse
		else if (!fft_bin_triggered[i] && fft_bin_triggered[i-1])
		{
			// got end
			center_of_pulse = (i-center_of_pulse) / 2  + center_of_pulse;
			fft_bin_pulse[center_of_pulse] = 1;
		}

		// skip above logic, just count every one
		//fft_bin_pulse[i] = fft_bin_triggered[i];
	}

	//printf("pulse_count: %d\n", pulse_count);


	// go through groups of pulses and map them to lights
	// a light can only trigger if either:
	// 	1. we find a pulse that is same place as last time for this light
	//	2. the light decay is zero, meaning it has not
	//		had a pulse in a while so we should pulse it asap

	// loop through each light
	for (int i=0; i<NUM_LIGHTS; i++)
	{
		// flag to see if we found a pulse (new or repeating)	
		char found_pulse = 0;

		// loop through all the pulses
		for (int j=0; j<FFT_NUM_BINS; j++)
		{
			// we found a pulse that was in same place as last time
			// we found a pulse and this light is clear to trigger
			if ( (fft_bin_pulse[j] && abs(lights_last_bin[i] - j) < 1) ||
				 (fft_bin_pulse[j] && lights_time_decay[i] == 0) )
			{
                // pulse is in a different spot
                if (abs(lights_last_bin[i] - j) >= 1)
                {
                    pulses[i].x = (int)(((float)rand() * (float)(TABLE_WIDTH-2) / (RAND_MAX - 1.0)) + 1.0)+1;
                    pulses[i].y = (int)(((float)rand() * (float)(TABLE_HEIGHT-2) / (RAND_MAX - 1.0)) + 1.0)+1;
                    int color = (int)(((float)rand() * 360.0 / (RAND_MAX - 1.0)) + 1.0);
                    HSVtoRGB(color, 255, 255, &pulses[i].r, &pulses[i].g, &pulses[i].b);
                    printf("%d %d\n", pulses[i].x, pulses[i].y);
                }

                pulses[i].decay = LIGHT_DECAY;
				
                lights[i] = 1;
				lights_last_bin[i] = j;
				lights_time_decay[i] = LIGHT_DECAY;


				fft_bin_pulse[j] = 0;	// clear this pulse since we just handled it

				found_pulse = 1;

				break;
			}
		}

		// we did not find an repeating or new pulse
		// either there is no more pulses or the decay is still dying
		if (!found_pulse)
		{
			lights[i] = 0;
		}

		// decrement decay for this light
		lights_time_decay[i] -= 1;
		if (lights_time_decay[i] < 0) lights_time_decay[i] = 0;

        pulses[i].decay -= 1;
        if (pulses[i].decay < 0) 
        {
            pulses[i].decay = 0;
        }


		// when there is a heavy bass line we want to turn on as many lights as possible
		// also when there is a heavy bass line we will probably be clipping it.
		// we didn't find a pulse for this light
		// we clipped a bin
		// this light is almost free to trigger
		if (!found_pulse && clipped && lights_time_decay[i] < LIGHT_DECAY / 2)
			lights[i] = 1;
		
	}
}

int draw_gl( GLvoid )
{
    glClear( GL_COLOR_BUFFER_BIT );

	glMatrixMode( GL_MODELVIEW );
    glLoadIdentity( );

	glTranslatef( xpos, ypos, zpos);

	// draw each bins mag, hist, hist_avg, hist_var
	for (int i=0; i< FFT_NUM_BINS; i++)
	{
		draw_mag(i, fft_bin[i] , 0, HIST_SIZE*FFT_BIN_WIDTH);
		//draw_mag(i, fft_bin_diff[i] , 0, HIST_SIZE*FFT_BIN_WIDTH);	// draw diff
		//draw_mag_hist_avg(i, 0, HIST_SIZE*FFT_BIN_WIDTH);
		//draw_mag_hist_var(i, 0, HIST_SIZE*FFT_BIN_WIDTH);
		draw_mag_hist(i, 0, 0);
	}

/*
	// draw average var line
	glBegin(GL_LINES);
		glColor3ub( 255, 255, 0);
		glVertex2f( 0				, HIST_SIZE*FFT_BIN_WIDTH + fft_bin_hist_std_avg);
		glVertex2f( FFT_NUM_BINS*FFT_BIN_WIDTH	, HIST_SIZE*FFT_BIN_WIDTH + fft_bin_hist_std_avg );
	glEnd();

	// draw history average average line
	glBegin(GL_LINES);
		glColor3ub( 0, 255, 0);
		glVertex2f( 0 				, HIST_SIZE*FFT_BIN_WIDTH + fft_bin_hist_global_avg);
		glVertex2f( FFT_NUM_BINS*FFT_BIN_WIDTH	, HIST_SIZE*FFT_BIN_WIDTH + fft_bin_hist_global_avg );
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
		    glVertex2f(0,  							HIST_SIZE*FFT_BIN_WIDTH + clip_mag);
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

// takes the standard deviation of each bins history
// calculates the average of the std devs
// calculates the max std dev
void compute_std_dev( void )
{
	fft_bin_hist_std_max = 0;
	fft_bin_hist_std_avg = 0;

	for (i = 0; i < FFT_NUM_BINS; i++)
	{
		// compute the std deviation
		// sqrt(1/n * sum(x - a)^2)

		fft_bin_hist_std[i] = 0;

		for (int k = 0; k < HIST_SIZE; k++)
		{
			fft_bin_hist_std[i] += pow(fft_bin_hist[i][k] - fft_bin_hist_avg[i], 2);
		}

		fft_bin_hist_std[i] /= HIST_SIZE;

		fft_bin_hist_std[i] = sqrt(fft_bin_hist_std[i]);

		// sum each one for average
		fft_bin_hist_std_avg += fft_bin_hist_std[i];

		// see if we found a new maximum
		if (fft_bin_hist_std[i] > fft_bin_hist_std_max) fft_bin_hist_std_max = fft_bin_hist_std[i];

		fft_bin_hist_std_idv[i][HIST_SIZE-1] = fft_bin_hist_std[i];
	}

	// take average
	fft_bin_hist_std_avg /= FFT_NUM_BINS;
}

// TODO: memcpy
void copy_bins_to_old( void )
{
	for (i = 0; i < FFT_NUM_BINS; i++)
	{
		fft_bin_last[i] = fft_bin[i];
	}
}

// takes the magnitude of the fft output
// scale the magnitude down
// clip it if desired
void compute_magnitude( void )
{
	// we have not clipped a signal until proven otherwise
	clipped = 0;

	static double clip_mag_raw = 0;

	for (i = 0; i < FFT_NUM_BINS; i++)
	{
		// compute magnitude magnitude
		fft_bin[i] = sqrt( fft_out[i][0]*fft_out[i][0] + fft_out[i][1]*fft_out[i][1] );

		// scale it
		fft_bin[i] /= (MAG_SCALE);

		// find new clip
		if (USE_CLIP && fft_bin[i] > clip_mag_raw)
		{
			clip_mag_raw = fft_bin[i];	
		}
	
	}

	// check if we want to use a dynamic clip or not
	if (USE_CLIP_DYN)
		// we want to set the clip at some fraction of the actual
		clip_mag = (clip_mag_raw * 1 / 2);
	else
		clip_mag = CLIP_STATIC_MAG;

	// loop through all bins and see if they need to be clipped
	for (i = 0; i < FFT_NUM_BINS; i++)
	{
		if (USE_CLIP && fft_bin[i] > clip_mag)
		{
			fft_bin[i] = clip_mag;
			clipped = 1;
		}	
	}

	// if we haven't clipped a bin this time
	// we want to start lowering the clip magnitude
	if (! clipped) 
	{
		clip_mag_raw -= pow(clip_mag_decay/100, 3);	// decrease the clip by x^3 since it has slow 'acceleration'
		clip_mag_decay++;							// increment the index 
	}
	else
		clip_mag_decay = 0;							// we did clip a bin so reset the decay


	// calculate the average magnitude
	fft_bin_avg = 0;
	for (i = 0; i < FFT_NUM_BINS; i++)
	{
		fft_bin_avg += fft_bin[i];
	}
	fft_bin_avg /= FFT_NUM_BINS;


	// if a new song comes on we want to reset the clip
	// we can tell a new song by seeing if the avg magnitude is close to zero
	if (fft_bin_avg < 0.005)
	{
		clip_mag_raw = 0;
		clip_mag_decay = 0;
	}
}



// take the difference between these bins are the last
// TODO: use fft_bin_hist[i][HIST_SIZE-2]
//		 would only work if noting use delta for everything
void compute_delta_from_last( void )
{
	for (i = 0; i < FFT_NUM_BINS; i++)
	{
		// calculate difference between these bins and the last
		// only takes the ones that increased from last time
		if (fft_bin[i] < fft_bin_last[i])
			fft_bin_diff[i] = 0;
		else
			fft_bin_diff[i] = fft_bin[i] - fft_bin_last[i];
	}
}

// push the current bins into history
void add_bins_to_history( void )
{
	for (i = 0; i < FFT_NUM_BINS; i++)
	{
		// shift history buffer left
		for (int k = 1; k<HIST_SIZE; k++)
		{
			fft_bin_hist[i][k-1] = fft_bin_hist[i][k];
			fft_bin_hist_std_idv[i][k-1] = fft_bin_hist_std_idv[i][k];
		}

		// add current value to the history
		fft_bin_hist[i][HIST_SIZE-1] = fft_bin[i];
	}
}

// calculate avergae for each bin history
// calculate average of all bin history averages
// find the global maximum magnitude 
void compute_bin_hist( void )
{
	fft_bin_hist_global_max = 0;
	fft_bin_hist_global_avg = 0;

	for (i = 0; i < FFT_NUM_BINS; i++)
	{
		// reset bin hist avg
		fft_bin_hist_avg[i] = 0;

		for (int k = 1; k<HIST_SIZE; k++)
		{
			// sum all the values in this bin hist
			fft_bin_hist_avg[i] += fft_bin_hist[i][k];

			// sum all values for global average
			fft_bin_hist_global_avg += fft_bin_hist[i][k];

			// check to see if we found new global max
			if (fft_bin_hist[i][k] > fft_bin_hist_global_max) fft_bin_hist_global_max  = fft_bin_hist[i][k];
		}
		
		// average this bin
		fft_bin_hist_avg[i] /= HIST_SIZE;

	}

	// average global
	fft_bin_hist_global_avg /= (FFT_NUM_BINS * HIST_SIZE);
}

void get_samples_do_fft( void )
{
    // read in PCM 44100:16:1
    static int16_t buf[SAMPLE_SIZE];

    for (i = 0; i < SAMPLE_SIZE; i++) buf[i] = 0;

	int data = fread(buf, sizeof(int16_t), SAMPLE_SIZE, fifo_file);

    if (data != SAMPLE_SIZE)
    {
        printf("WRONG SAMPLE SIZE: %d!\n", data);
    }
	else
	{
		// cast the int16 array into a double array
		for (i = 0; i < SAMPLE_SIZE; i++) fft_input[i] = (double)buf[i];

		// execute the dft
		fftw_execute(fft_plan);

		copy_bins_to_old();

		compute_magnitude();

		compute_delta_from_last();

		add_bins_to_history();

		compute_bin_hist();	

		compute_std_dev();

	}

	//printf("%d\n", buf[SAMPLE_SIZE-100]);
	//printf("sizeof(int16_t): %ld\n", sizeof(int16_t));
	//printf("sizeof(buf): %ld\n", sizeof(buf));
	//printf("sizeof(double): %ld\n", sizeof(double));

}


int init_fft( void )
{
    fft_input = fftw_malloc(sizeof(double) * SAMPLE_SIZE);
    fft_out = (fftw_complex *)fftw_malloc(sizeof(fftw_complex) * FFT_SIZE);
    fft_plan = fftw_plan_dft_r2c_1d(SAMPLE_SIZE, fft_input, fft_out, FFTW_ESTIMATE);

    fifo_file = fopen(FIFO_FILE, "rb");
    if (!fifo_file)
    {
        printf("Cannot open file!\n");
        return 1;
    }

	// init light decay
	for (int i = 0; i<NUM_LIGHTS; i++)
	{
		lights_time_decay[i] = 0;
	}

	return 0;
}


void init_gl( GLvoid )
{
    glShadeModel( GL_SMOOTH );		 

    glClearColor( 0,0,0, 0.0f );	 

	gluOrtho2D(0.0, SCREEN_WIDTH, 0.0, SCREEN_HEIGHT);

	glDisable(GL_DEPTH_TEST);

	glEnable(GL_LINE_SMOOTH);
	glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);	
	glEnable(GL_BLEND);							
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

}

int init_sdl( GLvoid )
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

	return 0;
}

int main( int argc, char **argv )
{
    SDL_Event event;
  
	printf("init_sdl()\n");
 	if ( init_sdl() ) return 1;

	printf("init_fft()\n");
	if ( init_fft() ) return 1;

	printf("init_serial()\n");
	char has_serial = init_serial();

	printf("init_gl()\n");
	init_gl();

    init_pulses();
    clear_table();

    resize_window( SCREEN_WIDTH, SCREEN_HEIGHT );

    while ( !done )
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

		get_samples_do_fft();

		detect_beats();

		assign_lights();

        assign_cells();

		draw_gl();

		service_keys();

		if ( has_serial ) send_serial();

		usleep(5000);

	}


    return 0;
}

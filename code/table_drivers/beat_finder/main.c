// http://stackoverflow.com/questions/604453/analyze-audio-using-fast-fourier-transform
// http://www.gamedev.net/reference/programming/features/beatdetection/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h> 

#include <fftw3.h>

#include "main.h"
#include "draw.h"
#include "table.h"

unsigned char use_gui = FALSE;
unsigned char use_serial = TRUE;

int serial_file;

double  clip_mag		=		0;			// dynamic magnitude clip
double	clip_mag_decay	=		0;			// dynamio clip decreases at rate of some function, this indexes the function
char	clipped 		=	 	0;			// 1 = we clipped a bin this loop

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


char lights[NUM_LIGHTS];					// 1 = On
int lights_last_bin[NUM_LIGHTS];			// keep track of which bin this light was assigned to
int lights_time_decay[NUM_LIGHTS];			// keep track of how long its been since light was triggered

double *fft_input;
fftw_complex *fft_out;
fftw_plan fft_plan;

int i = 0;

FILE *fifo_file;

void hsv_to_rgb( int h, int s, int v, int *r, int *g, int *b )
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
                    hsv_to_rgb(color, 255, 255, &pulses[i].r, &pulses[i].g, &pulses[i].b);
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


int main( int argc, char **argv )
{
 
    if ( use_gui )
    {
        printf("init_sdl()\n");
        if ( init_sdl() ) return 1;

        printf("init_gl()\n");
        init_gl();
    }

	printf("init_fft()\n");
	if ( init_fft() ) return 1;

    if ( use_serial )
    {
        printf("init_serial()\n");
        if ( init_serial() ) use_serial = FALSE;
    }


    init_pulses();
    clear_table();

    while ( !done )
	{
		get_samples_do_fft();

		detect_beats();

		assign_lights();

        assign_cells();

		if ( use_gui )
        {
            if (handle_sdl_events()) return 1;
            draw_all();
        }

		if ( use_serial ) send_serial();

		usleep(5000);

	}


    return 0;
}

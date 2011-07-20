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
#include "serial.h"

unsigned char use_gui = TRUE;
unsigned char use_serial = TRUE;

double clip_mag = 0;        // dynamic magnitude clip
double clip_mag_decay = 0;   // dynamio clip decreases at rate of some function, this indexes the function
char clipped = 0;            // 1 = we clipped a bin this loop

// Mag: 0.170000 Var: 0.220000
// default trigger levels for detecting beats
double MAG_TRIGGER=    .58; //.36
double VAR_TRIGGER=    .54; //.36

struct bin fft_bin[FFT_NUM_BINS];

double fft_global_mag_avg;
double fft_global_mag_max;
double fft_global_hist_mag_avg;     // average of all the bin history averages
double fft_global_hist_mag_max;     // max value of global history
double fft_global_hist_std_avg;     // avg of all the std deviations
double fft_global_hist_std_max;     // max of all the std deviations

struct light lights[NUM_LIGHTS];

double *fft_input;
fftw_complex *fft_out;
fftw_plan fft_plan;

int i,j,k = 0;

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

void init_lights(void)
{
    for (i=0; i<NUM_LIGHTS; i++)
    {
        lights[i].state = 0;
        lights[i].decay = 0;
        lights[i].last_bin = -1;
    }
}

void detect_beats( void )
{
    for (i = 0; i < FFT_NUM_BINS; i++)
    {
        // shift trigger history down
        for (k=1; k < HIST_SIZE; k++)
        {
            fft_bin[i].trigger_hist[k-1] = fft_bin[i].trigger_hist[k];
        }

        // see if we detect a beat
        if (fft_bin[i].mag/fft_global_mag_max > MAG_TRIGGER && fft_bin[i].hist_std/fft_global_hist_std_max > VAR_TRIGGER)
            fft_bin[i].triggered = 1;
        else
            fft_bin[i].triggered = 0;

        // if this bin is decreasing from last time it is no longer a beat
        //if (fft_bin_diff[i] <= 0)
        //    fft_bin_triggered[i] = 0;

        // add current trigger state to hist buffer
        fft_bin[i].trigger_hist[HIST_SIZE-1] = fft_bin[i].triggered;
    }
}


void assign_lights( void )
{
    int pulse_count = 0;
    int center_of_pulse = 0;

    // finds how many groups of pulses there are
    // marks the center of them
    for (i=1; i<FFT_NUM_BINS; i++)
    {
        // not a pulse until proved otherwise        
        fft_bin[i].is_pulse = 0;

        // if this one is triggered and the previous one isn't we found start of group
        if (fft_bin[i].triggered && !fft_bin[i-1].triggered)
        {
            pulse_count++;
            center_of_pulse = i;

        }
        // if it is not triggered but the last one is we found end of group
        else if (!fft_bin[i].triggered && fft_bin[i-1].triggered)
        {
            center_of_pulse = (i-center_of_pulse) / 2  + center_of_pulse;
            fft_bin[center_of_pulse].is_pulse = 1;
        }

        // skip grouping logic, just count every one
        //fft_bin_pulse[i] = fft_bin_triggered[i];
    }

    //printf("pulse_count: %d\n", pulse_count);


    // go through groups of pulses and map them to lights
    // a light can only trigger if either:
    //     1. we find a pulse that is same place as last time for this light
    //     2. the light decay is zero, meaning it has not
    //        had a pulse in a while so we should pulse it asap

    // assume were not going to find any pulses
    for (i=0; i<NUM_LIGHTS; i++) { lights[i].found_pulse = 0; }

    // first find pulses that aleady have a light assigned
    for (i=0; i<NUM_LIGHTS; i++)
    {
        for (j=0; j<FFT_NUM_BINS; j++)
        {
            // check if we found a pulse that is in the same spot
            if (fft_bin[j].is_pulse && lights[i].last_bin ==  j)
            {
                // we found a pulse for this light
                lights[i].found_pulse = 1;

                // reset the decay
                table_pulses[i].decay = LIGHT_DECAY;
                lights[i].decay = LIGHT_DECAY;

                // turn light on
                lights[i].state = 1;
               
                // clear this pulse since we just handled it
                fft_bin[j].is_pulse = 0;

                // stop looking for pulses, go to next light
                break;
            }
        }
    }


    // take left over pulses and assign them to empty lights
    for (i=0; i<NUM_LIGHTS; i++)
    {
        // loop through all the pulses
        for (j=0; j<FFT_NUM_BINS; j++)
        {
            // check if we found an empty light 
            if (fft_bin[j].is_pulse && lights[i].decay == 0) 
            {
                // we found a pulse for this light
                lights[i].found_pulse = 1;

                // calculate new position and color
                table_pulses[i].x = (int)(((float)rand() * (float)(TABLE_WIDTH-2) / (RAND_MAX - 1.0)) + 1.0)+1;
                table_pulses[i].y = (int)(((float)rand() * (float)(TABLE_HEIGHT-2) / (RAND_MAX - 1.0)) + 1.0)+1;
                int color = (int)(((float)rand() * 360.0 / (RAND_MAX - 1.0)) + 1.0);
                hsv_to_rgb(color, 255, 255, &table_pulses[i].r, &table_pulses[i].g, &table_pulses[i].b);

                // reset the decay
                table_pulses[i].decay = LIGHT_DECAY;
                lights[i].decay = LIGHT_DECAY;

                // turn light on
                lights[i].state = 1;

                // save which bin this was
                lights[i].last_bin = j;

                // clear this pulse since we just handled it
                fft_bin[j].is_pulse = 0;

                // stop looking for pulses, go to next light
                break;
            }
        }
    }

    // find all the lights that did not get assigned a pulse
    for (i=0; i<NUM_LIGHTS; i++)
    {
        // we this light has a pulse go to next one
        if (lights[i].found_pulse) continue;

        // turn the light off
        lights[i].state = 0;

        // decrement the decay for this light
        if (lights[i].decay == 0)
            // once the decay dies this light no long belongs to a bin
            lights[i].last_bin = -1;
        else
            lights[i].decay -= 1;

        // decrement the decay for this pulse
        table_pulses[i].decay -= 1;
        if (table_pulses[i].decay < 0) table_pulses[i].decay = 0;

        // when there is a heavy bass line we want to turn on as many lights as possible
        // also when there is a heavy bass line we will probably be clipping it.
        // we didn't find a pulse for this light
        // we clipped a bin
        // this light is almost free to trigger
        if (clipped && lights[i].decay < LIGHT_DECAY / 2)
            lights[i].state = 1;
    }
}


// takes the standard deviation of each bins history
// calculates the global average of the std devs
// calculates the global max std dev
void compute_std_dev( void )
{
    fft_global_hist_std_max = 0;
    fft_global_hist_std_avg = 0;

    for (i = 0; i < FFT_NUM_BINS; i++)
    {
        // compute the std deviation
        // sqrt(1/n * sum(x - a)^2)

        fft_bin[i].hist_std = 0;

        for (k = 0; k < HIST_SIZE; k++)
        {
            fft_bin[i].hist_std += pow(fft_bin[i].hist[k] - fft_bin[i].hist_avg, 2);
        }

        fft_bin[i].hist_std /= HIST_SIZE;

        fft_bin[i].hist_std = sqrt(fft_bin[i].hist_std);

        // sum each one for average
        fft_global_hist_std_avg += fft_bin[i].hist_std;

        // see if we found a new maximum
        if (fft_bin[i].hist_std > fft_global_hist_std_max) fft_global_hist_std_max = fft_bin[i].hist_std;

    }

    // take average
    fft_global_hist_std_avg /= FFT_NUM_BINS;
}

// TODO: memcpy
void copy_bins_to_old( void )
{
    for (i = 0; i < FFT_NUM_BINS; i++)
    {
        fft_bin[i].last_mag = fft_bin[i].mag;
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
        fft_bin[i].mag = sqrt( fft_out[i][0]*fft_out[i][0] + fft_out[i][1]*fft_out[i][1] );

        // scale it
        fft_bin[i].mag /= (MAG_SCALE);

        // find new clip
        if (USE_CLIP && fft_bin[i].mag > clip_mag_raw)
        {
            clip_mag_raw = fft_bin[i].mag;    
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
        if (USE_CLIP && fft_bin[i].mag > clip_mag)
        {
            fft_bin[i].mag = clip_mag;
            clipped = 1;
        }    
    }

    // if we haven't clipped a bin this time
    // we want to start lowering the clip magnitude
    if (! clipped) 
    {
        clip_mag_raw -= pow(clip_mag_decay/100, 3);    // decrease the clip by x^3 since it has slow 'acceleration'
        clip_mag_decay++;                            // increment the index 
    }
    else
        clip_mag_decay = 0;                            // we did clip a bin so reset the decay


    // calculate the average magnitude
    fft_global_mag_avg = 0;
    for (i = 0; i < FFT_NUM_BINS; i++)
    {
        fft_global_mag_avg += fft_bin[i].mag;
    }
    fft_global_mag_avg /= FFT_NUM_BINS;


    // if a new song comes on we want to reset the clip
    // we can tell a new song by seeing if the avg magnitude is close to zero
    if (fft_global_mag_avg < 0.005)
    {
        clip_mag_raw = 0;
        clip_mag_decay = 0;
    }
}



// take the difference between these bins are the last
// TODO: use fft_bin_hist[i][HIST_SIZE-2]
//     would only work if noting use delta for everything
void compute_delta_from_last( void )
{
    for (i = 0; i < FFT_NUM_BINS; i++)
    {
        // calculate difference between these bins and the last
        // only takes the ones that increased from last time
        if (fft_bin[i].mag < fft_bin[i].last_mag)
            fft_bin[i].diff = 0;
        else
            fft_bin[i].diff = fft_bin[i].mag - fft_bin[i].last_mag;
    }
}

// push the current bins into history
void add_bins_to_history( void )
{
    for (i = 0; i < FFT_NUM_BINS; i++)
    {
        // shift history buffer down
        for (k = 1; k<HIST_SIZE; k++)
        {
            fft_bin[i].hist[k-1] = fft_bin[i].hist[k];
        }

        // add current value to the history
        fft_bin[i].hist[HIST_SIZE-1] = fft_bin[i].mag;
    }
}

// calculate average for each bin history
// calculate average of all bin history averages
// find the global maximum magnitude 
void compute_bin_hist( void )
{
    fft_global_hist_mag_max = 0;
    fft_global_hist_mag_avg = 0;

    for (i = 0; i < FFT_NUM_BINS; i++)
    {
        // reset bin hist avg
        fft_bin[i].hist_avg = 0;

        for (k = 1; k<HIST_SIZE; k++)
        {
            // sum all the values in this bin hist
            fft_bin[i].hist_avg += fft_bin[i].hist[k];

            // sum all values for global average
            fft_global_hist_mag_avg += fft_bin[i].hist[k];

            // check to see if we found new global max
            if (fft_bin[i].hist[k] > fft_global_hist_mag_max)  fft_global_hist_mag_max = fft_bin[i].hist[k];
        }

        // average this bin
        fft_bin[i].hist_avg /= HIST_SIZE;
    }

    // average global
    fft_global_hist_mag_avg /= (FFT_NUM_BINS * HIST_SIZE);
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
    for (i = 0; i<NUM_LIGHTS; i++)
    {
        lights[i].decay = 0;
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

    init_lights();
    init_table();
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

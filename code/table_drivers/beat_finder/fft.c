#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <fftw3.h>
#include <alsa/asoundlib.h>

#include "main.h"
#include "fft.h"

FILE *fifo_file;

double *fft_input;
fftw_complex *fft_out;
fftw_plan fft_plan;

snd_pcm_t *handle;
snd_pcm_hw_params_t *params;
snd_pcm_uframes_t frames = 1024;

int rc;
int size;
unsigned int rate = 44100;
int dir = 0;
unsigned char *buffer; // 2 bytes / sample, 2 channels

uint16_t tmp_buffer[940];

int init_fft(void)
{
    fft_input = fftw_malloc(sizeof(double) * SAMPLE_SIZE);
    fft_out = (fftw_complex *)fftw_malloc(sizeof(fftw_complex) * FFT_SIZE);
    fft_plan = fftw_plan_dft_r2c_1d(SAMPLE_SIZE, fft_input, fft_out, FFTW_ESTIMATE);

    fifo_file = fopen(FIFO_FILE, "rb");
    if (!fifo_file)
        return 1;

    return 0;
}

int init_alsa(void)
{
    // open PCM device for recording
    rc = snd_pcm_open(&handle, "default", SND_PCM_STREAM_CAPTURE, 0);

    if (rc < 0)
    {
        printf("Unable to open pcm device: %s\n", snd_strerror(rc));
        return 1;
    }

    // allocate a hardware parameter object
    snd_pcm_hw_params_alloca(&params);

    if (rc < 0)
    {
        printf("Unable to configure this PCM device\n");
        return 1;
    }

    // fill it with default values
    snd_pcm_hw_params_any(handle, params);

    //
    // set the desired hardware parameters
    //

    // interleaved mode
    rc = snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (rc < 0)
    {
        printf("Unable to set interleaved mode\n");
        return 1;
    }

    // signed 16-bit little-endian format
    rc = snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_U16_BE);
    if (rc < 0)
    {
        printf("Unable to set format\n");
        return 1;
    }

    // two channel stereo
    rc = snd_pcm_hw_params_set_channels(handle, params, 2);
    if (rc < 0)
    {
        printf("Unable to set to two channel stereo\n");
        return 1;
    }

    // 44100 bits/second sample rate
    unsigned int exact_rate = rate;
    rc = snd_pcm_hw_params_set_rate_near(handle, params, &exact_rate, 0);
    if (rc < 0)
    {
        printf("Error setting rate\n");
        return 1;
    }

    if (rate != exact_rate)
    {
        printf("%d Hz not supported. Using %d Hz instead", rate, exact_rate);
    }

    // set period size to 32 frames
    rc = snd_pcm_hw_params_set_period_size_near(handle, params, &frames, &dir);
    if (rc < 0)
    {
        printf("Error setting period size\n");
        return 1;
    }

    // write the parameters to the driver
    rc = snd_pcm_hw_params(handle, params);
    if (rc < 0)
    {
        printf("Unable to set hw parameters: %s\n", snd_strerror(rc));
        return 1;
    }

    size = frames * 4; // 2 bytes / sample, 2 channels
    buffer = (unsigned char *) malloc(size);

    printf("Frames: %d\n", (int)frames);
    printf("Allocating buffer of size: %d\n", size);

    snd_pcm_hw_params_get_period_time(params, &rate, &dir);

    return 0;

}

void get_alsa(void)
{
    rc = snd_pcm_readi(handle, buffer, frames);

    if (rc == -EPIPE)
    {
        printf("Overrun occurred\n");
        snd_pcm_prepare(handle);
    }
    else if (rc < 0)
    {
        printf("Error from read: %s\n", snd_strerror(rc));
    }
    else if (rc != (int)frames)
    {
        printf("Short read. Only read %d frames, expected %d\n", rc, (int)frames);
    }
    else if (rc == (int)frames)
    {
        //printf("GREAT! Found %d frames\n", rc);

        int frame = 0;
        for (frame=0; frame<rc; frame++)
        {
            int left = (buffer[frame*4+0] << 8) | buffer[frame*4+1];
            int right = (buffer[frame*4+2] << 8) | buffer[frame*4+3];

            tmp_buffer[frame] = (left + right) / 2;
            //tmp_buffer[frame] = (buffer[frame*4+0] << 8) | (buffer[frame*4+1]);
            //tmp_buffer[frame] = (buffer[frame*4+2] << 8) | (buffer[frame*4+3]);
            //tmp_buffer[frame] /= 2.0;

            //printf("%x %x = %u | %x\n", buffer[frame*4+0], buffer[frame*4+1], tmp_buffer[frame], tmp_buffer[frame]);
            //printf("%x %x %d | %x %x %d\n", buffer[frame*4+0], buffer[frame*4+1], left, buffer[frame*4+2], buffer[frame*4+3], right);
            
        }
    }

}

void get_samples_do_fft(void)
{

    get_alsa();

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
}

// TODO: memcpy
void copy_bins_to_old(void)
{
    for (i = 0; i < FFT_NUM_BINS; i++)
    {
        fft_bin[i].last_mag = fft_bin[i].mag;
    }
}

// takes the magnitude of the fft output
// scale the magnitude down
// clip it if desired
void compute_magnitude(void)
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
void compute_delta_from_last(void)
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
void add_bins_to_history (void)
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
void compute_bin_hist(void)
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

// takes the standard deviation of each bins history
// calculates the global average of the std devs
// calculates the global max std dev
void compute_std_dev(void)
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

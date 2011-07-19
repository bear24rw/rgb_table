#ifndef MAIN_H
#define MAIN_H

#include <fftw3.h>

#define TRUE  			1
#define FALSE 			0

#define SERIAL_DEV		"/dev/ttyUSB0"
#define SERIAL_BAUD		B2400


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


#define MAG_SCALE	10000


#define NUM_LIGHTS		(3*4)				// how many lights to display
#define LIGHT_DECAY		(HIST_SIZE*2)		// cycles until light is clear to trigger again
#define LIGHT_SIZE		50					// pixel size
#define LIGHT_SPACING	20					// pixels between groups


#define TABLE_WIDTH     16
#define TABLE_HEIGHT    8
#define CELL_SIZE       25


#define FIFO_FILE		"/tmp/mpd.fifo"

extern double clip_mag;
extern double clip_mag_decay;
extern char	clipped;		

extern double MAG_TRIGGER; 
extern double VAR_TRIGGER; 

extern double fft_bin[FFT_NUM_BINS];
extern double fft_bin_avg;
extern double fft_bin_last[FFT_NUM_BINS];
extern double fft_bin_diff[FFT_NUM_BINS];

extern double fft_bin_hist[FFT_NUM_BINS][HIST_SIZE];

extern double fft_bin_hist_avg[FFT_NUM_BINS];
extern double fft_bin_hist_global_avg;		
extern double fft_bin_hist_global_max;	

extern double fft_bin_hist_std_idv[FFT_NUM_BINS][HIST_SIZE];
extern double fft_bin_hist_std[FFT_NUM_BINS];
extern double fft_bin_hist_std_avg;	
extern double fft_bin_hist_std_max;

extern char fft_bin_triggered[FFT_NUM_BINS];
extern char fft_bin_triggered_hist[FFT_NUM_BINS][HIST_SIZE];
extern char fft_bin_pulse[FFT_NUM_BINS];

extern char lights[NUM_LIGHTS];				
extern int lights_last_bin[NUM_LIGHTS];	
extern int lights_time_decay[NUM_LIGHTS];

extern fftw_complex *fft_out;
extern double *fft_input;

#endif


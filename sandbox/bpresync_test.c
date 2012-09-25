// 
// bpresync_test.c
//
// This example demonstrates the binary pre-demodulator synchronizer. A random
// binary sequence is generated, modulated with BPSK, and then interpolated.
// The resulting sequence is used to generate a bpresync object which in turn
// is used to detect a signal in the presence of carrier frequency and timing
// offsets and additive white Gauss noise.
//

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <math.h>
#include <time.h>
#include "liquid.h"

#define OUTPUT_FILENAME "bpresync_test.m"

// print usage/help message
void usage()
{
    printf("bpresync_test -- test binary pre-demodulation synchronization\n");
    printf("  h     : print usage/help\n");
    printf("  k     : samples/symbol, default: 2\n");
    printf("  n     : number of data symbols, default: 64\n");
    printf("  F     : carrier frequency offset, default: 0.02\n");
    printf("  S     : SNR [dB], default: 20\n");
    printf("  t     : number of trials, default: 40\n");
}

void bpresync_test(bpresync_cccf   _q,
                   float complex * _x,
                   unsigned int    _n,
                   float           _SNRdB,
                   float           _dphi_max,
                   float *         _rxy_max,
                   float *         _dphi_err,
                   float *         _delay_err,
                   unsigned int    _num_trials);

int main(int argc, char*argv[])
{
    srand(time(NULL));

    // options
    unsigned int k=2;                   // filter samples/symbol
    unsigned int num_sync_symbols = 64; // number of synchronization symbols
    float SNRdB = 20.0f;                // signal-to-noise ratio [dB]
    float dphi_max = 0.02f;             // maximum carrier frequency offset
    unsigned int num_trials = 40;

    int dopt;
    while ((dopt = getopt(argc,argv,"hk:n:F:S:t:")) != EOF) {
        switch (dopt) {
        case 'h': usage();                          return 0;
        case 'k': k = atoi(optarg);                 break;
        case 'n': num_sync_symbols = atoi(optarg);  break;
        case 'F': dphi_max = atof(optarg);          break;
        case 'S': SNRdB = atof(optarg);             break;
        case 't': num_trials = atoi(optarg);        break;
        default:
            exit(1);
        }
    }

    unsigned int i;

    // arrays
    float complex seq[k*num_sync_symbols];  // synchronization pattern (samples)
    float rxy_max[num_trials];
    float dphi_err[num_trials];
    float delay_err[num_trials];

    // generate synchronization pattern (BPSK) and interpolate
    unsigned int n=0;
    for (i=0; i<num_sync_symbols; i++) {
        float sym = rand() % 2 ? -1.0f : 1.0f;
        
        unsigned int j;
        for (j=0; j<k; j++)
            seq[n++] = sym;
    }

    // create cross-correlator
    bpresync_cccf sync = bpresync_cccf_create(seq, k*num_sync_symbols, 0.05f, 11);
    bpresync_cccf_print(sync);

    // run trials
    bpresync_test(sync, seq, k*num_sync_symbols,
                  SNRdB, dphi_max,
                  rxy_max, dphi_err, delay_err,
                  num_trials);

    // destroy objects
    bpresync_cccf_destroy(sync);
    
    // 
    // export results
    //
    FILE * fid = fopen(OUTPUT_FILENAME,"w");
    fprintf(fid,"%% %s : auto-generated file\n", OUTPUT_FILENAME);
    fprintf(fid,"clear all\n");
    fprintf(fid,"close all\n");
    fprintf(fid,"num_trials = %u;\n", num_trials);
    fprintf(fid,"k          = %u;\n", k);

    fprintf(fid,"rxy_max   = zeros(1,num_trials);\n");
    fprintf(fid,"dphi_err  = zeros(1,num_trials);\n");
    fprintf(fid,"delay_err = zeros(1,num_trials);\n");
    for (i=0; i<num_trials; i++) {
        fprintf(fid,"rxy_max(%4u)   = %12.4e;\n", i+1, rxy_max[i]);
        fprintf(fid,"dphi_err(%4u)  = %12.4e;\n", i+1, dphi_err[i]);
        fprintf(fid,"delay_err(%4u) = %12.4e;\n", i+1, delay_err[i]);
    }

#if 0
    fprintf(fid,"figure;\n");
    fprintf(fid,"subplot(3,1,1);\n");
    fprintf(fid,"  hist(rxy_max, 25);\n");
    fprintf(fid,"subplot(3,1,2);\n");
    fprintf(fid,"  hist(dphi_err, 25);\n");
#endif

    fclose(fid);
    printf("results written to '%s'\n", OUTPUT_FILENAME);

    return 0;
}

void bpresync_test(bpresync_cccf   _q,
                   float complex * _x,
                   unsigned int    _n,
                   float           _SNRdB,
                   float           _dphi_max,
                   float *         _rxy_max,
                   float *         _dphi_err,
                   float *         _delay_err,
                   unsigned int    _num_trials)
{
    unsigned int max_delay = 64;
    float gamma = powf(10.0f, _SNRdB/20.0f);
    float nstd  = 1.0f;

    // Farrow filter (for facilitating delay)
    unsigned int h_len = 49;
    unsigned int order = 4;
    float        fc    = 0.45f;
    float        As    = 60.0f;
    firfarrow_crcf fdelay = firfarrow_crcf_create(h_len, order, fc, As);

    unsigned int num_samples = _n + max_delay + (h_len-1)/2;
    float complex y[num_samples];

    unsigned int t;
    for (t=0; t<_num_trials; t++) {
        unsigned int delay = rand() % max_delay;    // sample delay
        float        dt    = randf() - 0.5f;        // fractional sample delay
        float dphi = (2.0f*randf() - 1.0f) * _dphi_max; // carrier frequency offset
        float phi   = 2*M_PI*randf();                   // carrier phase offset

        // reset binary pre-demod synchronizer
        bpresync_cccf_reset(_q);

        // reset farrow filter
        firfarrow_crcf_clear(fdelay);
        firfarrow_crcf_set_delay(fdelay, dt);

        unsigned int i;
        unsigned int n=0;

        // generate signal: delay
        for (i=0; i<delay; i++) {
            firfarrow_crcf_push(fdelay, 0.0f);
            firfarrow_crcf_execute(fdelay, &y[n++]);
        }

        // generate signal: input sequence
        for (i=0; i<_n; i++) {
            firfarrow_crcf_push(fdelay, _x[i]);
            firfarrow_crcf_execute(fdelay, &y[n++]);
        }

        // generate signal: flush filter
        while (n < num_samples) {
            firfarrow_crcf_push(fdelay, 0.0f);
            firfarrow_crcf_execute(fdelay, &y[n++]);
        }

        // add channel gain, carrier offset, noise
        for (i=0; i<num_samples; i++) {
            y[i] *= gamma;
            y[i] *= cexpf(_Complex_I*(phi + i*dphi));
            y[i] += nstd*( randnf() + randnf()*_Complex_I )*M_SQRT1_2;
        }

        // push through synchronizer
        _rxy_max[t]   = 0.0f;
        _dphi_err[t]  = 0.0f;
        _delay_err[t] = 0.0f;
        for (i=0; i<num_samples; i++) {
            // push through correlator
            float complex rxy;
            float         dphi_est;
            bpresync_cccf_correlate(_q, y[i], &rxy, &dphi_est);

            // retain maximum
            if ( cabsf(rxy) > _rxy_max[t] ) {
                _rxy_max[t]   = cabsf(rxy);
                _dphi_err[t]  = dphi_est - dphi;
                _delay_err[t] = 0.0f;
            }
        }

        // print results
        printf("  %3u   :   rxy_max = %12.8f\n", t, _rxy_max[t]);
    }

    // destroy Farrow filter
    firfarrow_crcf_destroy(fdelay);
}


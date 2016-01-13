/* aec.h
 *
 * Copyright (C) DFS Deutsche Flugsicherung (2004, 2005).
 * All Rights Reserved.
 * Author: Andre Adrian
 *
 * Acoustic Echo Cancellation Leaky NLMS-pw algorithm
 *
 * Version 0.3 filter created with www.dsptutor.freeuk.com
 * Version 0.3.1 Allow change of stability parameter delta
 * Version 0.4 Leaky Normalized LMS - pre whitening algorithm
 */

#ifndef _AEC_H                  /* include only once */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <pulse/gccmacro.h>
#include <pulse/xmalloc.h>

#include <pulsecore/macro.h>

#define WIDEB 2

// use double if your CPU does software-emulation of float
#define REAL float

/* dB Values */
#define M0dB 1.0f
#define M3dB 0.71f
#define M6dB 0.50f
#define M9dB 0.35f
#define M12dB 0.25f
#define M18dB 0.125f
#define M24dB 0.063f

/* dB values for 16bit PCM */
/* MxdB_PCM = 32767 * 10 ^(x / 20) */
#define M10dB_PCM 10362.0f
#define M20dB_PCM 3277.0f
#define M25dB_PCM 1843.0f
#define M30dB_PCM 1026.0f
#define M35dB_PCM 583.0f
#define M40dB_PCM 328.0f
#define M45dB_PCM 184.0f
#define M50dB_PCM 104.0f
#define M55dB_PCM 58.0f
#define M60dB_PCM 33.0f
#define M65dB_PCM 18.0f
#define M70dB_PCM 10.0f
#define M75dB_PCM 6.0f
#define M80dB_PCM 3.0f
#define M85dB_PCM 2.0f
#define M90dB_PCM 1.0f

#define MAXPCM 32767.0f

/* Design constants (Change to fine tune the algorithms */

/* The following values are for hardware AEC and studio quality
 * microphone */

/* NLMS filter length in taps (samples). A longer filter length gives
 * better Echo Cancellation, but maybe slower convergence speed and
 * needs more CPU power (Order of NLMS is linear) */
#define NLMS_LEN  (100*WIDEB*8)

/* Vector w visualization length in taps (samples).
 * Must match argv value for wdisplay.tcl */
#define DUMP_LEN  (40*WIDEB*8)

/* minimum energy in xf. Range: M70dB_PCM to M50dB_PCM. Should be equal
 * to microphone ambient Noise level */
#define NoiseFloor M55dB_PCM

/* Leaky hangover in taps.
 */
#define Thold (60 * WIDEB * 8)

// Adrian soft decision DTD
// left point. X is ratio, Y is stepsize
#define STEPX1 1.0
#define STEPY1 1.0
// right point. STEPX2=2.0 is good double talk, 3.0 is good single talk.
#define STEPX2 2.5
#define STEPY2 0
#define ALPHAFAST (1.0f / 100.0f)
#define ALPHASLOW (1.0f / 20000.0f)



/* Ageing multiplier for LMS memory vector w */
#define Leaky 0.9999f

/* Double Talk Detector Speaker/Microphone Threshold. Range <=1
 * Large value (M0dB) is good for Single-Talk Echo cancellation,
 * small value (M12dB) is good for Doulbe-Talk AEC */
#define GeigelThreshold M6dB

/* for Non Linear Processor. Range >0 to 1. Large value (M0dB) is good
 * for Double-Talk, small value (M12dB) is good for Single-Talk */
#define NLPAttenuation M12dB

/* Below this line there are no more design constants */

typedef struct IIR_HP IIR_HP;

/* Exponential Smoothing or IIR Infinite Impulse Response Filter */
struct IIR_HP {
  REAL x;
};

static  IIR_HP* IIR_HP_init(void) {
    IIR_HP *i = pa_xnew(IIR_HP, 1);
    i->x = 0.0f;
    return i;
  }

static  REAL IIR_HP_highpass(IIR_HP *i, REAL in) {
    const REAL a0 = 0.01f;      /* controls Transfer Frequency */
    /* Highpass = Signal - Lowpass. Lowpass = Exponential Smoothing */
    i->x += a0 * (in - i->x);
    return in - i->x;
  }

typedef struct FIR_HP_300Hz FIR_HP_300Hz;

#if WIDEB==1
/* 17 taps FIR Finite Impulse Response filter
 * Coefficients calculated with
 * www.dsptutor.freeuk.com/KaiserFilterDesign/KaiserFilterDesign.html
 */
class FIR_HP_300Hz {
  REAL z[18];

public:
   FIR_HP_300Hz() {
    memset(this, 0, sizeof(FIR_HP_300Hz));
  }

  REAL highpass(REAL in) {
    const REAL a[18] = {
    // Kaiser Window FIR Filter, Filter type: High pass
    // Passband: 300.0 - 4000.0 Hz, Order: 16
    // Transition band: 75.0 Hz, Stopband attenuation: 10.0 dB
    -0.034870606, -0.039650206, -0.044063766, -0.04800318,
    -0.051370874, -0.054082647, -0.056070227, -0.057283327,
    0.8214126, -0.057283327, -0.056070227, -0.054082647,
    -0.051370874, -0.04800318, -0.044063766, -0.039650206,
    -0.034870606, 0.0
    };
    memmove(z + 1, z, 17 * sizeof(REAL));
    z[0] = in;
    REAL sum0 = 0.0, sum1 = 0.0;
    int j;

    for (j = 0; j < 18; j += 2) {
      // optimize: partial loop unrolling
      sum0 += a[j] * z[j];
      sum1 += a[j + 1] * z[j + 1];
    }
    return sum0 + sum1;
  }
};

#else

/* 35 taps FIR Finite Impulse Response filter
 * Passband 150Hz to 4kHz for 8kHz sample rate, 300Hz to 8kHz for 16kHz
 * sample rate.
 * Coefficients calculated with
 * www.dsptutor.freeuk.com/KaiserFilterDesign/KaiserFilterDesign.html
 */
struct FIR_HP_300Hz {
  REAL z[36];
};

static  FIR_HP_300Hz* FIR_HP_300Hz_init(void) {
    FIR_HP_300Hz *ret = pa_xnew(FIR_HP_300Hz, 1);
    memset(ret, 0, sizeof(FIR_HP_300Hz));
    return ret;
  }

static  REAL FIR_HP_300Hz_highpass(FIR_HP_300Hz *f, REAL in) {
    REAL sum0 = 0.0, sum1 = 0.0;
    int j;
    const REAL a[36] = {
      // Kaiser Window FIR Filter, Filter type: High pass
      // Passband: 150.0 - 4000.0 Hz, Order: 34
      // Transition band: 34.0 Hz, Stopband attenuation: 10.0 dB
      -0.016165324, -0.017454365, -0.01871232, -0.019931411,
      -0.021104068, -0.022222936, -0.02328091, -0.024271343,
      -0.025187887, -0.02602462, -0.026776174, -0.027437767,
      -0.028004972, -0.028474221, -0.028842418, -0.029107114,
      -0.02926664, 0.8524841, -0.02926664, -0.029107114,
      -0.028842418, -0.028474221, -0.028004972, -0.027437767,
      -0.026776174, -0.02602462, -0.025187887, -0.024271343,
      -0.02328091, -0.022222936, -0.021104068, -0.019931411,
      -0.01871232, -0.017454365, -0.016165324, 0.0
    };
    memmove(f->z + 1, f->z, 35 * sizeof(REAL));
    f->z[0] = in;

    for (j = 0; j < 36; j += 2) {
      // optimize: partial loop unrolling
      sum0 += a[j] * f->z[j];
      sum1 += a[j + 1] * f->z[j + 1];
    }
    return sum0 + sum1;
  }
#endif

typedef struct IIR1 IIR1;

/* Recursive single pole IIR Infinite Impulse response High-pass filter
 *
 * Reference: The Scientist and Engineer's Guide to Digital Processing
 *
 * 	output[N] = A0 * input[N] + A1 * input[N-1] + B1 * output[N-1]
 *
 *      X  = exp(-2.0 * pi * Fc)
 *      A0 = (1 + X) / 2
 *      A1 = -(1 + X) / 2
 *      B1 = X
 *      Fc = cutoff freq / sample rate
 */
struct IIR1 {
  REAL in0, out0;
  REAL a0, a1, b1;
};

#if 0
  IIR1() {
    memset(this, 0, sizeof(IIR1));
  }
#endif

static  IIR1* IIR1_init(REAL Fc) {
    IIR1 *i = pa_xnew(IIR1, 1);
    i->b1 = expf(-2.0f * M_PI * Fc);
    i->a0 = (1.0f + i->b1) / 2.0f;
    i->a1 = -(i->a0);
    i->in0 = 0.0f;
    i->out0 = 0.0f;
    return i;
  }

static  REAL IIR1_highpass(IIR1 *i, REAL in) {
    REAL out = i->a0 * in + i->a1 * i->in0 + i->b1 * i->out0;
    i->in0 = in;
    i->out0 = out;
    return out;
  }


#if 0
/* Recursive two pole IIR Infinite Impulse Response filter
 * Coefficients calculated with
 * http://www.dsptutor.freeuk.com/IIRFilterDesign/IIRFiltDes102.html
 */
class IIR2 {
  REAL x[2], y[2];

public:
   IIR2() {
    memset(this, 0, sizeof(IIR2));
  }

  REAL highpass(REAL in) {
    // Butterworth IIR filter, Filter type: HP
    // Passband: 2000 - 4000.0 Hz, Order: 2
    const REAL a[] = { 0.29289323f, -0.58578646f, 0.29289323f };
    const REAL b[] = { 1.3007072E-16f, 0.17157288f };
    REAL out =
      a[0] * in + a[1] * x[0] + a[2] * x[1] - b[0] * y[0] - b[1] * y[1];

    x[1] = x[0];
    x[0] = in;
    y[1] = y[0];
    y[0] = out;
    return out;
  }
};
#endif


// Extention in taps to reduce mem copies
#define NLMS_EXT  (10*8)

// block size in taps to optimize DTD calculation
#define DTD_LEN   16

typedef struct AEC AEC;

struct AEC {
  // Time domain Filters
  IIR_HP *acMic, *acSpk;        // DC-level remove Highpass)
  FIR_HP_300Hz *cutoff;         // 150Hz cut-off Highpass
  REAL gain;                    // Mic signal amplify
  IIR1 *Fx, *Fe;                // pre-whitening Highpass for x, e

  // Adrian soft decision DTD (Double Talk Detector)
  REAL dfast, xfast;
  REAL dslow, xslow;

  // NLMS-pw
  REAL x[NLMS_LEN + NLMS_EXT];  // tap delayed loudspeaker signal
  REAL xf[NLMS_LEN + NLMS_EXT]; // pre-whitening tap delayed signal
  REAL w_arr[NLMS_LEN + (16 / sizeof(REAL))]; // tap weights
  REAL *w;                      // this will be a 16-byte aligned pointer into w_arr
  int j;                        // optimize: less memory copies
  double dotp_xf_xf;            // double to avoid loss of precision
  float delta;                  // noise floor to stabilize NLMS

  // AES
  float aes_y2;                 // not in use!

  // w vector visualization
  REAL ws[DUMP_LEN];            // tap weights sums
  int fdwdisplay;               // TCP file descriptor
  int dumpcnt;                  // wdisplay output counter

  // variables are public for visualization
  int hangover;
  float stepsize;

  // vfuncs that are picked based on processor features available
  REAL (*dotp) (REAL[], REAL[]);
};

/* Double-Talk Detector
 *
 * in d: microphone sample (PCM as REALing point value)
 * in x: loudspeaker sample (PCM as REALing point value)
 * return: from 0 for doubletalk to 1.0 for single talk
 */
static  float AEC_dtd(AEC *a, REAL d, REAL x);

static  void AEC_leaky(AEC *a);

/* Normalized Least Mean Square Algorithm pre-whitening (NLMS-pw)
 * The LMS algorithm was developed by Bernard Widrow
 * book: Haykin, Adaptive Filter Theory, 4. edition, Prentice Hall, 2002
 *
 * in d: microphone sample (16bit PCM value)
 * in x_: loudspeaker sample (16bit PCM value)
 * in stepsize: NLMS adaptation variable
 * return: echo cancelled microphone sample
 */
static  REAL AEC_nlms_pw(AEC *a, REAL d, REAL x_, float stepsize);

AEC* AEC_init(int RATE, int have_vector);
void AEC_done(AEC *a);

/* Acoustic Echo Cancellation and Suppression of one sample
 * in   d:  microphone signal with echo
 * in   x:  loudspeaker signal
 * return:  echo cancelled microphone signal
 */
  int AEC_doAEC(AEC *a, int d_, int x_);

PA_GCC_UNUSED static  float AEC_getambient(AEC *a) {
    return a->dfast;
  }
static  void AEC_setambient(AEC *a, float Min_xf) {
    a->dotp_xf_xf -= a->delta;  // subtract old delta
    a->delta = (NLMS_LEN-1) * Min_xf * Min_xf;
    a->dotp_xf_xf += a->delta;  // add new delta
  }
PA_GCC_UNUSED static  void AEC_setgain(AEC *a, float gain_) {
    a->gain = gain_;
  }
#if 0
  void AEC_openwdisplay(AEC *a);
#endif
PA_GCC_UNUSED static  void AEC_setaes(AEC *a, float aes_y2_) {
    a->aes_y2 = aes_y2_;
  }

#define _AEC_H
#endif

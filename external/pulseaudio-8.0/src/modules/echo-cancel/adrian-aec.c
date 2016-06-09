/* aec.cpp
 *
 * Copyright (C) DFS Deutsche Flugsicherung (2004, 2005).
 * All Rights Reserved.
 *
 * Acoustic Echo Cancellation NLMS-pw algorithm
 *
 * Version 0.3 filter created with www.dsptutor.freeuk.com
 * Version 0.3.1 Allow change of stability parameter delta
 * Version 0.4 Leaky Normalized LMS - pre whitening algorithm
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <math.h>
#include <string.h>
#include <stdint.h>

#include <pulse/xmalloc.h>

#include "adrian-aec.h"

#ifndef DISABLE_ORC
#include "adrian-aec-orc-gen.h"
#endif

#ifdef __SSE__
#include <xmmintrin.h>
#endif

/* Vector Dot Product */
static REAL dotp(REAL a[], REAL b[])
{
  REAL sum0 = 0.0f, sum1 = 0.0f;
  int j;

  for (j = 0; j < NLMS_LEN; j += 2) {
    // optimize: partial loop unrolling
    sum0 += a[j] * b[j];
    sum1 += a[j + 1] * b[j + 1];
  }
  return sum0 + sum1;
}

static REAL dotp_sse(REAL a[], REAL b[])
{
#ifdef __SSE__
  /* This is taken from speex's inner product implementation */
  int j;
  REAL sum;
  __m128 acc = _mm_setzero_ps();

  for (j=0;j<NLMS_LEN;j+=8)
  {
    acc = _mm_add_ps(acc, _mm_mul_ps(_mm_load_ps(a+j), _mm_loadu_ps(b+j)));
    acc = _mm_add_ps(acc, _mm_mul_ps(_mm_load_ps(a+j+4), _mm_loadu_ps(b+j+4)));
  }
  acc = _mm_add_ps(acc, _mm_movehl_ps(acc, acc));
  acc = _mm_add_ss(acc, _mm_shuffle_ps(acc, acc, 0x55));
  _mm_store_ss(&sum, acc);

  return sum;
#else
  return dotp(a, b);
#endif
}


AEC* AEC_init(int RATE, int have_vector)
{
  AEC *a = pa_xnew0(AEC, 1);
  a->j = NLMS_EXT;
  AEC_setambient(a, NoiseFloor);
  a->dfast = a->dslow = M75dB_PCM;
  a->xfast = a->xslow = M80dB_PCM;
  a->gain = 1.0f;
  a->Fx = IIR1_init(2000.0f/RATE);
  a->Fe = IIR1_init(2000.0f/RATE);
  a->cutoff = FIR_HP_300Hz_init();
  a->acMic = IIR_HP_init();
  a->acSpk = IIR_HP_init();

  a->aes_y2 = M0dB;

  a->fdwdisplay = -1;

  if (have_vector) {
      /* Get a 16-byte aligned location */
      a->w = (REAL *) (((uintptr_t) a->w_arr) - (((uintptr_t) a->w_arr) % 16) + 16);
      a->dotp = dotp_sse;
  } else {
      /* We don't care about alignment, just use the array as-is */
      a->w = a->w_arr;
      a->dotp = dotp;
  }

  return a;
}

void AEC_done(AEC *a) {
    pa_assert(a);

    pa_xfree(a->Fx);
    pa_xfree(a->Fe);
    pa_xfree(a->acMic);
    pa_xfree(a->acSpk);
    pa_xfree(a->cutoff);
    pa_xfree(a);
}

// Adrian soft decision DTD
// (Dual Average Near-End to Far-End signal Ratio DTD)
// This algorithm uses exponential smoothing with differnt
// ageing parameters to get fast and slow near-end and far-end
// signal averages. The ratio of NFRs term
// (dfast / xfast) / (dslow / xslow) is used to compute the stepsize
// A ratio value of 2.5 is mapped to stepsize 0, a ratio of 0 is
// mapped to 1.0 with a limited linear function.
static float AEC_dtd(AEC *a, REAL d, REAL x)
{
  float ratio, stepsize;

  // fast near-end and far-end average
  a->dfast += ALPHAFAST * (fabsf(d) - a->dfast);
  a->xfast += ALPHAFAST * (fabsf(x) - a->xfast);

  // slow near-end and far-end average
  a->dslow += ALPHASLOW * (fabsf(d) - a->dslow);
  a->xslow += ALPHASLOW * (fabsf(x) - a->xslow);

  if (a->xfast < M70dB_PCM) {
    return 0.0f;   // no Spk signal
  }

  if (a->dfast < M70dB_PCM) {
    return 0.0f;   // no Mic signal
  }

  // ratio of NFRs
  ratio = (a->dfast * a->xslow) / (a->dslow * a->xfast);

  // Linear interpolation with clamping at the limits
  if (ratio < STEPX1)
    stepsize = STEPY1;
  else if (ratio > STEPX2)
    stepsize = STEPY2;
  else
    stepsize = STEPY1 + (STEPY2 - STEPY1) * (ratio - STEPX1) / (STEPX2 - STEPX1);

  return stepsize;
}


static void AEC_leaky(AEC *a)
// The xfast signal is used to charge the hangover timer to Thold.
// When hangover expires (no Spk signal for some time) the vector w
// is erased. This is my implementation of Leaky NLMS.
{
  if (a->xfast >= M70dB_PCM) {
    // vector w is valid for hangover Thold time
    a->hangover = Thold;
  } else {
    if (a->hangover > 1) {
      --(a->hangover);
    } else if (1 == a->hangover) {
      --(a->hangover);
      // My Leaky NLMS is to erase vector w when hangover expires
      memset(a->w_arr, 0, sizeof(a->w_arr));
    }
  }
}


#if 0
void AEC::openwdisplay() {
  // open TCP connection to program wdisplay.tcl
  fdwdisplay = socket_async("127.0.0.1", 50999);
};
#endif


static REAL AEC_nlms_pw(AEC *a, REAL d, REAL x_, float stepsize)
{
  REAL e;
  REAL ef;
  a->x[a->j] = x_;
  a->xf[a->j] = IIR1_highpass(a->Fx, x_);     // pre-whitening of x

  // calculate error value
  // (mic signal - estimated mic signal from spk signal)
  e = d;
  if (a->hangover > 0) {
    e -= a->dotp(a->w, a->x + a->j);
  }
  ef = IIR1_highpass(a->Fe, e);     // pre-whitening of e

  // optimize: iterative dotp(xf, xf)
  a->dotp_xf_xf += (a->xf[a->j] * a->xf[a->j] - a->xf[a->j + NLMS_LEN - 1] * a->xf[a->j + NLMS_LEN - 1]);

  if (stepsize > 0.0f) {
    // calculate variable step size
    REAL mikro_ef = stepsize * ef / a->dotp_xf_xf;

#ifdef DISABLE_ORC
    // update tap weights (filter learning)
    int i;
    for (i = 0; i < NLMS_LEN; i += 2) {
      // optimize: partial loop unrolling
      a->w[i] += mikro_ef * a->xf[i + a->j];
      a->w[i + 1] += mikro_ef * a->xf[i + a->j + 1];
    }
#else
    update_tap_weights(a->w, &a->xf[a->j], mikro_ef, NLMS_LEN);
#endif
  }

  if (--(a->j) < 0) {
    // optimize: decrease number of memory copies
    a->j = NLMS_EXT;
    memmove(a->x + a->j + 1, a->x, (NLMS_LEN - 1) * sizeof(REAL));
    memmove(a->xf + a->j + 1, a->xf, (NLMS_LEN - 1) * sizeof(REAL));
  }

  // Saturation
  if (e > MAXPCM) {
    return MAXPCM;
  } else if (e < -MAXPCM) {
    return -MAXPCM;
  } else {
    return e;
  }
}


int AEC_doAEC(AEC *a, int d_, int x_)
{
  REAL d = (REAL) d_;
  REAL x = (REAL) x_;

  // Mic Highpass Filter - to remove DC
  d = IIR_HP_highpass(a->acMic, d);

  // Mic Highpass Filter - cut-off below 300Hz
  d = FIR_HP_300Hz_highpass(a->cutoff, d);

  // Amplify, for e.g. Soundcards with -6dB max. volume
  d *= a->gain;

  // Spk Highpass Filter - to remove DC
  x = IIR_HP_highpass(a->acSpk, x);

  // Double Talk Detector
  a->stepsize = AEC_dtd(a, d, x);

  // Leaky (ageing of vector w)
  AEC_leaky(a);

  // Acoustic Echo Cancellation
  d = AEC_nlms_pw(a, d, x, a->stepsize);

#if 0
  if (fdwdisplay >= 0) {
    if (++dumpcnt >= (WIDEB*RATE/10)) {
      // wdisplay creates 10 dumps per seconds = large CPU load!
      dumpcnt = 0;
      write(fdwdisplay, ws, DUMP_LEN*sizeof(float));
      // we don't check return value. This is not production quality!!!
      memset(ws, 0, sizeof(ws));
    } else {
      int i;
      for (i = 0; i < DUMP_LEN; i += 2) {
        // optimize: partial loop unrolling
        ws[i] += w[i];
        ws[i + 1] += w[i + 1];
      }
    }
  }
#endif

  return (int) d;
}

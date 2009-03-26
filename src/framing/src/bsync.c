//
// P/N synchronizer
//

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "liquid.internal.h"

struct BSYNC(_s) {
    unsigned int n;     // sequence length
    bsequence sync_i;   // synchronization pattern
    bsequence sync_q;   //
    bsequence sym_i;    // received symbols
    bsequence sym_q;    //
    TO rxy;             // cross correlation
};

BSYNC() BSYNC(_create)(unsigned int _n, TC * _v)
{
    BSYNC() fs = (BSYNC()) malloc(sizeof(struct BSYNC(_s)));
    fs->n = _n;

    fs->sync_i  = bsequence_create(fs->n);
#ifdef TC_COMPLEX
    fs->sync_q  = bsequence_create(fs->n);
#endif

    fs->sym_i   = bsequence_create(fs->n);
#ifdef TI_COMPLEX
    fs->sym_q   = bsequence_create(fs->n);
#endif

    unsigned int i;
    for (i=0; i<fs->n; i++) {
        bsequence_push(fs->sync_i, crealf(_v[i])>0);
#ifdef TC_COMPLEX
        bsequence_push(fs->sync_q, cimagf(_v[i])>0);
#endif
    }

    return fs;
}

BSYNC() BSYNC(_create_msequence)(unsigned int _g)
{
    unsigned int m;
    switch (_g) {
    case LIQUID_MSEQUENCE_N3:       m=2;    break;
    case LIQUID_MSEQUENCE_N7:       m=3;    break;
    case LIQUID_MSEQUENCE_N15:      m=4;    break;
    case LIQUID_MSEQUENCE_N31:      m=5;    break;
    case LIQUID_MSEQUENCE_N63:      m=6;    break;
    case LIQUID_MSEQUENCE_N127:     m=7;    break;
    case LIQUID_MSEQUENCE_N255:     m=8;    break;
    case LIQUID_MSEQUENCE_N511:     m=9;    break;
    case LIQUID_MSEQUENCE_N1023:    m=10;   break;
    case LIQUID_MSEQUENCE_N2047:    m=11;   break;
    case LIQUID_MSEQUENCE_N4095:    m=12;   break;
    default:
        printf("error: framesync_create_msequence(), unsupported generator polynomial %.32x\n", _g);
        exit(0);
    }
    msequence ms = msequence_create(m);

    BSYNC() fs = (BSYNC()) malloc(sizeof(struct BSYNC(_s)));
    fs->n = msequence_get_length(ms);

    fs->sync_i  = bsequence_create(fs->n);
#ifdef TC_COMPLEX
    fs->sync_q  = bsequence_create(fs->n);
#endif

    fs->sym_i   = bsequence_create(fs->n);
#ifdef TI_COMPLEX
    fs->sym_q   = bsequence_create(fs->n);
#endif

    msequence_reset(ms);
    bsequence_init_msequence(fs->sync_i,ms);
#ifdef TC_COMPLEX
    msequence_reset(ms);
    bsequence_init_msequence(fs->sync_q,ms);
#endif

    msequence_destroy(ms);

    return fs;
}

void BSYNC(_destroy)(BSYNC() _fs)
{
    bsequence_destroy(_fs->sync_i);
#ifdef TC_COMPLEX
    bsequence_destroy(_fs->sync_q);
#endif

    bsequence_destroy(_fs->sym_i);
#ifdef TI_COMPLEX
    bsequence_destroy(_fs->sym_q);
#endif
    free(_fs);
}

void BSYNC(_print)(BSYNC() _fs)
{

}

void BSYNC(_correlate)(BSYNC() _fs, TI _sym, TO *_y)
{
    // push symbol into buffers
    bsequence_push(_fs->sym_i, crealf(_sym)>0);
#ifdef TI_COMPLEX
    bsequence_push(_fs->sym_q, cimagf(_sym)>0);
#endif

    // compute dotprod
#if   defined TC_COMPLEX && defined TO_COMPLEX
    // cccx
    _fs->rxy = bsequence_correlate(_fs->sync_i, _fs->sym_i)
             - bsequence_correlate(_fs->sync_q, _fs->sym_q)
             +(bsequence_correlate(_fs->sync_i, _fs->sym_q)
             + bsequence_correlate(_fs->sync_q, _fs->sym_i)) * _Complex_I;
#elif defined TC_COMPLEX
    // crcx
    _fs->rxy = bsequence_correlate(_fs->sync_i, _fs->sym_i)
             + bsequence_correlate(_fs->sync_i, _fs->sym_q) * _Complex_I;
#else
    // rrrx
    _fs->rxy = bsequence_correlate(_fs->sync_i, _fs->sym_i);
#endif

    *_y = _fs->rxy / (float)(_fs->n);
}


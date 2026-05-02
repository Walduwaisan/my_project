/*
 *  ============================================================
 *  File:     waveform.h
 *  Module:   Analytical kernel of the Power Quality Waveform
 *            Analyser.  Declares the public data structures and
 *            function interface consumed by main.c and io.c.
 *  Standard: C99.
 *  ============================================================
 */

#ifndef WAVEFORM_H_
#define WAVEFORM_H_

#include <stddef.h>
#include <stdint.h>

/* ---------- Grid parameters and sensor limits -------------- */
/*
 *  Constants are declared as  static const double  rather than
 *  as #define macros.  This attaches a type to each value so
 *  that compiler diagnostics remain meaningful at their point
 *  of use, and so that the debugger can print them.
 */
static const double  NominalRmsVoltage           = 230.0;
static const double  RmsTolerancePercentage      =  10.0;
static const double  ClippingVoltageThreshold    = 324.9;
static const double  SignificantDcOffsetVoltage  =   1.0;

/* ---------- Status-flag bit positions ---------------------- */
/*
 *  Bitwise flags packed into an 8-bit status word carried by
 *  each PhaseAnalysisResult.  Bits 3..7 are reserved.
 *
 *      bit 0 : signal reached sensor saturation (clipping)
 *      bit 1 : RMS voltage outside +/- tolerance band
 *      bit 2 : arithmetic mean departs significantly from zero
 */
static const unsigned char StatusBitClipping        = 0x01u;
static const unsigned char StatusBitOutOfTolerance  = 0x02u;
static const unsigned char StatusBitDcOffset        = 0x04u;

/* ---------- Data types ------------------------------------- */

/*
 *  A single logged measurement.  The field ordering mirrors the
 *  CSV column ordering supplied by the dataset specification so
 *  that a reader can map struct fields directly to column names
 *  without external documentation.
 */
typedef struct WaveformSample_s {
    double sampleTimestampSeconds;
    double phaseVoltageA;
    double phaseVoltageB;
    double phaseVoltageC;
    double lineCurrentAmperes;
    double frequencyHertz;
    double powerFactorRatio;
    double totalHarmonicDistortionPercent;
} WaveformSample;

/*
 *  Runtime identifier for a single electrical phase.  The
 *  enumerator value doubles as an index used by the phase-
 *  voltage selector macro below.
 */
typedef enum {
    PHASE_INDEX_A = 0,
    PHASE_INDEX_B = 1,
    PHASE_INDEX_C = 2
} PhaseIndex;

/*
 *  Aggregated analytical results for a single phase.  Produced
 *  by AnalysePhase() and consumed by the report writer.
 */
typedef struct PhaseAnalysisResult_s {
    double        rootMeanSquareVoltage;
    double        peakToPeakAmplitude;
    double        directCurrentOffset;
    double        populationVariance;
    double        standardDeviation;
    double        minimumVoltageValue;
    double        maximumVoltageValue;
    size_t        numberOfClippingEvents;
    int           isWithinTolerance;
    unsigned char statusFlags;
} PhaseAnalysisResult;

/* ---------- Phase-voltage selector ------------------------- */
/*
 *  PHASE_VOLTAGE(p, ph) expands to an lvalue referring to the
 *  voltage field of *p corresponding to phase  ph .  Implemented
 *  as a conditional expression so that the phase branch is
 *  constant-folded at any call site where  ph  is a compile-
 *  time constant.  The first macro argument is evaluated once.
 */
#define PHASE_VOLTAGE(SAMPLE_PTR, PHASE_INDEX)                       \
    ( (PHASE_INDEX) == PHASE_INDEX_A ? (SAMPLE_PTR)->phaseVoltageA : \
      (PHASE_INDEX) == PHASE_INDEX_B ? (SAMPLE_PTR)->phaseVoltageB : \
                                        (SAMPLE_PTR)->phaseVoltageC )

/* ---------- Public interface ------------------------------- */

double ComputeRootMeanSquareVoltage(
        const WaveformSample *pSamples,
        size_t numberOfSamples,
        PhaseIndex phase);

double ComputePeakToPeakAmplitude(
        const WaveformSample *pSamples,
        size_t numberOfSamples,
        PhaseIndex phase);

double ComputeDirectCurrentOffset(
        const WaveformSample *pSamples,
        size_t numberOfSamples,
        PhaseIndex phase);

double ComputePopulationVariance(
        const WaveformSample *pSamples,
        size_t numberOfSamples,
        PhaseIndex phase);

double ComputeStandardDeviation(
        const WaveformSample *pSamples,
        size_t numberOfSamples,
        PhaseIndex phase);

size_t CountClippingEvents(
        const WaveformSample *pSamples,
        size_t numberOfSamples,
        PhaseIndex phase);

void FindVoltageExtremes(
        const WaveformSample *pSamples,
        size_t numberOfSamples,
        PhaseIndex phase,
        double *pOutputMinimumVoltage,
        double *pOutputMaximumVoltage);

int IsRmsWithinTolerance(double rmsVoltage);

unsigned char DeriveStatusFlagsFromAnalysis(
        const PhaseAnalysisResult *pAnalysis);

PhaseAnalysisResult AnalysePhase(
        const WaveformSample *pSamples,
        size_t numberOfSamples,
        PhaseIndex phase);

double *CopyPhaseVoltagesToHeap(
        const WaveformSample *pSamples,
        size_t numberOfSamples,
        PhaseIndex phase);

void SelectionSortDescendingByAbsoluteValue(
        double *pBuffer,
        size_t bufferLength);

#endif /* WAVEFORM_H_ */

/*
 *  ============================================================
 *  File:     waveform.c
 *  Purpose:  Implementation of the analytical kernel.  Each
 *            routine is accompanied by a brief statement of
 *            the mathematical quantity it evaluates and a note
 *            on numerical or algorithmic considerations where
 *            they affect the choice of implementation.
 *  ============================================================
 */

#include "waveform.h"

#include <math.h>
#include <stdlib.h>
#include <assert.h>

/*
 *  Theory.
 *      The root-mean-square of a finite sequence  {v_0,..,v_{n-1}}
 *      is defined as
 *
 *              rms = sqrt( (1/n) * sum_{i=0}^{n-1}  v_i^2 ).
 *
 *      For a continuous sinusoid  v(t) = V_p sin(omega t)  the
 *      closed-form RMS is  V_p / sqrt(2).  A dataset of 1000
 *      samples drawn from such a signal with V_p = 325 V should
 *      therefore yield an RMS in the neighbourhood of 229.81 V;
 *      the sampled result will approach this limit as n grows.
 *
 *  Implementation.
 *      Single pass.  The accumulator is a  double  so that
 *      quantities of order 10^5 V^2 do not exhaust significand
 *      precision.  Index traversal is performed by pointer
 *      addition from the base,  pSamples + sampleIdx , with the
 *      compiler responsible for scaling by sizeof(WaveformSample).
 */
double ComputeRootMeanSquareVoltage(
        const WaveformSample *pSamples,
        size_t numberOfSamples,
        PhaseIndex phase)
{
    if (pSamples == NULL || numberOfSamples == 0) {
        return 0.0;
    }

    double sumOfSquares = 0.0;
    for (size_t sampleIdx = 0; sampleIdx < numberOfSamples; ++sampleIdx) {
        const WaveformSample *pCurrent = pSamples + sampleIdx;
        double v = PHASE_VOLTAGE(pCurrent, phase);
        sumOfSquares += v * v;
    }
    return sqrt(sumOfSquares / (double)numberOfSamples);
}


double ComputeDirectCurrentOffset(
        const WaveformSample *pSamples,
        size_t numberOfSamples,
        PhaseIndex phase)
{
    if (pSamples == NULL || numberOfSamples == 0) {
        return 0.0;
    }

    double runningTotal = 0.0;
    for (size_t sampleIdx = 0; sampleIdx < numberOfSamples; ++sampleIdx) {
        const WaveformSample *pCurrent = pSamples + sampleIdx;
        runningTotal += PHASE_VOLTAGE(pCurrent, phase);
    }
    return runningTotal / (double)numberOfSamples;
}


double ComputePopulationVariance(
        const WaveformSample *pSamples,
        size_t numberOfSamples,
        PhaseIndex phase)
{
    if (pSamples == NULL || numberOfSamples == 0) {
        return 0.0;
    }

    const double mean = ComputeDirectCurrentOffset(pSamples, numberOfSamples, phase);

    double sumOfSquaredDeviations = 0.0;
    for (size_t sampleIdx = 0; sampleIdx < numberOfSamples; ++sampleIdx) {
        const WaveformSample *pCurrent = pSamples + sampleIdx;
        double deviation = PHASE_VOLTAGE(pCurrent, phase) - mean;
        sumOfSquaredDeviations += deviation * deviation;
    }
    return sumOfSquaredDeviations / (double)numberOfSamples;
}

/*
 *  Standard deviation is the positive square root of variance.
 */
double ComputeStandardDeviation(
        const WaveformSample *pSamples,
        size_t numberOfSamples,
        PhaseIndex phase)
{
    return sqrt(ComputePopulationVariance(pSamples, numberOfSamples, phase));
}

/*
 *  Extremes.  Computes minimum and maximum in a single pass by
 *  maintaining two running accumulators.  The alternative - a
 *  sort followed by reading the first and last elements - has
 *  higher complexity and is unnecessary here.
 */
void FindVoltageExtremes(
        const WaveformSample *pSamples,
        size_t numberOfSamples,
        PhaseIndex phase,
        double *pOutputMinimumVoltage,
        double *pOutputMaximumVoltage)
{
    assert(pOutputMinimumVoltage != NULL);
    assert(pOutputMaximumVoltage != NULL);

    if (pSamples == NULL || numberOfSamples == 0) {
        *pOutputMinimumVoltage = 0.0;
        *pOutputMaximumVoltage = 0.0;
        return;
    }

    double observedMinimum = PHASE_VOLTAGE(pSamples, phase);
    double observedMaximum = observedMinimum;

    for (size_t sampleIdx = 1; sampleIdx < numberOfSamples; ++sampleIdx) {
        const WaveformSample *pCurrent = pSamples + sampleIdx;
        double v = PHASE_VOLTAGE(pCurrent, phase);
        if (v < observedMinimum) {
            observedMinimum = v;
        } else if (v > observedMaximum) {
            observedMaximum = v;
        }
    }

    *pOutputMinimumVoltage = observedMinimum;
    *pOutputMaximumVoltage = observedMaximum;
}

double ComputePeakToPeakAmplitude(
        const WaveformSample *pSamples,
        size_t numberOfSamples,
        PhaseIndex phase)
{
    double minimumVoltage = 0.0;
    double maximumVoltage = 0.0;
    FindVoltageExtremes(pSamples, numberOfSamples, phase,
                        &minimumVoltage, &maximumVoltage);
    return maximumVoltage - minimumVoltage;
}

/*
 *  Clipping detector.  The sensor specification states that
 *  readings at or beyond +/-324.9 V are artefacts of analog
 *  saturation rather than true measurements.  Any sample whose
 *  absolute value meets or exceeds that threshold is counted.
 */
size_t CountClippingEvents(
        const WaveformSample *pSamples,
        size_t numberOfSamples,
        PhaseIndex phase)
{
    if (pSamples == NULL) return 0;

    size_t numberOfEventsObserved = 0;
    for (size_t sampleIdx = 0; sampleIdx < numberOfSamples; ++sampleIdx) {
        const WaveformSample *pCurrent = pSamples + sampleIdx;
        if (fabs(PHASE_VOLTAGE(pCurrent, phase)) >= ClippingVoltageThreshold) {
            ++numberOfEventsObserved;
        }
    }
    return numberOfEventsObserved;
}

/*
 *  Tolerance.  IEC-style compliance check: RMS must lie within
 *  +/- RmsTolerancePercentage of the nominal line voltage.
 */
int IsRmsWithinTolerance(double rmsVoltage)
{
    double allowedDelta = NominalRmsVoltage *
                          (RmsTolerancePercentage / 100.0);
    double lowerBound = NominalRmsVoltage - allowedDelta;
    double upperBound = NominalRmsVoltage + allowedDelta;

    if (rmsVoltage < lowerBound) return 0;
    if (rmsVoltage > upperBound) return 0;
    return 1;
}

/*
 *  Status derivation.  Three conditions are encoded into a
 *  single unsigned byte by bitwise OR of the StatusBit*
 *  constants.  The resulting word can be tested cheaply at
 *  call sites:  if (flags & StatusBitClipping) ...
 */
unsigned char DeriveStatusFlagsFromAnalysis(const PhaseAnalysisResult *pAnalysis)
{
    assert(pAnalysis != NULL);

    unsigned char flags = 0;

    if (pAnalysis->numberOfClippingEvents > 0) {
        flags |= StatusBitClipping;
    }
    if (pAnalysis->isWithinTolerance == 0) {
        flags |= StatusBitOutOfTolerance;
    }
    if (fabs(pAnalysis->directCurrentOffset) > SignificantDcOffsetVoltage) {
        flags |= StatusBitDcOffset;
    }

    return flags;
}

/*
 *  Composite analysis.  Dispatches to the individual metric
 *  functions and assembles a PhaseAnalysisResult for return.
 *  Returned by value:  the structure is small (< 80 bytes on
 *  all common ABIs), so the copy is inexpensive and the caller
 *  is freed from ownership concerns.
 */
PhaseAnalysisResult AnalysePhase(
        const WaveformSample *pSamples,
        size_t numberOfSamples,
        PhaseIndex phase)
{
    PhaseAnalysisResult result;

    result.rootMeanSquareVoltage =
            ComputeRootMeanSquareVoltage(pSamples, numberOfSamples, phase);
    result.peakToPeakAmplitude =
            ComputePeakToPeakAmplitude(pSamples, numberOfSamples, phase);
    result.directCurrentOffset =
            ComputeDirectCurrentOffset(pSamples, numberOfSamples, phase);
    result.populationVariance =
            ComputePopulationVariance(pSamples, numberOfSamples, phase);
    result.standardDeviation =
            sqrt(result.populationVariance);

    FindVoltageExtremes(pSamples, numberOfSamples, phase,
                        &result.minimumVoltageValue,
                        &result.maximumVoltageValue);

    result.numberOfClippingEvents =
            CountClippingEvents(pSamples, numberOfSamples, phase);
    result.isWithinTolerance =
            IsRmsWithinTolerance(result.rootMeanSquareVoltage);
    result.statusFlags =
            DeriveStatusFlagsFromAnalysis(&result);

    return result;
}
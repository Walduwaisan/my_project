

#include "waveform.h"

#include <math.h>
#include <stdlib.h>
#include <assert.h>


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
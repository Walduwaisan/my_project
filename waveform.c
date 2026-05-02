

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
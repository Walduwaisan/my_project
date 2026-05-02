/*
 *  ============================================================
 *  File:     io.c
 *  Purpose:  File-system implementation.  The CSV loader uses
 *            strtok() for field tokenisation and strtod() for
 *            numeric conversion.  Memory is sized exactly by a
 *            preliminary row-counting pass.  The report writer
 *            emits a five-section structured document.
 *  ============================================================
 */

#include "io.h"
#include "waveform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <dirent.h>

/*
 *  Buffer sizing.
 *      A sample row consists of eight numeric fields separated
 *      by commas and terminated by a line break.  Full-precision
 *      double output (17 significant digits) can produce rows
 *      approaching 320 bytes; the early constant of 256 bytes
 *      proved insufficient on such inputs.  LINE_LENGTH_BYTES is
 *      set to 1024, providing approximately threefold headroom.
 */
#define LINE_LENGTH_BYTES         1024
#define NUMBER_OF_COLUMNS            8
#define TOP_SAMPLES_DISPLAYED        5

/*  True iff pLine contains only whitespace characters.  */
static int IsBlankLine(const char *pLine)
{
    for (const char *pChar = pLine; *pChar != '\0'; ++pChar) {
        if (*pChar != ' '  && *pChar != '\t' &&
            *pChar != '\r' && *pChar != '\n') {
            return 0;
        }
    }
    return 1;
}

/*
 *  CountDataRows.
 *
 *      Traverses the file once, counting non-blank lines after
 *      the header, then rewinds the stream.  This pre-count
 *      allows the loader to obtain an exact-sized allocation in
 *      a single malloc() call, as required by the brief.
 */
static size_t CountDataRows(FILE *pFile)
{
    char lineBuffer[LINE_LENGTH_BYTES];
    size_t dataRowCount = 0;
    int    lineNumber   = 0;

    while (fgets(lineBuffer, sizeof(lineBuffer), pFile) != NULL) {
        ++lineNumber;
        if (lineNumber == 1) continue;                /* skip header */
        if (!IsBlankLine(lineBuffer)) ++dataRowCount;
    }

    rewind(pFile);
    return dataRowCount;
}

/*
 *  ParseRowIntoSample.
 *
 *      Tokenises pLine by the separator set  ",\r\n"  and
 *      converts each token to a double via strtod().  strtok is
 *      destructive but the supplied buffer is scratch, so that
 *      is acceptable here.
 *
 *      The inclusion of  \r  and  \n  in the delimiter set
 *      ensures correct behaviour on CRLF-terminated files,
 *      where otherwise the final token would carry a trailing
 *      carriage-return byte into strtod().
 */
static int ParseRowIntoSample(char *pLine, WaveformSample *pDestination)
{
    double *pDestinationFields[NUMBER_OF_COLUMNS] = {
        &pDestination->sampleTimestampSeconds,
        &pDestination->phaseVoltageA,
        &pDestination->phaseVoltageB,
        &pDestination->phaseVoltageC,
        &pDestination->lineCurrentAmperes,
        &pDestination->frequencyHertz,
        &pDestination->powerFactorRatio,
        &pDestination->totalHarmonicDistortionPercent
    };

    char *pToken = strtok(pLine, ",\r\n");
    for (int fieldIdx = 0; fieldIdx < NUMBER_OF_COLUMNS; ++fieldIdx) {
        if (pToken == NULL) {
            return 0;                                 /* truncated */
        }

        char *pEndOfNumber = NULL;
        double parsedValue = strtod(pToken, &pEndOfNumber);

        if (pEndOfNumber == pToken) {
            return 0;                                 /* non-numeric */
        }

        *(pDestinationFields[fieldIdx]) = parsedValue;
        pToken = strtok(NULL, ",\r\n");
    }
    return 1;
}

/*
 *  Loader proper.  Obtains a malloc() buffer sized to the data
 *  row count established by CountDataRows().
 */
WaveformSample *LoadCsvFileIntoSampleArray(
        const char *pFilePath,
        size_t *pOutputRowCount)
{
    if (pFilePath == NULL || pOutputRowCount == NULL) {
        fprintf(stderr, "LoadCsvFileIntoSampleArray: NULL argument\n");
        return NULL;
    }
    *pOutputRowCount = 0;

    FILE *pFile = fopen(pFilePath, "r");
    if (pFile == NULL) {
        fprintf(stderr, "ERROR | unable to open file: %s\n", pFilePath);
        return NULL;
    }

    size_t numberOfDataRows = CountDataRows(pFile);
    if (numberOfDataRows == 0) {
        fprintf(stderr, "ERROR | file '%s' contains no data rows\n", pFilePath);
        fclose(pFile);
        return NULL;
    }

    /*
     *  Allocation.  malloc() as mandated by the coursework
     *  brief; every field is subsequently written by the parser
     *  so the uninitialised content is of no consequence.
     */
    WaveformSample *pSamples = (WaveformSample *)
        malloc(numberOfDataRows * sizeof(WaveformSample));
    if (pSamples == NULL) {
        fprintf(stderr, "ERROR | out of memory (%zu samples requested)\n",
                numberOfDataRows);
        fclose(pFile);
        return NULL;
    }

    char lineBuffer[LINE_LENGTH_BYTES];

    /* Consume the header row. */
    if (fgets(lineBuffer, sizeof(lineBuffer), pFile) == NULL) {
        fprintf(stderr, "ERROR | cannot read header of '%s'\n", pFilePath);
        free(pSamples);
        fclose(pFile);
        return NULL;
    }

    size_t numberOfGoodRows = 0;
    int    lineNumber       = 1;    /* header already consumed */

    while (fgets(lineBuffer, sizeof(lineBuffer), pFile) != NULL) {
        ++lineNumber;
        if (IsBlankLine(lineBuffer)) continue;

        WaveformSample *pDestination = pSamples + numberOfGoodRows;
        if (ParseRowIntoSample(lineBuffer, pDestination)) {
            ++numberOfGoodRows;
        } else {
            fprintf(stderr,
                    "WARN  | '%s' line %d malformed; skipping\n",
                    pFilePath, lineNumber);
        }
    }

    fclose(pFile);

    if (numberOfGoodRows == 0) {
        fprintf(stderr, "ERROR | '%s' contained no parseable rows\n",
                pFilePath);
        free(pSamples);
        return NULL;
    }

    *pOutputRowCount = numberOfGoodRows;
    return pSamples;
}

/*
 *  Emit one phase's contribution to the report, including the
 *  "top N by magnitude" list produced via selection sort on a
 *  scratch copy of the phase column.
 */
static void WritePhaseSection(
        FILE                      *pFile,
        const char                *pSectionNumber,
        const char                *pPhaseLabel,
        const PhaseAnalysisResult *pAnalysis,
        const WaveformSample      *pSamples,
        size_t                     numberOfSamples,
        PhaseIndex                 phase)
{
    fprintf(pFile, "\n  %s  %s\n", pSectionNumber, pPhaseLabel);
    fprintf(pFile, "  ----------------------------------------------------\n");
    fprintf(pFile, "    Root-mean-square voltage ......... %12.4f V\n",
            pAnalysis->rootMeanSquareVoltage);
    fprintf(pFile, "    Peak-to-peak amplitude ........... %12.4f V\n",
            pAnalysis->peakToPeakAmplitude);
    fprintf(pFile, "    Observed minimum ................. %12.4f V\n",
            pAnalysis->minimumVoltageValue);
    fprintf(pFile, "    Observed maximum ................. %12.4f V\n",
            pAnalysis->maximumVoltageValue);
    fprintf(pFile, "    DC offset (arithmetic mean) ...... %12.6f V\n",
            pAnalysis->directCurrentOffset);
    fprintf(pFile, "    Population variance .............. %12.4f V^2\n",
            pAnalysis->populationVariance);
    fprintf(pFile, "    Standard deviation ............... %12.4f V\n",
            pAnalysis->standardDeviation);
    fprintf(pFile, "    Clipping events detected ......... %12zu\n",
            pAnalysis->numberOfClippingEvents);
    fprintf(pFile, "    Compliance (+/- %.0f %% of %.0f V) .. %12s\n",
            RmsTolerancePercentage, NominalRmsVoltage,
            pAnalysis->isWithinTolerance ? "COMPLIANT" : "NON-COMPLIANT");

    fprintf(pFile, "    Status word ...................... 0b");
    for (int bit = 7; bit >= 0; --bit) {
        fprintf(pFile, "%d", (pAnalysis->statusFlags >> bit) & 1);
    }
    fprintf(pFile, "  (0x%02X)\n", pAnalysis->statusFlags);

    if (pAnalysis->statusFlags & StatusBitClipping) {
        fprintf(pFile,
                "      bit 0 = SET : clipping threshold reached\n");
    }
    if (pAnalysis->statusFlags & StatusBitOutOfTolerance) {
        fprintf(pFile,
                "      bit 1 = SET : RMS outside +/- %.0f %% band\n",
                RmsTolerancePercentage);
    }
    if (pAnalysis->statusFlags & StatusBitDcOffset) {
        fprintf(pFile,
                "      bit 2 = SET : significant DC offset\n");
    }

    /* Distinction extension: top-N by magnitude via in-project sort. */
    double *pVoltages = CopyPhaseVoltagesToHeap(pSamples, numberOfSamples, phase);
    if (pVoltages != NULL) {
        SelectionSortDescendingByAbsoluteValue(pVoltages, numberOfSamples);
        size_t howMany = (numberOfSamples < TOP_SAMPLES_DISPLAYED)
                        ? numberOfSamples : TOP_SAMPLES_DISPLAYED;
        fprintf(pFile, "    Top %zu samples by absolute magnitude:\n", howMany);
        for (size_t rank = 0; rank < howMany; ++rank) {
            fprintf(pFile,
                    "      rank %zu : %+10.4f V  (|v| = %.4f)\n",
                    rank + 1, *(pVoltages + rank), fabs(*(pVoltages + rank)));
        }
        free(pVoltages);
    }
}

int WriteAnalysisReportToFile(
        const char                *pOutputFilePath,
        const char                *pSourceFileName,
        const WaveformSample      *pSamples,
        size_t                     numberOfSamples,
        const PhaseAnalysisResult *pPhaseA,
        const PhaseAnalysisResult *pPhaseB,
        const PhaseAnalysisResult *pPhaseC)
{
    if (pOutputFilePath == NULL || pSamples == NULL ||
        pPhaseA == NULL || pPhaseB == NULL || pPhaseC == NULL) {
        fprintf(stderr, "WriteAnalysisReportToFile: NULL argument\n");
        return 1;
    }

    FILE *pFile = fopen(pOutputFilePath, "w");
    if (pFile == NULL) {
        fprintf(stderr, "ERROR | unable to create output file: %s\n",
                pOutputFilePath);
        return 1;
    }

    /* ---- Header ---- */
    fprintf(pFile,
            "POWER QUALITY WAVEFORM ANALYSER\n"
            "Analytical Report\n"
            "====================================================\n\n");

    fprintf(pFile, "  I.  Dataset description\n");
    fprintf(pFile, "  ----------------------------------------------------\n");
    fprintf(pFile, "    Source file ........ %s\n",
            pSourceFileName ? pSourceFileName : "(unnamed)");
    fprintf(pFile, "    Samples processed .. %zu\n", numberOfSamples);
    if (numberOfSamples >= 2) {
        double samplingInterval =
            pSamples[1].sampleTimestampSeconds -
            pSamples[0].sampleTimestampSeconds;
        fprintf(pFile,
                "    Time range ......... %.6f s  ->  %.6f s\n",
                pSamples[0].sampleTimestampSeconds,
                pSamples[numberOfSamples - 1].sampleTimestampSeconds);
        if (samplingInterval > 0.0) {
            fprintf(pFile, "    Sampling rate ...... %.1f Hz\n",
                    1.0 / samplingInterval);
        }
    }

    WritePhaseSection(pFile, "II. ",  "Phase A analysis",
                      pPhaseA, pSamples, numberOfSamples, PHASE_INDEX_A);
    WritePhaseSection(pFile, "III.",  "Phase B analysis",
                      pPhaseB, pSamples, numberOfSamples, PHASE_INDEX_B);
    WritePhaseSection(pFile, "IV. ",  "Phase C analysis",
                      pPhaseC, pSamples, numberOfSamples, PHASE_INDEX_C);

    /* ---- Summary ---- */
    fprintf(pFile, "\n  V.  Summary and verdict\n");
    fprintf(pFile, "  ----------------------------------------------------\n");

    size_t totalClippingEvents =
          pPhaseA->numberOfClippingEvents
        + pPhaseB->numberOfClippingEvents
        + pPhaseC->numberOfClippingEvents;

    int allPhasesCompliant =
        pPhaseA->isWithinTolerance &&
        pPhaseB->isWithinTolerance &&
        pPhaseC->isWithinTolerance;

    unsigned char combinedStatus =
        pPhaseA->statusFlags |
        pPhaseB->statusFlags |
        pPhaseC->statusFlags;

    fprintf(pFile, "    Total clipping events  .. %zu\n",
            totalClippingEvents);
    fprintf(pFile, "    All phases compliant  ... %s\n",
            allPhasesCompliant ? "yes" : "no");
    fprintf(pFile, "    Combined status word .... 0x%02X\n",
            combinedStatus);

    fprintf(pFile, "\n    VERDICT: ");
    if (combinedStatus == 0) {
        fprintf(pFile,
                "NOMINAL - all metrics within acceptable operating range.\n");
    } else if (combinedStatus & StatusBitClipping) {
        fprintf(pFile,
                "ANOMALY - sensor saturation observed.  Recommend\n"
                "             immediate investigation of upstream voltage.\n");
    } else if (combinedStatus & StatusBitOutOfTolerance) {
        fprintf(pFile,
                "NON-COMPLIANT - RMS on one or more phases is outside\n"
                "             the +/- %.0f %% tolerance band.\n",
                RmsTolerancePercentage);
    } else {
        fprintf(pFile,
                "REVIEW - non-critical status flag(s) raised.\n");
    }

    fprintf(pFile, "\n====================================================\n");
    fprintf(pFile, "End of report.\n");

    fclose(pFile);
    return 0;
}

int ExecuteAnalysisPipelineForSingleFile(
        const char *pInputFilePath,
        const char *pOutputFilePath)
{
    if (pInputFilePath == NULL || pOutputFilePath == NULL) return 1;

    size_t numberOfSamples  = 0;
    WaveformSample *pSamples =
        LoadCsvFileIntoSampleArray(pInputFilePath, &numberOfSamples);
    if (pSamples == NULL) return 1;

    PhaseAnalysisResult phaseA = AnalysePhase(pSamples, numberOfSamples, PHASE_INDEX_A);
    PhaseAnalysisResult phaseB = AnalysePhase(pSamples, numberOfSamples, PHASE_INDEX_B);
    PhaseAnalysisResult phaseC = AnalysePhase(pSamples, numberOfSamples, PHASE_INDEX_C);

    int rc = WriteAnalysisReportToFile(
                pOutputFilePath, pInputFilePath,
                pSamples, numberOfSamples,
                &phaseA, &phaseB, &phaseC);

    free(pSamples);
    return rc;
}

static int FileNameHasCsvExtension(const char *pName)
{
    size_t length = strlen(pName);
    if (length < 4) return 0;

    const char *pExtension = pName + length - 4;
    return (pExtension[0] == '.')
        && (pExtension[1] == 'c' || pExtension[1] == 'C')
        && (pExtension[2] == 's' || pExtension[2] == 'S')
        && (pExtension[3] == 'v' || pExtension[3] == 'V');
}

int ExecuteAnalysisPipelineForDirectory(const char *pDirectoryPath)
{
    if (pDirectoryPath == NULL) return 1;

    DIR *pDirectory = opendir(pDirectoryPath);
    if (pDirectory == NULL) {
        fprintf(stderr,
                "ERROR | unable to open directory '%s'\n", pDirectoryPath);
        return 1;
    }

    int    overallReturnCode = 0;
    int    successfullyProcessed = 0;
    size_t directoryPathLength = strlen(pDirectoryPath);
    struct dirent *pEntry = NULL;

    while ((pEntry = readdir(pDirectory)) != NULL) {
        if (!FileNameHasCsvExtension(pEntry->d_name)) continue;

        size_t fileNameLength = strlen(pEntry->d_name);

        size_t inputPathBufferSize = directoryPathLength + fileNameLength + 2;
        char *pInputPath = (char *)malloc(inputPathBufferSize);
        if (pInputPath == NULL) {
            overallReturnCode = 1;
            continue;
        }
        snprintf(pInputPath, inputPathBufferSize, "%s/%s",
                 pDirectoryPath, pEntry->d_name);

        size_t outputPathBufferSize =
            directoryPathLength + fileNameLength + 16;
        char *pOutputPath = (char *)malloc(outputPathBufferSize);
        if (pOutputPath == NULL) {
            free(pInputPath);
            overallReturnCode = 1;
            continue;
        }
        int stemLength = (int)(fileNameLength - 4);
        snprintf(pOutputPath, outputPathBufferSize,
                 "%s/%.*s_results.txt",
                 pDirectoryPath, stemLength, pEntry->d_name);

        printf("INFO  | processing %s\n", pInputPath);
        printf("      |  -> %s\n", pOutputPath);

        int rc = ExecuteAnalysisPipelineForSingleFile(pInputPath, pOutputPath);
        if (rc != 0) overallReturnCode = rc;
        else ++successfullyProcessed;

        free(pInputPath);
        free(pOutputPath);
    }

    closedir(pDirectory);
    printf("INFO  | batch complete - %d file(s) processed\n",
           successfullyProcessed);
    return overallReturnCode;
}
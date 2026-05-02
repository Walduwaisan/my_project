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
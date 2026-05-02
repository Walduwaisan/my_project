/*
 *  ============================================================
 *  File:     io.h
 *  Purpose:  File-system interface.  Declares the loader, the
 *            report writer, and the two orchestration routines
 *            used by main.c for single-file and batch execution.
 *  ============================================================
 */

#ifndef IO_H_
#define IO_H_

#include <stddef.h>
#include "waveform.h"

/*
 *  LoadCsvFileIntoSampleArray.
 *
 *      Reads the CSV file at pFilePath, parses each data row
 *      into a WaveformSample, and returns a heap-allocated
 *      array containing the parsed samples.
 *
 *      The allocation is sized exactly to the number of data
 *      rows present in the file, determined by a preliminary
 *      counting pass.  The array is obtained via malloc() and
 *      must be released by the caller via free().
 *
 *      Arguments
 *          pFilePath          Path to the input CSV.
 *          pOutputRowCount    Receives the number of parsed rows.
 *
 *      Returns
 *          Pointer to the heap buffer on success; NULL on any
 *          error (file missing, empty, malformed beyond recovery,
 *          or allocation failure).  Diagnostics are written to
 *          stderr.
 */
WaveformSample *LoadCsvFileIntoSampleArray(
        const char *pFilePath,
        size_t *pOutputRowCount);

/*
 *  WriteAnalysisReportToFile.
 *
 *      Produces a plain-text analytical report structured as
 *      five numbered sections (see the README for layout).
 *
 *      Returns 0 on success, non-zero on I/O failure.
 */
int WriteAnalysisReportToFile(
        const char                *pOutputFilePath,
        const char                *pSourceFileName,
        const WaveformSample      *pSamples,
        size_t                     numberOfSamples,
        const PhaseAnalysisResult *pPhaseA,
        const PhaseAnalysisResult *pPhaseB,
        const PhaseAnalysisResult *pPhaseC);

/*
 *  ExecuteAnalysisPipelineForSingleFile.
 *
 *      End-to-end single-file pipeline: load, analyse, write,
 *      release.  All heap allocations are freed before return
 *      regardless of the exit path.
 */
int ExecuteAnalysisPipelineForSingleFile(
        const char *pInputFilePath,
        const char *pOutputFilePath);

/*
 *  ExecuteAnalysisPipelineForDirectory.
 *
 *      Batch pipeline.  Invokes
 *      ExecuteAnalysisPipelineForSingleFile() for every regular
 *      file in pDirectoryPath whose name ends in  .csv  (case
 *      insensitive).  The output for  foo.csv  is written to
 *      foo_results.txt  alongside the input.
 */
int ExecuteAnalysisPipelineForDirectory(
        const char *pDirectoryPath);

#endif /* IO_H_ */

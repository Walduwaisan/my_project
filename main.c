/*
 *  ============================================================
 *  File:     main.c
 *  Purpose:  Program entry point.  Parses the command line and
 *            dispatches to the appropriate pipeline in io.c.
 *            No analytical logic is performed here; the brief
 *            requires that main.c be a thin orchestrator.
 *  ============================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "io.h"

static void PrintUsageMessage(const char *pProgramName)
{
    fprintf(stderr,
        "\nPower Quality Waveform Analyser\n"
        "===============================\n"
        "\n"
        "  Usage:\n"
        "    %s  <input.csv>                  analyse one file (output: results.txt)\n"
        "    %s  <input.csv>  <output.txt>    analyse one file with named output\n"
        "    %s  --batch  <directory>         process every *.csv in <directory>\n"
        "    %s  --help                       print this usage summary\n"
        "\n",
        pProgramName, pProgramName, pProgramName, pProgramName);
}

static int PathReferencesADirectory(const char *pPath)
{
    struct stat statBuffer;
    if (stat(pPath, &statBuffer) != 0) return 0;
    return S_ISDIR(statBuffer.st_mode) ? 1 : 0;
}

int main(int argumentCount, char *argumentVector[])
{
    const char *pProgramName = (argumentCount > 0)
                               ? argumentVector[0]
                               : "analyser";

    /* Branch 1 - no arguments supplied. */
    if (argumentCount < 2) {
        fprintf(stderr, "ERROR | no input file specified.\n");
        PrintUsageMessage(pProgramName);
        return 1;
    }

    const char *pFirstArgument = argumentVector[1];

    /* Branch 2 - help requested. */
    if (strcmp(pFirstArgument, "--help") == 0 ||
        strcmp(pFirstArgument, "-h")     == 0) {
        PrintUsageMessage(pProgramName);
        return 0;
    }

    /* Branch 3 - batch mode. */
    if (strcmp(pFirstArgument, "--batch") == 0) {
        if (argumentCount < 3) {
            fprintf(stderr,
                    "ERROR | --batch requires a directory argument.\n");
            PrintUsageMessage(pProgramName);
            return 1;
        }

        const char *pDirectoryPath = argumentVector[2];
        if (!PathReferencesADirectory(pDirectoryPath)) {
            fprintf(stderr,
                    "ERROR | '%s' is not a directory.\n", pDirectoryPath);
            return 1;
        }

        return ExecuteAnalysisPipelineForDirectory(pDirectoryPath);
    }

    /* Branch 4 - single-file mode (default). */
    const char *pInputFilePath  = pFirstArgument;
    const char *pOutputFilePath = (argumentCount >= 3)
                                  ? argumentVector[2]
                                  : "results.txt";

    if (PathReferencesADirectory(pInputFilePath)) {
        fprintf(stderr,
                "ERROR | '%s' is a directory.  Use --batch for directories.\n",
                pInputFilePath);
        PrintUsageMessage(pProgramName);
        return 1;
    }

    int returnCode = ExecuteAnalysisPipelineForSingleFile(pInputFilePath,
                                                          pOutputFilePath);
    if (returnCode == 0) {
        printf("INFO  | analysis complete; report written to '%s'.\n",
               pOutputFilePath);
    }
    return returnCode;
}
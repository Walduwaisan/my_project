Power Quality Waveform Analyser
================================

A C program that analyses three-phase voltage waveforms from CSV
data files. Computes per-phase statistics and writes a text report
indicating whether the supply meets nominal grid tolerances.



------------

For each of the three phases (A, B, C) the analyser computes:

- Root-mean-square voltage
- Peak-to-peak amplitude
- Minimum and maximum voltage
- DC offset (arithmetic mean)
- Population variance and standard deviation
- Number of clipping events (samples at or beyond +/- 324.9 V)
- Tolerance status against a 230 V nominal at +/- 10 percent

The report includes per-phase results and an overall verdict for
each phase based on three status flags: clipping detected, RMS out
of tolerance, and significant DC offset.


Input format
------------

CSV file with eight columns per row:

timestamp, vA, vB, vC, current, frequency, power_factor, THD


Build
-----

mkdir build && cd build
cmake ..
make


Usage
-----

./analyser input.csv                    analyse one file (output: results.txt)
./analyser input.csv output.txt         analyse one file, named output
./analyser --batch directory/           process every *.csv in directory
./analyser --help                       print usage summary


Project structure
-----------------

main.c        Command-line dispatch
io.c / io.h   CSV loading and report writing
waveform.c    Analytical kernel (RMS, variance, sorting, etc.)
waveform.h    Data structures and public interface
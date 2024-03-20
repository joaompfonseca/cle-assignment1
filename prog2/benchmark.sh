# Usage: ./benchmark.sh
# Description: Compiles the source code, runs the multithreaded bitonic sort program, and outputs the results in a
#              "results" directory, where each file contains the execution time of the program for a specific
#              configuration of threads (1, 2, 4, 8) and array sizes (32, 256K, 1M, 16M).
# Example: ./benchmark.sh

OUTPUT_FILE="benchmark_results.txt"
FOLDER_NUMBERS="data"
FILE_NUMBERS="datSeq32.bin datSeq256K.bin datSeq1M.bin datSeq16M.bin"
N_THREADS="1 2 4 8"

# Create the output file
rm -f $OUTPUT_FILE
touch $OUTPUT_FILE

# Compile the source code
gcc -Wall -O3 -o bmprog2 multiBitonic.c

# Run the program for each configuration of threads and array sizes
for file in $FILE_NUMBERS; do
  for threads in $N_THREADS; do
    echo "Running program with file $file and $threads threads..."
    echo "File: $file; Threads: $threads" >> $OUTPUT_FILE
    ./bmprog2 -f $FOLDER_NUMBERS/$file -n $threads | grep '^\[TIME\]' >> $OUTPUT_FILE
    echo "" >> $OUTPUT_FILE
  done
done

# Clean-up
rm -f benchmark

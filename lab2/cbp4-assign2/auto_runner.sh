#!/bin/bash

# Check if the output file name is provided
if [ -z "$1" ]; then
  echo "Usage: $0 <output_file_name>"
  exit 1
fi

# Output file name from the command line argument
output_file="$1"

# Commands to be executed
commands=(
  "predictor /cad2/ece552f/cbp4_benchmarks/astar.cbp4.gz"
  "predictor /cad2/ece552f/cbp4_benchmarks/bwaves.cbp4.gz"
  "predictor /cad2/ece552f/cbp4_benchmarks/bzip2.cbp4.gz"
  "predictor /cad2/ece552f/cbp4_benchmarks/gcc.cbp4.gz"
  "predictor /cad2/ece552f/cbp4_benchmarks/gromacs.cbp4.gz"
  "predictor /cad2/ece552f/cbp4_benchmarks/hmmer.cbp4.gz"
  "predictor /cad2/ece552f/cbp4_benchmarks/mcf.cbp4.gz"
  "predictor /cad2/ece552f/cbp4_benchmarks/soplex.cbp4.gz"
)

# Start timer
start_time=$(date +%s)

# Run each command and append the output to the specified file
for cmd in "${commands[@]}"; do
  echo "Running: $cmd" | tee -a "$output_file"
  eval "$cmd" >> "$output_file" 2>&1
  echo "---" >> "$output_file"
done

# End timer
end_time=$(date +%s)
elapsed_time=$((end_time - start_time))

# Log the elapsed time
echo "Total execution time: ${elapsed_time} seconds" | tee -a "$output_file"

echo "All commands have been executed. Output saved in $output_file."
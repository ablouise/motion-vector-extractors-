# README.md

## Getting Started

To set up and use this project, follow these steps:

1. **Clone the repository**
2. **Install the required Python packages**
pip install -r requirements.txt

## Running the Benchmark

To run the full benchmark, use the provided shell script. For example:

./run_full_benchmark.sh videofile.h264 [num_executions]

- Replace `videofile.h264` with your input video file.
- `[num_executions]` is optional and specifies how many times you want to run the algorithm in parallel. The default is 1 if you don't specify it.

During execution, you’ll be presented with options. If you select **option `0`**, the script will:
- Run all benchmarks.
- Generate charts.
- Create a PowerPoint presentation (PPT).

> **Note:** Selecting option 0 will take longer because it performs both the benchmarks and the full reporting.

## Results Output

After the benchmarks are complete:
- All plot images (`.png`) and the PowerPoint presentation (`.ppt`), including the results, will be available in the `plot` folder.

## Current Results 

> **Note:** The 3 with FFMPEG Patched use the Naive return version of FFMPEG, and the one called "Same" - is a copy of the code that performs best on the patched running not  on the Patched

<img width="1600" height="900" alt="grouped_barchart_fps" src="https://github.com/user-attachments/assets/21f15b0b-f9a1-4ca6-8f5a-04c6f3347246" />

<img width="1600" height="900" alt="scaling_timeperframe" src="https://github.com/user-attachments/assets/16ae1c73-3a82-4525-b752-12fa3311d01d" />

<img width="3077" height="1112" alt="detail_table_15streams" src="https://github.com/user-attachments/assets/e1e74285-9eb4-4354-b18c-13a192364db4" />


# README.md

## Getting Started

To set up and use this project, follow these steps:

1. **Clone the repository**
2. **Install the required Python packages**
pip install -r requirements.txt

## Running the Benchmark

To run the full benchmark, use the provided shell script. For example:

./run_full_benchmark.sh videofile.h264 [num_executions]

text

- Replace `videofile.h264` with your input video file.
- `[num_executions]` is optional and specifies how many times you want to run the algorithm in parallel. The default is 1 if you don't specify it.

During execution, you’ll be presented with options. If you select **option `0`**, the script will:
- Run all benchmarks.
- Generate charts.
- Create a PowerPoint presentation (PPT).

> **Note:** Selecting option 0 will take longer because it performs both the benchmarks and the full reporting.

Feel free to modify this template with more details about your project, usage examples, or specific instructions tailored to your workflow.

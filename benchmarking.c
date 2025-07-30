#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <errno.h>

#define N_METHODS 9

typedef struct {
    const char* name;
    const char* exe;
    const char* output_csv;
    int supports_high_profile;
} MethodInfo;

typedef struct {
    const char* name;
    double total_time_ms;
    double avg_time_per_frame_ms;
    double throughput_fps;
    double cpu_usage_percent;
    long memory_peak_kb;
    int total_motion_vectors;
    int frame_count;
    int supports_high_profile;
} BenchmarkResult;

MethodInfo methods[N_METHODS] = {
    {"FFMPEG decode frames", "./extractor6", "method6_output.csv", 1},
    {"FFmpeg MV", "./extractor0", "method0_output.csv", 1},
    {"Same Code Not Patched", "./extractor1", "method1_output.csv", 1},
    {"Optimized MV-Only - FFMPEG Patched", "./extractor2", "method2_output.csv", 1},
    {"Custom H.264 Parser", "./extractor3", "method3_output.csv", 0},
    {"LIVE555 Parser", "./extractor4", "method4_output.csv", 0},
    {"Python mv-extractor", "./extractor5", "method5_output.csv", 1},
    {"FFMPEG Patched - Minimal", "./extractor7", "method7_output.csv", 1},
    {"FFMPEG Patched!", "./extractor8", "method8_output.csv", 1}
};

static double now_ms() {
    struct timeval tv;
    if (gettimeofday(&tv, NULL) != 0) {
        perror("gettimeofday failed");
        return 0.0;
    }
    return 1000.0 * tv.tv_sec + tv.tv_usec / 1000.0;
}

static void parse_csv(const char *fname, int *frames, int *mvs) {
    FILE* f = fopen(fname, "r");
    if (!f) {
        fprintf(stderr, "Warning: cannot open CSV file '%s': %s\n", fname, strerror(errno));
        *frames = 0;
        *mvs = 0;
        return;
    }
    char line[2048];
    int last = -1;
    *frames = 0;
    *mvs = 0;
    if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        return;
    }
    while (fgets(line, sizeof(line), f)) {
        int frame = -1;
        if (sscanf(line, "%d", &frame) == 1) {
            (*mvs)++;
            if (frame != last) {
                (*frames)++;
                last = frame;
            }
        }
    }
    fclose(f);
}

BenchmarkResult run_benchmark_parallel(MethodInfo m, const char *input, int par_streams) {
    BenchmarkResult r = {0};
    r.name = m.name;
    r.supports_high_profile = m.supports_high_profile;

    printf("Starting %d parallel streams for method: %s\n", par_streams, m.name);

    double t_start = now_ms();

    pid_t *pids = calloc(par_streams, sizeof(pid_t));
    int *statuses = calloc(par_streams, sizeof(int));
    struct rusage *usage = calloc(par_streams, sizeof(struct rusage));
    if (!pids || !statuses || !usage) {
        perror("calloc failed");
        free(pids); free(statuses); free(usage);
        exit(1);
    }

    for (int i = 0; i < par_streams; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork failed");
            free(pids); free(statuses); free(usage);
            exit(1);
        } else if (pid == 0) {
            char csv_filename[256];
            snprintf(csv_filename, sizeof(csv_filename), "%s_%d.csv", m.output_csv, i);

            if (!freopen(csv_filename, "w", stdout)) {
                fprintf(stderr, "Child %d: freopen to '%s' failed: %s\n", i, csv_filename, strerror(errno));
                exit(1);
            }

            execl(m.exe, m.exe, input, NULL);

            // If execl returns, it failed
            fprintf(stderr, "Child %d: exec failed for command %s %s: %s\n", i, m.exe, input, strerror(errno));
            exit(127);
        } else {
            pids[i] = pid;
            printf("Forked child %d with pid %d\n", i, pid);
        }
    }

    for (int i = 0; i < par_streams; i++) {
        if (wait4(pids[i], &statuses[i], 0, &usage[i]) == -1) {
            perror("wait4 failed");
            statuses[i] = -1;
        } else {
            if (WIFEXITED(statuses[i])) {
                printf("Child %d (pid %d) exited with code %d\n", i, pids[i], WEXITSTATUS(statuses[i]));
            } else if (WIFSIGNALED(statuses[i])) {
                printf("Child %d (pid %d) killed by signal %d\n", i, pids[i], WTERMSIG(statuses[i]));
            } else {
                printf("Child %d (pid %d) ended abnormally\n", i, pids[i]);
            }
        }
    }

    double t_end = now_ms();
    printf("All children done; total wall time elapsed: %.2f ms\n", t_end - t_start);

    long max_rss_kb = 0;
    double total_user_cpu_sec = 0;
    int total_mvs = 0;

    for (int i = 0; i < par_streams; i++) {
        if (usage[i].ru_maxrss > max_rss_kb) {
            max_rss_kb = usage[i].ru_maxrss;
        }
        double u_sec = usage[i].ru_utime.tv_sec + usage[i].ru_utime.tv_usec / 1e6;
        total_user_cpu_sec += u_sec;

        char csv_filename[256];
        snprintf(csv_filename, sizeof(csv_filename), "%s_%d.csv", m.output_csv, i);
        int frames, mvs;
        parse_csv(csv_filename, &frames, &mvs);
        printf("Parsed file '%s': frames=%d, mvs=%d\n", csv_filename, frames, mvs);

        total_mvs += mvs;

        // Uncomment to delete CSV files after parsing:
        if (remove(csv_filename) != 0) {
             fprintf(stderr, "Warning: failed to remove file '%s'\n", csv_filename);
        }
    }

    free(pids);
    free(statuses);
    free(usage);

    // Fixed frames per stream as requested
    int fixed_frames_per_stream = 298;
    int total_frames = fixed_frames_per_stream * par_streams;

    r.total_time_ms = t_end - t_start;
    r.frame_count = total_frames;  // Fixed total frames
    r.total_motion_vectors = total_mvs; // sum from CSVs
    r.memory_peak_kb = max_rss_kb;
    r.cpu_usage_percent = (r.total_time_ms > 0) ? (total_user_cpu_sec / (r.total_time_ms / 1000.0)) * 100.0 : 0.0;

    r.avg_time_per_frame_ms = (total_frames > 0) ? (r.total_time_ms / total_frames) : 0;
    r.throughput_fps = (r.avg_time_per_frame_ms > 0) ? 1000.0 / r.avg_time_per_frame_ms : 0;

    return r;
}

void print_complete_results(BenchmarkResult r[N_METHODS], int par_streams) {
    printf("\n==========================================================================================================\n");
    printf("                                   COMPLETE MOTION VECTOR EXTRACTION BENCHMARK\n");
    printf("                              Streams per Method: %d\n", par_streams);
    printf("==========================================================================================================\n\n");
    printf("%-30s | %-12s | %-6s | %-10s | %-9s | %-12s | %-8s | %s\n",
           "Method", "Time/Frame", "FPS", "CPU Usage", "Mem Δ KB", "Total MVs", "Frames", "High Profile");
    printf("------------------------------------------------------------------------------------------------------------\n");

    for (int i = 0; i < N_METHODS; i++) {
        printf("%-30s | %10.2f ms | %6.1f | %8.1f%% | %9ld | %10d | %8d | %s\n",
               r[i].name, r[i].avg_time_per_frame_ms, r[i].throughput_fps,
               r[i].cpu_usage_percent, r[i].memory_peak_kb,
               r[i].total_motion_vectors, r[i].frame_count,
               r[i].supports_high_profile ? "✅" : "❌");
    }
}

int main(int argc, char **argv) {
    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Usage: %s <video_file_or_rtsp_url> [streams]\n", argv[0]);
        return 1;
    }
    const char* input = argv[1];
    int par_streams = 1;
    if (argc == 3)
        par_streams = atoi(argv[2]);
    if (par_streams < 1 || par_streams > 100) {
        fprintf(stderr, "Streams must be between 1 and 100.\n");
        return 1;
    }

    BenchmarkResult results[N_METHODS];
    printf("🔍 Starting benchmarking on: %s\n", input);
    printf("   Streams per method: %d\n\n", par_streams);

    for (int i = 0; i < N_METHODS; i++) {
        printf("▶️  Running: %s\n", methods[i].name);
        results[i] = run_benchmark_parallel(methods[i], input, par_streams);
        printf("✅ Done: %d frames, %.2f ms/frame, %.1f FPS\n\n",
               results[i].frame_count, results[i].avg_time_per_frame_ms, results[i].throughput_fps);
    }

    print_complete_results(results, par_streams);
    return 0;
}


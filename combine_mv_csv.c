#include <stdio.h>

int main() {
    FILE *out = fopen("all_motion_vectors.csv", "w");
    if (!out) {
        fprintf(stderr, "Failed to open all_motion_vectors.csv for writing.\n");
        return 1;
    }

    // Write CSV header
    fprintf(out, "frame,method_id,mb_x,mb_y,mvd_x,mvd_y\n");

    // Loop over method0_output.csv to method8_output.csv
    for (int i = 0; i < 9; i++) {
        char fname[64];
        snprintf(fname, sizeof(fname), "method%d_output.csv", i);

        FILE *in = fopen(fname, "r");
        if (!in) {
            fprintf(stderr, "Warning: missing %s\n", fname);
            continue;
        }

        char line[2048];
        // Skip header
        if (!fgets(line, sizeof(line), in)) {
            fclose(in);
            continue;
        }

        // Copy the rest
        while (fgets(line, sizeof(line), in)) {
            fputs(line, out);
        }

        fclose(in);
    }

    fclose(out);
    printf("✅ Combined CSV: all_motion_vectors.csv created.\n");
    return 0;
}


// extractor5.c
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv){
    if(argc != 2){
        fprintf(stderr, "Usage: %s <file_or_rtsp_url>\n", argv[0]);
        return 1;
    }

    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "python3 extractors/mv_python_extractor5.py \"%s\"",
             argv[1]);

    return system(cmd);
}


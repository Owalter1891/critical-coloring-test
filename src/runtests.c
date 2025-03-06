#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>

#define MAX_FILES 100
#define MAX_LINE 1024
#define MAX_CMD 512
#define NUM_RUNS 1
#define MAX_TOKENS 64
#define TIME_LIMIT 1
#define NUM_INSTANCES 70

// Structure to hold key-value pairs parsed from the summary line.
typedef struct {
    char key[64];
    char value[128];
} KeyValue;

typedef struct {
    char name[50];
    int k_value;
} Instance;

// List of header keys in the desired order.
// Note: "time_confirm_chroma" is not included because the output file header expects "time_confirm_crit".
const char *header_keys[] = {
    "instance", "k", "n_red", "m_red", "n", "m",
    "heu_timelimit", "exact_timelimit", "do_confirm_crit", "confirm_timelimit",
    "R", "reltg_size", "do_heu", "ss", "ss_gen_1st", "ss_fin_1st",
    "chroma_k_early", "chroma_k", "crit_early", "crit", "edges",
    "time", "time_pp", "time_confirm_crit", "time_color", "iter",
    "ttb", "itb", "max_iter_diff", "ss_fin_avg", "ss_gen_avg",
    "ss_gen_best", "num_size_att", "heu_mistakes", "num_pp",
    "avg_pp_reduced", "avg_pp_skipped", "calls_to_coloring",
    "num_gen_subsets", "infeas", "clique_start", "color_not_ok", "seed"
};
const int num_header_keys = sizeof(header_keys) / sizeof(header_keys[0]);

Instance instances[NUM_INSTANCES] = {
        {"1-Insertions_4.col", 5}, {"2-Insertions_3.col", 4}, {"2-Insertions_4.col", 5},
        {"3-Insertions_3.col", 4}, {"3-Insertions_4.col", 5}, {"4-Insertions_3.col", 4},
        {"mug100_1.col", 4}, {"mug100_25.col", 4}, {"mug88_1.col", 4}, {"mug88_25.col", 4},
        {"myciel2.col", 3}, {"myciel3.col", 4}, {"myciel4.col", 5}, {"myciel5.col", 6},
        {"myciel6.col", 7}, {"queen10_10.col", 11}, {"DSJC125.5.col", 14}, {"DSJC250.5.col", 14},
        {"DSJR500.1.col", 6}, {"DSJR500.1c.col", 80}, {"DSJR500.5.col", 90},
        {"1-FullIns_3.col", 4}, {"1-FullIns_4.col", 5}, {"1-FullIns_5.col", 6},
        {"2-FullIns_3.col", 5}, {"2-FullIns_4.col", 6}, {"2-FullIns_5.col", 7},
        {"3-FullIns_3.col", 6}, {"3-FullIns_4.col", 7}, {"3-FullIns_5.col", 8},
        {"4-FullIns_3.col", 7}, {"4-FullIns_4.col", 8}, {"4-FullIns_5.col", 7},
        {"5-FullIns_3.col", 8}, {"5-FullIns_4.col", 9}, {"1-Insertions_5.col", 6},
        {"1-Insertions_6.col", 7}, {"2-Insertions_5.col", 6}, {"3-Insertions_5.col", 6},
        {"4-Insertions_4.col", 5}, {"ash331GPIA.col", 4}, {"ash608GPIA.col", 4},
        {"ash958GPIA.col", 4}, {"flat1000_50_0.col", 50}, {"flat1000_60_0.col", 60},
        {"flat1000_76_0.col", 76}, {"flat300_20_0.col", 20}, {"flat300_26_0.col", 26},
        {"flat300_28_0.col", 28}, {"myciel7.col", 8}, {"queen6_6.col", 7}, {"queen8_8.col", 9},
        {"queen9_9.col", 10}, {"r1000.1c.col", 96}, {"DSJC125.1.col", 5}, {"DSJC125.9.col", 44},
        {"DSJC250.1.col", 6}, {"DSJC250.9.col", 72}, {"DSJC500.1.col", 9}, {"DSJC500.5.col", 43}, 
        {"DSJC500.9.col", 123}, {"DSJC1000.1.col", 9}, {"DSJC1000.5.col", 73}, {"DSJC1000.9.col", 216},
        {"C2000.5.col", 99}, {"C4000.5.col", 107}, {"will199GPIA.col", 7}
    };

// Helper function to trim newline characters
void trim_newline(char *str) {
    size_t len = strlen(str);
    if(len > 0 && (str[len-1]=='\n' || str[len-1]=='\r')) {
        str[len-1] = '\0';
    }
}

// Parse a summary line (a string of space-separated tokens like key=value)
// and fill tokens[] with the extracted key-value pairs.
// Returns the number of tokens parsed.
int parse_summary_line(const char *line, KeyValue tokens[], int max_tokens) {
    int count = 0;
    // Make a copy of line to tokenize
    char buffer[MAX_LINE];
    strncpy(buffer, line, MAX_LINE-1);
    buffer[MAX_LINE-1] = '\0';

    // Tokenize by spaces
    char *token = strtok(buffer, " ");
    while (token != NULL && count < max_tokens) {
        // Look for an '=' in the token
        char *eq = strchr(token, '=');
        if (eq != NULL) {
            size_t key_len = eq - token;
            strncpy(tokens[count].key, token, key_len);
            tokens[count].key[key_len] = '\0';
            strncpy(tokens[count].value, eq + 1, sizeof(tokens[count].value) - 1);
            tokens[count].value[sizeof(tokens[count].value)-1] = '\0';
            count++;
        }
        token = strtok(NULL, " ");
    }
    return count;
}

// Retrieve a value from tokens[] given a key; returns pointer to value if found, else "NA"
const char *get_token_value(const char *key, KeyValue tokens[], int num_tokens) {
    for (int i = 0; i < num_tokens; i++) {
        if (strcmp(tokens[i].key, key) == 0) {
            return tokens[i].value;
        }
    }
    return "NA";
}

int main(void) {
    DIR *dir;
    struct dirent *ent;
    int file_index = 0;
    const char *dir_path = "../data/";
    char filepath[MAX_CMD];

    // Open the output .dat file for writing results
    FILE *out_fp = fopen("results.dat", "w");
    if (!out_fp) {
        perror("Error opening results.dat for writing");
        return EXIT_FAILURE;
    }

    // Write header line to the output file
    for (int i = 0; i < num_header_keys; i++) {
        fprintf(out_fp, "%-20s", header_keys[i]); // Left-align headers with a fixed width
    }
    fprintf(out_fp, "\n");

    // Open the directory containing data files.
    dir = opendir(dir_path);
    if (dir == NULL) {
        perror("Unable to open directory");
        fclose(out_fp);
        return EXIT_FAILURE;
    }

    // Iterate over each file in the directory.
    while ((ent = readdir(dir)) != NULL) {
        // Skip non-regular files and hidden entries (like . and ..)
        if (ent->d_type != DT_REG)
            continue;
        if (ent->d_name[0] == '.')
            continue;

        // Construct full file path
        snprintf(filepath, sizeof(filepath), "%s%s", dir_path, ent->d_name);

        // Find the corresponding k_value for this file
        int k_value = 0; // Default if no match is found
        for (int i = 0; i < NUM_INSTANCES; i++) {
            if (strcmp(ent->d_name, instances[i].name) == 0) {
                k_value = instances[i].k_value;
                break;
            }
        }

        // Run the command NUM_RUNS times for each file.
        for (int run = 0; run < NUM_RUNS; run++) {
            char cmd[MAX_CMD];
            // Build the command string.
            // Example: "./critcol -i ../data/FILE_NAME -k K_VALUE -t 1800"
           snprintf(cmd, sizeof(cmd), "./critcol -i %s -k %d -t %d 2>&1", filepath, k_value, TIME_LIMIT);

            // Use popen to run the command and capture its output.
            FILE *pipe = popen(cmd, "r");
            if (!pipe) {
                fprintf(stderr, "Error executing command: %s\n", cmd);
                continue;
            }

            // Read one line of output (the summary line)
            char output_line[MAX_LINE];

            //Skip first line
            fgets(output_line, sizeof(output_line), pipe);
            if (fgets(output_line, sizeof(output_line), pipe) != NULL) {
                printf("Testing %s attempt no %d \n", filepath, (run+1));
                fflush(stdout);
                trim_newline(output_line);

                // Parse the output line into key-value tokens.
                KeyValue tokens[MAX_TOKENS];
                int num_tokens = parse_summary_line(output_line, tokens, MAX_TOKENS);

                // For each header key, print the corresponding value (or NA if not found).
                for (int i = 0; i < num_header_keys; i++) {
                    const char *val = get_token_value(header_keys[i], tokens, num_tokens);
                    if (strcmp(val, "NA") == 0) {
                        fprintf(out_fp, "%-20s", "NA");  // Ensure NA values are aligned
                    } else {
                        fprintf(out_fp, "%-20s", val);   // Use fixed width for alignment
                    }
                }
                fprintf(out_fp, "\n");
            } else {
                fprintf(stderr, "No output from command: %s\n", cmd);
            }
            pclose(pipe);
        }
        file_index++;
    }
    closedir(dir);
    fclose(out_fp);

    printf("Results written to results.dat\n");
    return EXIT_SUCCESS;
}

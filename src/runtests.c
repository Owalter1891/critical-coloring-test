#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>

#define MAX_FILES 100
#define MAX_LINE 1024
#define MAX_CMD 512
#define NUM_RUNS 5
#define MAX_TOKENS 64
#define TIME_LIMIT 100

// Structure to hold key-value pairs parsed from the summary line.
typedef struct {
    char key[64];
    char value[128];
} KeyValue;

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

// A static array mapping each file (by order) to its k value.
// Adjust these values as needed.
int k_values[MAX_FILES] = {
    4,  
    5,  
    6,
    5,
    6,
    7,
    5,
    6,
    7,
    4,
    5,
    6,
    7,
    8,
    4,
    5,
    6,
    7,
    8,
    7,
    4,
    5,
    8,
    9,
    4,
    4,
    4,
    99,
    107,
    5,
    17,
    44,
    6,
    26,
    72,
    9,
    43,
    123,
    9,
    73,
    216,
    6,
    80,
    90,
    8,
    7,
    9,
    10,
    11,
    96,
    7,
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

        // Get the k value for this file from the static array.
        // If k_values is not set for this file, default to 0.
        int k_value = (file_index < MAX_FILES) ? k_values[file_index] : 0;

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

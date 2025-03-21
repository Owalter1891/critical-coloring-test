#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>

#define MAX_FILES 100
#define MAX_LINE 1024
#define MAX_CMD 512
#define NUM_RUNS 8
#define MAX_TOKENS 64
#define TIME_LIMIT 1800
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

// Retrieve a value from tokens[] given a key; returns pointer to value if found, else "NA"
const char *get_token_value(const char *key, KeyValue tokens[], int num_tokens) {
    for (int i = 0; i < num_tokens; i++) {
        if (strcmp(tokens[i].key, key) == 0) {
            return tokens[i].value;
        }
    }
    return "NA";
}

// Parse a summary line (a string of space-separated tokens like key=value)
// and fill tokens[] with the extracted key-value pairs.
// Returns the number of tokens parsed.
int parse_summary_line(const char *line, KeyValue tokens[], int max_tokens) {
    int count = 0;
    char buffer[MAX_LINE];
    strncpy(buffer, line, MAX_LINE-1);
    buffer[MAX_LINE-1] = '\0';

    char *token = strtok(buffer, " ");
    while (token != NULL && count < max_tokens) {
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

// Define a structure for passing thread arguments.
typedef struct {
    int run_index;
    const char *instance_name;
    int k_value;
    int run_duration;
    char *result; // Buffer to store the output line.
} ThreadArg;

    // Semaphore to limit the number of concurrent threads to 10.
sem_t sem;

// Thread function.
void *thread_func(void *arg) {
    ThreadArg *targ = (ThreadArg *) arg;
    char cmd[MAX_CMD];
    snprintf(cmd, sizeof(cmd), "./critcol -i ../data/%s -k %d -t %d 2>&1",
             targ->instance_name, targ->k_value, targ->run_duration);
    FILE *pipe = popen(cmd, "r");
    if (!pipe) {
        fprintf(stderr, "Error executing command: %s\n", cmd);
        strcpy(targ->result, "NA");
    } else {
        char output_line[MAX_LINE];
        // Skip the first line.
        fgets(output_line, sizeof(output_line), pipe);
        if (fgets(output_line, sizeof(output_line), pipe) != NULL) {
            trim_newline(output_line);
            strncpy(targ->result, output_line, MAX_LINE-1);
            targ->result[MAX_LINE-1] = '\0';
        } else {
            fprintf(stderr, "No output from command: %s\n", cmd);
            strcpy(targ->result, "NA");
        }
        pclose(pipe);
    }
    sem_post(&sem); // Signal that this thread has finished.
    return NULL;
}

void run_instance_multiple_times_parallel(const char *instance_name, int num_runs, int run_duration) {
    int k_value = 0;
    for (int i = 0; i < NUM_INSTANCES; i++) {
        if (strcmp(instance_name, instances[i].name) == 0) {
            k_value = instances[i].k_value;
            break;
        }
    }

    // Allocate an array to store each run's result.
    char **results = malloc(num_runs * sizeof(char *));
    for (int i = 0; i < num_runs; i++) {
        results[i] = malloc(MAX_LINE);
        results[i][0] = '\0';
    }

    ThreadArg *thread_args = malloc(num_runs * sizeof(ThreadArg));
    pthread_t *threads = malloc(num_runs * sizeof(pthread_t));

    sem_init(&sem, 0, 10);

    // Create threads, waiting on the semaphore to allow up to 10 concurrent threads.
    for (int i = 0; i < num_runs; i++) {
        sem_wait(&sem); // Ensure no more than 10 threads are running concurrently.
        thread_args[i].run_index = i;
        thread_args[i].instance_name = instance_name;
        thread_args[i].k_value = k_value;
        thread_args[i].run_duration = run_duration;
        thread_args[i].result = results[i];
        if (pthread_create(&threads[i], NULL, thread_func, &thread_args[i]) != 0) {
            fprintf(stderr, "Error creating thread for run %d\n", i);
            strcpy(results[i], "NA");
            sem_post(&sem);
        }
    }

    // Wait for all threads to complete.
    for (int i = 0; i < num_runs; i++) {
        pthread_join(threads[i], NULL);
    }

    sem_destroy(&sem);
    free(thread_args);
    free(threads);

    // Write the results (and compute averages) to file.
    FILE *out_fp = fopen("testResults.dat", "w");
    if (!out_fp) {
        perror("Error opening testResults.dat for writing");
        for (int i = 0; i < num_runs; i++) {
            free(results[i]);
        }
        free(results);
        return;
    }

    // Write header line.
    for (int i = 0; i < num_header_keys; i++) {
        fprintf(out_fp, "%-20s", header_keys[i]);
    }
    fprintf(out_fp, "\n");

    double sums[num_header_keys];  // For averaging.
    int valid_counts[num_header_keys]; // Numeric counts per column.
    memset(sums, 0, sizeof(sums));
    memset(valid_counts, 0, sizeof(valid_counts));

    // Process each run's result.
    for (int i = 0; i < num_runs; i++) {
        KeyValue tokens[MAX_TOKENS];
        int num_tokens = parse_summary_line(results[i], tokens, MAX_TOKENS);
        for (int j = 0; j < num_header_keys; j++) {
            const char *val = get_token_value(header_keys[j], tokens, num_tokens);
            if (strcmp(val, "NA") == 0) {
                fprintf(out_fp, "%-20s", "NA");
            } else {
                fprintf(out_fp, "%-20s", val);
                char *endptr;
                double num = strtod(val, &endptr);
                if (endptr != val) {
                    sums[j] += num;
                    valid_counts[j]++;
                }
            }
        }
        fprintf(out_fp, "\n");
    }

    // Append the averages as the final row.
    fprintf(out_fp, "%-20s", "AVG");
    for (int i = 1; i < num_header_keys; i++) { // Skip "instance" column.
        if (valid_counts[i] > 0) {
            fprintf(out_fp, "%-20.2f", sums[i] / valid_counts[i]);
        } else {
            fprintf(out_fp, "%-20s", "NA");
        }
    }
    fprintf(out_fp, "\n");

    fclose(out_fp);

    // Clean up allocated memory.
    for (int i = 0; i < num_runs; i++) {
        free(results[i]);
    }
    free(results);
}

typedef struct {
    int instance_index;
    int run_index;
    char filename[256];    // Name of the instance file (e.g. "myciel3.col")
    int k_value;
    int run_duration;
    char *result;          // Buffer to store the output line.
} AllInstanceThreadArg;

sem_t sem;  // Global semaphore to limit concurrency to 10 threads.

void *thread_func_all(void *arg) {
    AllInstanceThreadArg *targ = (AllInstanceThreadArg *) arg;
    char filepath[512];
    // Construct full file path.
    snprintf(filepath, sizeof(filepath), "../data/%s", targ->filename);
    char cmd[MAX_CMD];
    snprintf(cmd, sizeof(cmd), "./critcol -i %s -k %d -t %d 2>&1",
             filepath, targ->k_value, targ->run_duration);
    FILE *pipe = popen(cmd, "r");
    if (!pipe) {
        fprintf(stderr, "Error executing command: %s\n", cmd);
        strcpy(targ->result, "NA");
    } else {
        char output_line[MAX_LINE];
        // Skip the first line.
        fgets(output_line, sizeof(output_line), pipe);
        if (fgets(output_line, sizeof(output_line), pipe) != NULL) {
            trim_newline(output_line);
            strncpy(targ->result, output_line, MAX_LINE-1);
            targ->result[MAX_LINE-1] = '\0';
        } else {
            fprintf(stderr, "No output from command: %s\n", cmd);
            strcpy(targ->result, "NA");
        }
        pclose(pipe);
    }
    sem_post(&sem); // Signal completion.
    return NULL;
}

// --- Modified run_all_instances function with parallelisation ---
// This function reads all instance files from ../data/,
// then for each instance runs NUM_RUNS runs in parallel (max 10 threads concurrently),
// and finally writes each instance’s run results followed by an average row to results.dat.
int run_all_instances(void) {
    const char *dir_path = "../data/";
    DIR *dir = opendir(dir_path);
    if (!dir) {
        perror("Unable to open directory");
        return EXIT_FAILURE;
    }

    // First, count valid files.
    int file_count = 0;
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_type != DT_REG)
            continue;
        if (ent->d_name[0] == '.')
            continue;
        file_count++;
    }

    // Allocate arrays to store file names and corresponding k_values.
    char **file_names = malloc(file_count * sizeof(char *));
    int *k_values = malloc(file_count * sizeof(int));
    rewinddir(dir);
    int idx = 0;
    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_type != DT_REG)
            continue;
        if (ent->d_name[0] == '.')
            continue;
        file_names[idx] = strdup(ent->d_name);
        // Look up k_value from the instances array.
        int k_val = 0;
        for (int i = 0; i < NUM_INSTANCES; i++) {
            if (strcmp(ent->d_name, instances[i].name) == 0) {
                k_val = instances[i].k_value;
                break;
            }
        }
        k_values[idx] = k_val;
        idx++;
    }
    closedir(dir);

    // Allocate a 2D array to store results for each instance and each run.
    // results[i][j] will store the output for file_names[i] run j.
    char ***results = malloc(file_count * sizeof(char **));
    for (int i = 0; i < file_count; i++) {
        results[i] = malloc(NUM_RUNS * sizeof(char *));
        for (int j = 0; j < NUM_RUNS; j++) {
            results[i][j] = malloc(MAX_LINE);
            results[i][j][0] = '\0';
        }
    }

    int total_tasks = file_count * NUM_RUNS;
    AllInstanceThreadArg *thread_args = malloc(total_tasks * sizeof(AllInstanceThreadArg));
    pthread_t *threads = malloc(total_tasks * sizeof(pthread_t));

    sem_init(&sem, 0, 10);  // Allow up to 10 threads concurrently.

    // Launch threads for each run of each instance.
    int task_index = 0;
    for (int i = 0; i < file_count; i++) {
        for (int j = 0; j < NUM_RUNS; j++) {
            thread_args[task_index].instance_index = i;
            thread_args[task_index].run_index = j;
            strncpy(thread_args[task_index].filename, file_names[i],
                    sizeof(thread_args[task_index].filename) - 1);
            thread_args[task_index].filename[sizeof(thread_args[task_index].filename)-1] = '\0';
            thread_args[task_index].k_value = k_values[i];
            thread_args[task_index].run_duration = TIME_LIMIT;
            thread_args[task_index].result = results[i][j];
            sem_wait(&sem); // Wait if already 10 threads running.
            if (pthread_create(&threads[task_index], NULL, thread_func_all,
                               &thread_args[task_index]) != 0) {
                fprintf(stderr, "Error creating thread for file %s run %d\n",
                        file_names[i], j);
                strcpy(results[i][j], "NA");
                sem_post(&sem);
            }
            task_index++;
        }
    }

    // Wait for all threads to complete.
    for (int t = 0; t < total_tasks; t++) {
        pthread_join(threads[t], NULL);
    }
    sem_destroy(&sem);

    // Open output file.
    FILE *out_fp = fopen("results.dat", "w");
    if (!out_fp) {
        perror("Error opening results.dat for writing");
        // Clean up before returning.
        for (int i = 0; i < file_count; i++) {
            for (int j = 0; j < NUM_RUNS; j++) {
                free(results[i][j]);
            }
            free(results[i]);
            free(file_names[i]);
        }
        free(results);
        free(file_names);
        free(k_values);
        free(thread_args);
        free(threads);
        return EXIT_FAILURE;
    }

    // Write header line.
    for (int i = 0; i < num_header_keys; i++) {
        fprintf(out_fp, "%-20s", header_keys[i]);
    }
    fprintf(out_fp, "\n");

    // Process each instance: write all runs then an average row.
    for (int i = 0; i < file_count; i++) {
        double sums[num_header_keys];
        int valid_counts[num_header_keys];
        memset(sums, 0, sizeof(sums));
        memset(valid_counts, 0, sizeof(valid_counts));

        for (int j = 0; j < NUM_RUNS; j++) {
            KeyValue tokens[MAX_TOKENS];
            int num_tokens = parse_summary_line(results[i][j], tokens, MAX_TOKENS);
            for (int col = 0; col < num_header_keys; col++) {
                const char *val = get_token_value(header_keys[col], tokens, num_tokens);
                if (strcmp(val, "NA") == 0) {
                    fprintf(out_fp, "%-20s", "NA");
                } else {
                    fprintf(out_fp, "%-20s", val);
                    char *endptr;
                    double num = strtod(val, &endptr);
                    if (endptr != val) {
                        sums[col] += num;
                        valid_counts[col]++;
                    }
                }
            }
            fprintf(out_fp, "\n");
        }
        // Write average row for this instance.
        fprintf(out_fp, "%-20s", "AVG");
        for (int col = 1; col < num_header_keys; col++) { // Skip the "instance" column.
            if (valid_counts[col] > 0)
                fprintf(out_fp, "%-20.2f", sums[col] / valid_counts[col]);
            else
                fprintf(out_fp, "%-20s", "NA");
        }
        fprintf(out_fp, "\n");
    }
    fclose(out_fp);
    printf("Results written to results.dat\n");

    // Clean up allocated memory.
    for (int i = 0; i < file_count; i++) {
        for (int j = 0; j < NUM_RUNS; j++) {
            free(results[i][j]);
        }
        free(results[i]);
        free(file_names[i]);
    }
    free(results);
    free(file_names);
    free(k_values);
    free(thread_args);
    free(threads);

    return EXIT_SUCCESS;
}

void main(void){
    run_all_instances();
}
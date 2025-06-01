#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include <omp.h>

#define MAX_GENERATIONS 10000
#define SAMPLES_PER_CONFIG 100000

typedef struct
{
    int size;
    unsigned char **grid;
    unsigned char **next;
    unsigned char **prev1;
    unsigned char **prev2;
} Board;

// Allocate memory for a board
Board *create_board(int size)
{
    Board *board = malloc(sizeof(Board));
    board->size = size;

    board->grid = malloc(size * sizeof(unsigned char *));
    board->next = malloc(size * sizeof(unsigned char *));
    board->prev1 = malloc(size * sizeof(unsigned char *));
    board->prev2 = malloc(size * sizeof(unsigned char *));

    for (int i = 0; i < size; i++)
    {
        board->grid[i] = calloc(size, sizeof(unsigned char));
        board->next[i] = calloc(size, sizeof(unsigned char));
        board->prev1[i] = calloc(size, sizeof(unsigned char));
        board->prev2[i] = calloc(size, sizeof(unsigned char));
    }

    return board;
}

// Free board memory
void free_board(Board *board)
{
    for (int i = 0; i < board->size; i++)
    {
        free(board->grid[i]);
        free(board->next[i]);
        free(board->prev1[i]);
        free(board->prev2[i]);
    }
    free(board->grid);
    free(board->next);
    free(board->prev1);
    free(board->prev2);
    free(board);
}

// Initialize board with random cells
void randomize_board(Board *board, double density, unsigned int seed)
{
    // Use a local random state to avoid thread conflicts
    for (int i = 0; i < board->size; i++)
    {
        for (int j = 0; j < board->size; j++)
        {
            seed = seed * 1103515245 + 12345; // Simple LCG
            board->grid[i][j] = ((seed / 65536) % 32768) / 32768.0 < density ? 1 : 0;
        }
    }
}

// Count living neighbors with wrapping
int count_neighbors(Board *board, int row, int col)
{
    int count = 0;
    int size = board->size;

    for (int dr = -1; dr <= 1; dr++)
    {
        for (int dc = -1; dc <= 1; dc++)
        {
            if (dr == 0 && dc == 0)
                continue;

            int r = (row + dr + size) % size;
            int c = (col + dc + size) % size;

            count += board->grid[r][c];
        }
    }

    return count;
}

// Evolve the board one generation
void evolve(Board *board)
{
    for (int i = 0; i < board->size; i++)
    {
        for (int j = 0; j < board->size; j++)
        {
            int neighbors = count_neighbors(board, i, j);

            if (board->grid[i][j])
            {
                // Living cell
                board->next[i][j] = (neighbors == 2 || neighbors == 3) ? 1 : 0;
            }
            else
            {
                // Dead cell
                board->next[i][j] = (neighbors == 3) ? 1 : 0;
            }
        }
    }

    // Swap grids
    unsigned char **temp = board->grid;
    board->grid = board->next;
    board->next = temp;
}

// Check if current state matches a previous state
bool matches_state(unsigned char **grid1, unsigned char **grid2, int size)
{
    for (int i = 0; i < size; i++)
    {
        for (int j = 0; j < size; j++)
        {
            if (grid1[i][j] != grid2[i][j])
            {
                return false;
            }
        }
    }
    return true;
}

// Copy grid state
void copy_state(unsigned char **src, unsigned char **dst, int size)
{
    for (int i = 0; i < size; i++)
    {
        memcpy(dst[i], src[i], size * sizeof(unsigned char));
    }
}

// Run simulation until stable state
int run_until_stable(Board *board)
{
    int generation = 0;

    // Initialize previous states
    copy_state(board->grid, board->prev1, board->size);
    copy_state(board->grid, board->prev2, board->size);

    while (generation < MAX_GENERATIONS)
    {
        evolve(board);
        generation++;

        // Check for period-1 stability
        if (matches_state(board->grid, board->prev1, board->size))
        {
            return generation;
        }

        // Check for period-2 stability
        if (matches_state(board->grid, board->prev2, board->size))
        {
            return generation;
        }

        // Update previous states
        copy_state(board->prev1, board->prev2, board->size);
        copy_state(board->grid, board->prev1, board->size);
    }

    return MAX_GENERATIONS;
}

// Compare function for qsort
int compare_int(const void *a, const void *b)
{
    return *(int *)a - *(int *)b;
}

// Calculate percentile
int percentile(int *data, int n, int p)
{
    int index = (p * n) / 100;
    if (index >= n)
        index = n - 1;
    return data[index];
}

int main()
{
    srand(time(NULL));

    int board_sizes[] = {8, 16, 32, 64, 128, 256};
    int num_sizes = sizeof(board_sizes) / sizeof(board_sizes[0]);

    printf("Board Size,Density,P10,Median,Mean,Outliers Removed,Samples\n");

    // Set number of threads (optional - OpenMP will use system default if not set)
    // omp_set_num_threads(8);

    for (int size_idx = 0; size_idx < num_sizes; size_idx++)
    {
        int size = board_sizes[size_idx];

        // Test densities from 20% to 50% in 1% increments
        for (int density_pct = 20; density_pct <= 50; density_pct += 1)
        {
            double density = density_pct / 100.0;
            int generations[SAMPLES_PER_CONFIG];
            long long total_generations = 0;
            int outlier_count = 0;

// Parallel region for running samples
#pragma omp parallel
            {
                // Each thread needs its own board
                Board *board = create_board(size);
                unsigned int thread_seed = rand() + omp_get_thread_num() * 1000;

#pragma omp for reduction(+ : total_generations) reduction(+ : outlier_count)
                for (int sample = 0; sample < SAMPLES_PER_CONFIG; sample++)
                {
                    // Use a unique seed for each sample
                    unsigned int sample_seed = thread_seed + sample * 100000;
                    randomize_board(board, density, sample_seed);
                    generations[sample] = run_until_stable(board);
                    if (generations[sample] == MAX_GENERATIONS)
                    {
                        outlier_count++;
                    }
                    else
                    {
                        total_generations += generations[sample];
                    }
                }

                free_board(board);
            }

            // Filter out outliers (MAX_GENERATIONS values)
            int filtered_generations[SAMPLES_PER_CONFIG];
            int valid_count = 0;
            for (int i = 0; i < SAMPLES_PER_CONFIG; i++)
            {
                if (generations[i] < MAX_GENERATIONS)
                {
                    filtered_generations[valid_count++] = generations[i];
                }
            }

            if (valid_count == 0)
            {
                // All samples hit MAX_GENERATIONS
                printf("%d,%d%%,%d,%d,%.1f,%d,%d\n",
                       size, density_pct, MAX_GENERATIONS, MAX_GENERATIONS,
                       (double)MAX_GENERATIONS, outlier_count, SAMPLES_PER_CONFIG);
                fflush(stdout);
                continue;
            }

            // Sort for percentile calculation
            qsort(filtered_generations, valid_count, sizeof(int), compare_int);

            int p10 = percentile(filtered_generations, valid_count, 10);
            int median = percentile(filtered_generations, valid_count, 50);
            double mean = (double)total_generations / valid_count;

            // Output results immediately
            printf("%d,%d%%,%d,%d,%.1f,%d,%d\n",
                   size, density_pct, p10, median, mean, outlier_count, SAMPLES_PER_CONFIG);
            fflush(stdout);
        }
    }

    return 0;
}

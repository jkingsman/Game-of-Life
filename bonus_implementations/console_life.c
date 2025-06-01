#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// ==================== Configuration ====================
#define BOARD_WIDTH 32
#define BOARD_HEIGHT 32
#define FRAME_DELAY_MS 100

// Fade effect parameters
#define FADE_LEVELS 8
// #define FADE_CHAR_FULL "██"
// #define FADE_CHAR_HIGH "▒▒"
// #define FADE_CHAR_MED "▒▒"
// #define FADE_CHAR_LOW "░░"
// #define FADE_CHAR_NONE "  "

#define FADE_CHAR_FULL "● "
#define FADE_CHAR_HIGH "· "
#define FADE_CHAR_MED "  "
#define FADE_CHAR_LOW "  "
#define FADE_CHAR_NONE "  "

// Stability detection parameters
#define STABLE_GENERATIONS 10
#define HISTORY_SIZE 3

// ==================== Terminal Control ====================
#define ANSI_CLEAR_SCREEN "\033[2J"
#define ANSI_CURSOR_HOME "\033[H"
#define ANSI_CLEAR_LINE "\033[K"
#define ANSI_CLEAR_BELOW "\033[J"

// ==================== Board Access Macros ====================
// Calculate buffer size (8 cells per byte)
#define BUFFER_SIZE ((BOARD_WIDTH * BOARD_HEIGHT + 7) / 8)

// Wrap coordinates to create toroidal topology
#define WRAP_X(x) (((x) + BOARD_WIDTH) % BOARD_WIDTH)
#define WRAP_Y(y) (((y) + BOARD_HEIGHT) % BOARD_HEIGHT)

// Convert 2D coordinates to linear index
#define COORD_TO_IDX(x, y) (WRAP_Y(y) * BOARD_WIDTH + WRAP_X(x))

// Bit manipulation for cell access
#define BYTE_INDEX(idx) ((idx) >> 3)
#define BIT_MASK(idx) (1 << ((idx) & 7))

#define GET_CELL(board, x, y) \
    ((board[BYTE_INDEX(COORD_TO_IDX(x, y))] & BIT_MASK(COORD_TO_IDX(x, y))) != 0)

#define SET_CELL(board, x, y) \
    (board[BYTE_INDEX(COORD_TO_IDX(x, y))] |= BIT_MASK(COORD_TO_IDX(x, y)))

#define CLEAR_CELL(board, x, y) \
    (board[BYTE_INDEX(COORD_TO_IDX(x, y))] &= ~BIT_MASK(COORD_TO_IDX(x, y)))

// ==================== Type Definitions ====================
typedef unsigned char cell_buffer_t;

typedef struct
{
    cell_buffer_t *current;
    cell_buffer_t *next;
    unsigned char *fade_buffer;
    int population_history[HISTORY_SIZE];
    int stable_count;
    int generation;
} game_state_t;

// ==================== Function Prototypes ====================
static void init_game_state(game_state_t *state);
static void cleanup_game_state(game_state_t *state);
static void randomize_board(cell_buffer_t *board);
static int count_neighbors(const cell_buffer_t *board, int x, int y);
static void update_board(const cell_buffer_t *current, cell_buffer_t *next);
static int count_population(const cell_buffer_t *board);
static void sleep_ms(int milliseconds);
static void display_board_fade(const cell_buffer_t *board, unsigned char *fade_buffer);
static void reset_fade_buffer(unsigned char *fade_buffer, const cell_buffer_t *board);
static int check_stability(int *history, int current_pop);
static void handle_stable_state(game_state_t *state);

// ==================== Main Function ====================
int main(void)
{
    game_state_t game;

    // Initialize game state
    init_game_state(&game);
    if (!game.current || !game.next)
    {
        fprintf(stderr, "Failed to initialize game state\n");
        return 1;
    }

    // Seed random number generator
    srand((unsigned int)time(NULL));

    // Clear screen once at start
    printf(ANSI_CLEAR_SCREEN);

    // Initialize board with random pattern
    randomize_board(game.current);
    reset_fade_buffer(game.fade_buffer, game.current);

    // Main game loop
    while (1)
    {
        // Display current state
        display_board_fade(game.current, game.fade_buffer);

        // Check for stable states and handle resets
        handle_stable_state(&game);

        // Update to next generation
        update_board(game.current, game.next);

        // Swap buffers
        cell_buffer_t *temp = game.current;
        game.current = game.next;
        game.next = temp;

        // Frame delay
        sleep_ms(FRAME_DELAY_MS);
    }

    // Cleanup (unreachable in this example)
    cleanup_game_state(&game);
    return 0;
}

// ==================== Game State Management ====================
static void init_game_state(game_state_t *state)
{
    state->current = calloc(BUFFER_SIZE, sizeof(cell_buffer_t));
    state->next = calloc(BUFFER_SIZE, sizeof(cell_buffer_t));
    state->fade_buffer = calloc(BOARD_WIDTH * BOARD_HEIGHT, sizeof(unsigned char));
    memset(state->population_history, 0, sizeof(state->population_history));
    state->stable_count = 0;
    state->generation = 0;
}

static void cleanup_game_state(game_state_t *state)
{
    free(state->current);
    free(state->next);
    free(state->fade_buffer);
}

// ==================== Board Operations ====================
static void randomize_board(cell_buffer_t *board)
{
    for (int i = 0; i < BUFFER_SIZE; i++)
    {
        board[i] = (cell_buffer_t)(rand() & 0xFF);
    }
}

// Count living neighbors using Moore neighborhood
static int count_neighbors(const cell_buffer_t *board, int x, int y)
{
    static const int dx[] = {-1, 0, 1, -1, 1, -1, 0, 1};
    static const int dy[] = {-1, -1, -1, 0, 0, 1, 1, 1};

    int count = 0;
    for (int i = 0; i < 8; i++)
    {
        count += GET_CELL(board, x + dx[i], y + dy[i]);
    }

    return count;
}

// Apply Conway's rules to generate next board state
static void update_board(const cell_buffer_t *current, cell_buffer_t *next)
{
    // Clear next board
    memset(next, 0, BUFFER_SIZE);

    for (int y = 0; y < BOARD_HEIGHT; y++)
    {
        for (int x = 0; x < BOARD_WIDTH; x++)
        {
            int neighbors = count_neighbors(current, x, y);
            int alive = GET_CELL(current, x, y);

            // Conway's rules:
            // 1. Live cell with 2-3 neighbors survives
            // 2. Dead cell with exactly 3 neighbors becomes alive
            if ((alive && (neighbors == 2 || neighbors == 3)) ||
                (!alive && neighbors == 3))
            {
                SET_CELL(next, x, y);
            }
        }
    }
}

static int count_population(const cell_buffer_t *board)
{
    int count = 0;
    for (int y = 0; y < BOARD_HEIGHT; y++)
    {
        for (int x = 0; x < BOARD_WIDTH; x++)
        {
            count += GET_CELL(board, x, y);
        }
    }
    return count;
}

// ==================== Display Functions ====================
static void display_board_fade(const cell_buffer_t *board, unsigned char *fade_buffer)
{
    printf(ANSI_CURSOR_HOME);

    for (int y = 0; y < BOARD_HEIGHT; y++)
    {
        for (int x = 0; x < BOARD_WIDTH; x++)
        {
            int idx = y * BOARD_WIDTH + x;

            if (GET_CELL(board, x, y))
            {
                // Cell is alive - set to maximum fade
                fade_buffer[idx] = FADE_LEVELS;
                printf(FADE_CHAR_FULL);
            }
            else
            {
                // Cell is dead - decay fade value
                if (fade_buffer[idx] > 0)
                {
                    fade_buffer[idx]--;
                }

                // Display based on fade level
                switch (fade_buffer[idx])
                {
                case 7:
                case 6:
                    printf(FADE_CHAR_HIGH);
                    break;
                case 5:
                case 4:
                    printf(FADE_CHAR_MED);
                    break;
                case 3:
                case 2:
                case 1:
                    printf(FADE_CHAR_LOW);
                    break;
                default:
                    printf(FADE_CHAR_NONE);
                    break;
                }
            }
        }
        printf(ANSI_CLEAR_LINE "\n");
    }

    printf(ANSI_CLEAR_BELOW);
    fflush(stdout);
}

static void reset_fade_buffer(unsigned char *fade_buffer, const cell_buffer_t *board)
{
    for (int y = 0; y < BOARD_HEIGHT; y++)
    {
        for (int x = 0; x < BOARD_WIDTH; x++)
        {
            int idx = y * BOARD_WIDTH + x;
            fade_buffer[idx] = GET_CELL(board, x, y) ? FADE_LEVELS : 0;
        }
    }
}

// ==================== Stability Detection ====================
static int check_stability(int *history, int current_pop)
{
    // Shift history and add current population
    for (int i = HISTORY_SIZE - 1; i > 0; i--)
    {
        history[i] = history[i - 1];
    }
    history[0] = current_pop;

    // Check for static pattern (unchanging population)
    if (history[0] == history[1])
    {
        return 1;
    }

    // Check for period-2 oscillator
    if (HISTORY_SIZE >= 3 && history[0] == history[2] && history[0] != history[1])
    {
        return 1;
    }

    // Check for period-3 oscillator (simplified)
    if (HISTORY_SIZE >= 3)
    {
        int unique_count = 1;
        for (int i = 1; i < HISTORY_SIZE; i++)
        {
            int is_unique = 1;
            for (int j = 0; j < i; j++)
            {
                if (history[i] == history[j])
                {
                    is_unique = 0;
                    break;
                }
            }
            if (is_unique)
                unique_count++;
        }

        // If we have 3 unique values that are close together, likely a period-3
        if (unique_count == 3)
        {
            int min = history[0], max = history[0];
            for (int i = 1; i < HISTORY_SIZE; i++)
            {
                if (history[i] < min)
                    min = history[i];
                if (history[i] > max)
                    max = history[i];
            }
            if (max - min <= 2)
            {
                return 1;
            }
        }
    }

    return 0;
}

static void handle_stable_state(game_state_t *state)
{
    int current_pop = count_population(state->current);

    if (check_stability(state->population_history, current_pop))
    {
        state->stable_count++;

        if (state->stable_count >= STABLE_GENERATIONS)
        {
            // Display reset message
            printf("\033[%d;1H" ANSI_CLEAR_LINE "Resetting due to stable state (gen %d)...",
                   BOARD_HEIGHT + 1, state->generation);
            fflush(stdout);
            sleep_ms(1000);

            // Reset board
            randomize_board(state->current);
            reset_fade_buffer(state->fade_buffer, state->current);

            // Reset tracking variables
            memset(state->population_history, 0, sizeof(state->population_history));
            state->stable_count = 0;
            state->generation = 0;

            // Clear reset message
            printf("\033[%d;1H" ANSI_CLEAR_LINE, BOARD_HEIGHT + 1);
        }
    }
    else
    {
        state->stable_count = 0;
    }

    state->generation++;
}

// ==================== Utility Functions ====================
static void sleep_ms(int milliseconds)
{
#ifdef _WIN32
    Sleep(milliseconds);
#else
    usleep(milliseconds * 1000);
#endif
}

// clang-format off
#include <Adafruit_Protomatter.h>

// serial debugging -- set to 1 to enable debug output
// note that this sends some messages at critical times esp. during stagnancy detetction,
// so shouldn't be enabled unless you're troubleshooting; you're likely to get some stuttering
#define DEBUG_ENABLED 0
#if DEBUG_ENABLED
  #define DEBUG_INIT() Serial.begin(115200); while(!Serial) delay(10);
  #define DEBUG_PRINT(x) Serial.print(x)
  #define DEBUG_PRINTLN(x) Serial.println(x)
  #define DEBUG_PRINTF(fmt, ...) Serial.printf(fmt, __VA_ARGS__)
#else
  #define DEBUG_INIT()
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINTF(fmt, ...)
#endif

// seengreat rgb matrix adapter with rpi pico matrix pin definitions
#define R1 2
#define G1 3
#define B1 4
#define R2 5
#define G2 8
#define B2 9
#define A 10
#define B 16
#define C 18
#define D 20
#define E 22
#define CLK 11
#define LAT 12
#define OE 13

// display dimensions
#define DISPLAY_WIDTH 64 // physical width of rgb matrix display for driver setup

// game board dimensions (can be different from display if you want to use a subset of the matrix)
#define BOARD_WIDTH 64  // width of game of life playing field
#define BOARD_HEIGHT 64 // height of game of life playing field

// game params
#define FRAME_DELAY_MIN 10  // min ms between generations, randomized per run-to-stagnant (if you don't want random timing, make this match FRAME_DELAY_MIN)
#define FRAME_DELAY_MAX 100 // max ms between generations, randomized per run-to-stagnant
#define FADE_LEVELS 8       // fade trail length for dead cells (if changed, the color schemes should be updated to match)

// stagnancy and stuck-board detection
#define STAGNANT_GENERATIONS 10    // generations of stagnancy before resetting board ("simmer" time before reset)
#define HISTORY_SIZE 8             // board state history for detecting oscillators -- oscillators periodic at greater than this value will not be detected and the board will inf loop
#define POP_HISTORY_SIZE 32        // population count history for detecting moving repetition patterns (like gliders) -- must be greater than POP_PATTERN_LENGTH
#define POP_PATTERN_LENGTH 8       // check for repeating patterns of this length in population history to mark board as stagnant
#define MAX_GENERATION_COUNT 60000 // failsafe fallback -- board will always reset after this many gens. 10min = 600s; 600s / .01s delelay = 60_000 gens at fastest speed (100mins at slowest, eh.)

// color config
#define COLOR_SCHEME -1       // -1 = random color scheme, other number = use the nth color scheme in the array (0 indexed)
#define NUM_COLOR_SCHEMES 15   // must match number of schemes in COLOR_SCHEMES array

// bitpacked board access macros -- why store HEIGHT * WIDTH values when we can just store HEIGHT * WIDTH bits?
// +7 means we always round in the generous direction when determining how many bytes we need to pack the board
#define BUFFER_SIZE ((BOARD_WIDTH * BOARD_HEIGHT + 7) / 8)
#define WRAP_X(x) (((x) + BOARD_WIDTH) % BOARD_WIDTH)
#define WRAP_Y(y) (((y) + BOARD_HEIGHT) % BOARD_HEIGHT)
// turn an X,Y coordinate into the index of the bit location in the packed board
#define COORD_TO_IDX(x, y) (WRAP_Y(y) * BOARD_WIDTH + WRAP_X(x))
// get the byte a given index is in
#define BYTE_INDEX(idx) ((idx) >> 3)
// get the bit within that byte -- together, makes it fast to do bitwise logic against the bitpack to check cell life
#define BIT_MASK(idx) (1 << ((idx) & 7))

// combining the above into nice helpful macros:
// AND to fetch
#define GET_CELL(board, x, y) \
  ((board[BYTE_INDEX(COORD_TO_IDX(x, y))] & BIT_MASK(COORD_TO_IDX(x, y))) != 0)

// OR to set
#define SET_CELL(board, x, y) \
  (board[BYTE_INDEX(COORD_TO_IDX(x, y))] |= BIT_MASK(COORD_TO_IDX(x, y)))

// AND NOT to clear
#define CLEAR_CELL(board, x, y) \
  (board[BYTE_INDEX(COORD_TO_IDX(x, y))] &= ~BIT_MASK(COORD_TO_IDX(x, y)))

// convert 24-bit rgb to 16-bit 5:6:5 format so we can have pretty and easy-to-read color scheme defs
static inline uint16_t color565(uint8_t red, uint8_t green, uint8_t blue)
{
  return ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3);
}

// color schemes with fade trails
const uint16_t COLOR_SCHEMES[NUM_COLOR_SCHEMES][FADE_LEVELS] = {
  // blue → green → red
  {
    color565(0, 0, 0),
    color565(0, 0, 8), color565(0, 0, 8), color565(0, 0, 8),
    color565(0, 16, 0), color565(0, 16, 0), color565(0, 16, 0),
    color565(255, 0, 0)
  },

  // sunset: purple → orange → yellow
  {
    color565(0, 0, 0),
    color565(16, 0, 16), color565(32, 0, 32), color565(64, 0, 48),
    color565(128, 32, 0), color565(192, 64, 0), color565(255, 128, 0),
    color565(255, 255, 0)
  },

  // fire: dark red → orange → yellow
  {
    color565(0, 0, 0),
    color565(16, 0, 0), color565(32, 0, 0), color565(64, 16, 0),
    color565(128, 32, 0), color565(192, 64, 0), color565(255, 128, 0),
    color565(255, 255, 64)
  },

  // matrix: dark → bright green
  {
    color565(0, 0, 0),
    color565(0, 8, 0), color565(0, 16, 0), color565(0, 32, 0),
    color565(0, 64, 0), color565(0, 128, 0), color565(0, 192, 0),
    color565(0, 255, 0)
  },

  // Cyberpunk: hot pink → electric blue → lime green
  {
    color565(0, 0, 0),
    color565(64, 0, 32), color565(128, 0, 64), color565(255, 0, 128),
    color565(128, 64, 255), color565(0, 128, 255), color565(0, 255, 128),
    color565(128, 255, 0)
  },

  // Autumn: crimson → burnt orange → golden yellow → teal
  {
    color565(0, 0, 0),
    color565(128, 0, 32), color565(192, 0, 48), color565(255, 64, 0),
    color565(255, 128, 0), color565(255, 192, 0), color565(192, 224, 64),
    color565(0, 192, 192)
  },

  // Prism: red → green → blue → yellow → magenta
  {
    color565(0, 0, 0),
    color565(128, 0, 0), color565(255, 0, 0), color565(128, 128, 0),
    color565(0, 255, 0), color565(0, 128, 128), color565(0, 0, 255),
    color565(255, 255, 0)
  },

  // Aurora: deep purple → pink → yellow
  {
    color565(0, 0, 0),
    color565(32, 0, 48), color565(64, 0, 96), color565(96, 16, 128),
    color565(192, 32, 128), color565(255, 64, 128), color565(255, 128, 192),
    color565(255, 255, 128)
  },

  // Toxic: black → toxic green → lime
  {
    color565(0, 0, 0),
    color565(0, 16, 0), color565(16, 32, 0), color565(32, 64, 0),
    color565(64, 128, 0), color565(128, 192, 0), color565(192, 255, 0),
    color565(224, 255, 32)
  },

  // Deep Sea: black → deep blue → turquoise
  {
    color565(0, 0, 0),
    color565(0, 0, 32), color565(0, 16, 64), color565(0, 32, 96),
    color565(0, 64, 128), color565(0, 128, 160), color565(32, 192, 192),
    color565(64, 255, 224)
  },

  // Candy: dark pink → hot pink → white
  {
    color565(0, 0, 0),
    color565(32, 0, 16), color565(64, 0, 32), color565(128, 16, 64),
    color565(192, 32, 96), color565(255, 64, 128), color565(255, 128, 192),
    color565(255, 224, 255)
  },

  // Stark white -- off -> white
  {
    color565(0, 0, 0),
    color565(0, 0, 0), color565(0, 0, 0), color565(0, 0, 0),
    color565(0, 0, 0), color565(0, 0, 0), color565(0, 0, 0),
    color565(255, 255, 255)
  },

  // Stark red -- off -> red
  {
    color565(0, 0, 0),
    color565(0, 0, 0), color565(0, 0, 0), color565(0, 0, 0),
    color565(0, 0, 0), color565(0, 0, 0), color565(0, 0, 0),
    color565(255, 0, 0)
  },

  // Stark green -- off -> green
  {
    color565(0, 0, 0),
    color565(0, 0, 0), color565(0, 0, 0), color565(0, 0, 0),
    color565(0, 0, 0), color565(0, 0, 0), color565(0, 0, 0),
    color565(0, 255, 0)
  },

  // Stark blue -- off -> blue
  {
    color565(0, 0, 0),
    color565(0, 0, 0), color565(0, 0, 0), color565(0, 0, 0),
    color565(0, 0, 0), color565(0, 0, 0), color565(0, 0, 0),
    color565(0, 0, 255)
  }
};

typedef unsigned char cell_buffer_t;

typedef struct
{
  cell_buffer_t *current;
  cell_buffer_t *next;
  unsigned char *fade_buffer;
  cell_buffer_t *board_history[HISTORY_SIZE]; // store complete board states

  int generation;
  int current_color_scheme;
  int frame_delay;

  // stagnancy -- history repetition
  int history_index; // current position in circular buffer
  int stagnant_count;

  // stagnancy -- population tracking
  uint16_t population_history[POP_HISTORY_SIZE];
  int pop_history_index;
  int pop_stagnant_count;
} game_state_t;

void update_board(const cell_buffer_t *current, cell_buffer_t *next);
void display_board_fade(const cell_buffer_t *board, unsigned char *fade_buffer, int color_scheme);
int check_stagnancy(game_state_t *state);
void initialize_fresh_board(game_state_t *state);
uint16_t count_population(const cell_buffer_t *board);
int check_population_pattern(game_state_t *state);

// hardware setup
uint8_t rgbPins[] = {R1, G1, B1, R2, G2, B2};
uint8_t addrPins[] = {A, B, C, D, E};
Adafruit_Protomatter matrix(
  DISPLAY_WIDTH, 6, 1, rgbPins, 5, addrPins,
  CLK, LAT, OE, true);

game_state_t game;
unsigned long lastUpdate = 0;

// moore's law? nah man, community policing. moore's neighborhood watch!
// (moore neighborhood offsets)
// (i'll show myself out)
static const int mn_x[] = {-1, 0, 1, -1, 1, -1, 0, 1};
static const int mn_y[] = {-1, -1, -1, 0, 0, 1, 1, 1};

// clang-format on

// count the number of living cells on the board
uint16_t count_population(const cell_buffer_t *board) {
  uint16_t population = 0;
  for (int i = 0; i < BUFFER_SIZE; i++) {
    // Count set bits in each byte using Brian Kernighan's algorithm
    uint8_t byte = board[i];
    while (byte) {
      population++;
      byte &= byte - 1; // Clear the lowest set bit
    }
  }
  return population;
}

// check if the population (i.e. /count/) history shows a repeating pattern
int check_population_pattern(game_state_t *state) {
  // need at least 2x pattern length of history to detect a pattern
  if (state->generation < POP_PATTERN_LENGTH * 2) {
    return 0;
  }

  // check if the last POP_PATTERN_LENGTH values repeat
  int pattern_found = 1;
  for (int i = 0; i < POP_PATTERN_LENGTH; i++) {
    int idx1 = (state->pop_history_index - i - 1 + POP_HISTORY_SIZE) %
               POP_HISTORY_SIZE;
    int idx2 = (state->pop_history_index - i - 1 - POP_PATTERN_LENGTH +
                POP_HISTORY_SIZE) %
               POP_HISTORY_SIZE;

    if (state->population_history[idx1] != state->population_history[idx2]) {
      pattern_found = 0;
      break;
    }
  }

  return pattern_found;
}

void update_board(const cell_buffer_t *current, cell_buffer_t *next) {
  memset(next, 0, BUFFER_SIZE);

  for (int y = 0; y < BOARD_HEIGHT; y++) {
    for (int x = 0; x < BOARD_WIDTH; x++) {
      // count neighbors
      int neighbors = 0;
      for (int i = 0; i < 8; i++) {
        neighbors += GET_CELL(current, x + mn_x[i], y + mn_y[i]);
      }

      int alive = GET_CELL(current, x, y);

      // all hail conway! survive with 2-3 neighbors, birth with exactly 3 [and
      // implied die with more or fewer]
      if ((alive && (neighbors == 2 || neighbors == 3)) ||
          (!alive && neighbors == 3)) {
        SET_CELL(next, x, y);
      }
    }
  }
}

// we don't actually render the "board", we render the fade buffer because it
// tells us what the pretty colors should be
void display_board_fade(const cell_buffer_t *board, unsigned char *fade_buffer,
                        int color_scheme) {
  for (int y = 0; y < BOARD_HEIGHT; y++) {
    for (int x = 0; x < BOARD_WIDTH; x++) {
      int idx = y * BOARD_WIDTH + x;

      if (GET_CELL(board, x, y)) {
        // alive
        fade_buffer[idx] = FADE_LEVELS - 1;
      } else {
        // rip; begin to fade away...
        if (fade_buffer[idx] > 0) {
          fade_buffer[idx]--;
        }
      }

      // actually write the value
      matrix.drawPixel(x, y, COLOR_SCHEMES[color_scheme][fade_buffer[idx]]);
    }
  }
}

// check if current board state exists in history; if it does, we're in a loop
// and should start again
int check_stagnancy(game_state_t *state) {
  // First check for exact board state repetition
  for (int i = 0; i < HISTORY_SIZE; i++) {
    if (memcmp(state->current, state->board_history[i], BUFFER_SIZE) == 0) {
      DEBUG_PRINTLN("Stagnancy detected -- board repetition");
      return 1; // found a match - we're in a loop
    }
  }

  // add current board to history (circular buffer)
  memcpy(state->board_history[state->history_index], state->current,
         BUFFER_SIZE);
  state->history_index = (state->history_index + 1) % HISTORY_SIZE;

  // now check population count and patterns
  uint16_t current_population = count_population(state->current);
  state->population_history[state->pop_history_index] = current_population;
  state->pop_history_index = (state->pop_history_index + 1) % POP_HISTORY_SIZE;

  // check if we have a repeating population (i.e. /count/) pattern
  if (check_population_pattern(state)) {
    state->pop_stagnant_count++;
    if (state->pop_stagnant_count >= STAGNANT_GENERATIONS) {
      DEBUG_PRINTLN("Stagnancy detected -- repeating population count");
      return 1;
    }
  } else {
    state->pop_stagnant_count = 0;
  }

  return 0; // no stagnancy detected
}

// init/reset the board with fresh random state
void initialize_fresh_board(game_state_t *state) {
  // dim the house lights
  matrix.fillScreen(0);
  matrix.show();
  delay(100);

  // new colors & frame delay
  if (COLOR_SCHEME == -1) {
    state->current_color_scheme = random(NUM_COLOR_SCHEMES);
  } else {
    // do some bounding because we're nice like that
    state->current_color_scheme = COLOR_SCHEME % NUM_COLOR_SCHEMES;
  }
  state->frame_delay = random(FRAME_DELAY_MIN, FRAME_DELAY_MAX + 1);

  DEBUG_PRINTF("New game: Color scheme %d, Frame delay: %dms\n",
               state->current_color_scheme, state->frame_delay);

  // generate new board @ 35% alive cells -- blah blah optimized blah monte
  // carlo blah blah
  memset(state->current, 0, BUFFER_SIZE);
  for (int y = 0; y < BOARD_HEIGHT; y++) {
    for (int x = 0; x < BOARD_WIDTH; x++) {
      if (random(100) < 35) {
        SET_CELL(state->current, x, y);
      }
    }
  }

  // fresh new fade
  for (int y = 0; y < BOARD_HEIGHT; y++) {
    for (int x = 0; x < BOARD_WIDTH; x++) {
      int idx = y * BOARD_WIDTH + x;
      state->fade_buffer[idx] =
          GET_CELL(state->current, x, y) ? (FADE_LEVELS - 1) : 0;
    }
  }

  // fresh new history
  for (int i = 0; i < HISTORY_SIZE; i++) {
    memset(state->board_history[i], 0, BUFFER_SIZE);
  }
  state->history_index = 0;
  state->stagnant_count = 0;
  state->generation = 0;

  // Reset population tracking
  memset(state->population_history, 0, sizeof(state->population_history));
  state->pop_history_index = 0;
  state->pop_stagnant_count = 0;

  // let's begin
  display_board_fade(state->current, state->fade_buffer,
                     state->current_color_scheme);
  matrix.show();
}

void setup(void) {
  DEBUG_INIT();
  DEBUG_PRINTLN("Conway's Game of Life starting up...");

  ProtomatterStatus status = matrix.begin();

  if (status != PROTOMATTER_OK) {
    DEBUG_PRINTF("Non-Protomatter_OK begin return (expected 0, got %d) -- "
                 "matrix initialization failed; crashing out!\n",
                 status);
    for (;;)
      ; // if your matrix won't show anything, this might be a good place to
        // inject Serial.println() info about its status
  }

  // initialize game state
  game.current = (cell_buffer_t *)calloc(BUFFER_SIZE, sizeof(cell_buffer_t));
  game.next = (cell_buffer_t *)calloc(BUFFER_SIZE, sizeof(cell_buffer_t));
  game.fade_buffer = (unsigned char *)calloc(BOARD_WIDTH * BOARD_HEIGHT,
                                             sizeof(unsigned char));

  // allocate memory for board history
  for (int i = 0; i < HISTORY_SIZE; i++) {
    game.board_history[i] =
        (cell_buffer_t *)calloc(BUFFER_SIZE, sizeof(cell_buffer_t));
  }

  game.history_index = 0;
  game.stagnant_count = 0;
  game.generation = 0;
  game.current_color_scheme = 0;
  game.frame_delay = FRAME_DELAY_MIN;

  // Initialize population tracking
  memset(game.population_history, 0, sizeof(game.population_history));
  game.pop_history_index = 0;
  game.pop_stagnant_count = 0;

  if (!game.current || !game.next) {
    DEBUG_PRINTLN("Failure to allocate the game -- crashing out!");
    for (;;)
      ;
  }

  // seed rng with analog noise and hammer it a few times since apparently that
  // can improve randomness? the randomness sucked and google said to hammer it
  // so okay
  randomSeed(analogRead(A0));
  for (int i = 0; i < 16; i++) {
    random(256);
  }

  initialize_fresh_board(&game);
  DEBUG_PRINTLN("Setup complete!");
}

void loop(void) {
  unsigned long currentTime = millis();

  // cool kids don't hang their processors with delay()
  if (currentTime - lastUpdate >= game.frame_delay) {
    lastUpdate = currentTime;

    if (check_stagnancy(&game)) {
      game.stagnant_count++;

      if (game.stagnant_count >= STAGNANT_GENERATIONS) {
        DEBUG_PRINTF("New game due to stagnancy (generation %d)\n",
                     game.generation);
        initialize_fresh_board(&game);
      }
    } else {
      game.stagnant_count = 0;
    }

    if (game.generation > MAX_GENERATION_COUNT) {
      // we can end up in states like forever-gliders, so cap it
      DEBUG_PRINTF("New game due to max generation count (generation %d)\n",
                   game.generation);
      initialize_fresh_board(&game);
    }

    game.generation++;

    update_board(game.current, game.next);

    cell_buffer_t *temp = game.current;
    game.current = game.next;
    game.next = temp;

    display_board_fade(game.current, game.fade_buffer,
                       game.current_color_scheme);
    matrix.show();
  }
}

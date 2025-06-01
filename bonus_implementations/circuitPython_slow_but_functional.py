# SPDX-FileCopyrightText: 2020 Jeff Epler for Adafruit Industries
# SPDX-License-Identifier: MIT

import random
import time
import board
import displayio
import framebufferio
import rgbmatrix

displayio.release_displays()

# Configuration
FADE_LEVELS = 8  # Number of fade levels for dying cells
FADE_DECAY_RATE = 1  # How fast cells fade (higher = faster, reduced to 1 for smoother transition)

# Stability detection parameters
AUTO_RESET_ON_STABLE = True
STABLE_GENERATIONS = 10
HISTORY_SIZE = 3

# Colors for fade effect - Red → Green → Blue → Off
FADE_COLORS = [
    0x000000,  # Level 0: Black (dead)
    0x000004,  # Level 1: Very dim blue
    0x00000A,  # Level 2: Dim blue
    0x00000F,  # Level 3: Medium blue
    0x000400,  # Level 4: Dim green
    0x000A00,  # Level 5: Medium green
    0x000F00,  # Level 6: Bright green
    0xFF0000,  # Level 7: Bright red (alive)
]

class GameOfLifeMatrix:
    def __init__(self, matrix, display):
        self.matrix = matrix
        self.display = display
        self.width = display.width
        self.height = display.height

        # Create bitmaps for double buffering
        self.b1 = displayio.Bitmap(self.width, self.height, FADE_LEVELS)
        self.b2 = displayio.Bitmap(self.width, self.height, FADE_LEVELS)

        # Create palette with fade colors
        self.palette = displayio.Palette(FADE_LEVELS)
        for i in range(FADE_LEVELS):
            self.palette[i] = FADE_COLORS[i]

        # Create tile grids and groups
        self.tg1 = displayio.TileGrid(self.b1, pixel_shader=self.palette)
        self.tg2 = displayio.TileGrid(self.b2, pixel_shader=self.palette)
        self.g1 = displayio.Group()
        self.g1.append(self.tg1)
        self.g2 = displayio.Group()
        self.g2.append(self.tg2)

        # Set initial display
        self.display.root_group = self.g1
        self.current_bitmap = self.b1
        self.next_bitmap = self.b2

        # Fade buffer to track fade levels
        self.fade_buffer = bytearray(self.width * self.height)

        # Stability tracking
        if AUTO_RESET_ON_STABLE:
            self.population_history = [0] * HISTORY_SIZE
            self.stable_count = 0
            self.generation = 0

        # Pre-calculate for optimization
        self.width_minus_1 = self.width - 1
        self.height_minus_1 = self.height - 1

    def randomize(self, fraction=0.33):
        """Initialize board with random pattern"""
        random_threshold = int(fraction * 32767)
        size = self.width * self.height

        # Randomize current bitmap
        for i in range(size):
            if random.getrandbits(15) < random_threshold:
                self.current_bitmap[i] = FADE_LEVELS - 1  # Full brightness
                self.fade_buffer[i] = FADE_LEVELS - 1
            else:
                self.current_bitmap[i] = 0
                self.fade_buffer[i] = 0

    def apply_life_rule_with_fade(self):
        """Apply Conway's rules with fade effect"""
        width = self.width
        height = self.height
        current = self.current_bitmap
        next_bmp = self.next_bitmap
        fade = self.fade_buffer

        # Pre-calculate constants
        width_minus_1 = self.width_minus_1
        height_minus_1 = self.height_minus_1
        max_fade = FADE_LEVELS - 1

        for y in range(height):
            # Calculate row offsets once per row
            y_offset = y * width

            # Calculate wrapped y coordinates
            if y == 0:
                ym1_offset = height_minus_1 * width
            else:
                ym1_offset = (y - 1) * width

            if y == height_minus_1:
                yp1_offset = 0
            else:
                yp1_offset = (y + 1) * width

            # Process row
            x_minus_1 = width_minus_1

            for x in range(width):
                # Calculate wrapped x coordinates
                if x == width_minus_1:
                    x_plus_1 = 0
                else:
                    x_plus_1 = x + 1

                # Count living neighbors (only fully alive cells count)
                neighbors = 0
                if current[x_minus_1 + ym1_offset] == max_fade: neighbors += 1
                if current[x + ym1_offset] == max_fade: neighbors += 1
                if current[x_plus_1 + ym1_offset] == max_fade: neighbors += 1
                if current[x_minus_1 + y_offset] == max_fade: neighbors += 1
                if current[x_plus_1 + y_offset] == max_fade: neighbors += 1
                if current[x_minus_1 + yp1_offset] == max_fade: neighbors += 1
                if current[x + yp1_offset] == max_fade: neighbors += 1
                if current[x_plus_1 + yp1_offset] == max_fade: neighbors += 1

                # Apply Conway's rules
                cell_index = x + y_offset
                cell_alive = current[cell_index] == max_fade

                if (neighbors == 3) or (neighbors == 2 and cell_alive):
                    # Cell lives/is born
                    next_bmp[cell_index] = max_fade
                    fade[cell_index] = max_fade
                else:
                    # Cell dies or stays dead - apply fade
                    if fade[cell_index] > 0:
                        fade[cell_index] = max(0, fade[cell_index] - FADE_DECAY_RATE)
                    next_bmp[cell_index] = fade[cell_index]

                # Update x_minus_1 for next iteration
                x_minus_1 = x

    def count_population(self):
        """Count fully alive cells"""
        count = 0
        max_fade = FADE_LEVELS - 1
        size = self.width * self.height

        for i in range(size):
            if self.current_bitmap[i] == max_fade:
                count += 1

        return count

    def check_stability(self, current_pop):
        """Check if the population has stabilized"""
        # Shift history
        for i in range(HISTORY_SIZE - 1, 0, -1):
            self.population_history[i] = self.population_history[i - 1]
        self.population_history[0] = current_pop

        # Check for static pattern
        if self.population_history[0] == self.population_history[1]:
            return True

        # Check for period-2 oscillator
        if (HISTORY_SIZE >= 3 and
            self.population_history[0] == self.population_history[2] and
            self.population_history[0] != self.population_history[1]):
            return True

        # Check for small oscillations
        if HISTORY_SIZE >= 3:
            unique_values = set(self.population_history)
            if len(unique_values) <= 3:
                min_val = min(self.population_history)
                max_val = max(self.population_history)
                if max_val - min_val <= 2:
                    return True

        return False

    def handle_stable_state(self):
        """Handle stability detection and auto-reset"""
        if not AUTO_RESET_ON_STABLE:
            return

        current_pop = self.count_population()

        if self.check_stability(current_pop):
            self.stable_count += 1

            if self.stable_count >= STABLE_GENERATIONS:
                print(f"Resetting due to stable state (gen {self.generation})")
                # Reset board
                self.randomize(0.33)

                # Reset tracking
                self.population_history = [0] * HISTORY_SIZE
                self.stable_count = 0
                self.generation = 0

                time.sleep(0.5)  # Brief pause
        else:
            self.stable_count = 0

        self.generation += 1

    def run(self):
        """Main game loop"""
        self.display.auto_refresh = True
        self.randomize(0.33)

        while True:
            # Apply rules with fade
            self.apply_life_rule_with_fade()

            # Swap buffers
            self.current_bitmap, self.next_bitmap = self.next_bitmap, self.current_bitmap
            if self.display.root_group == self.g1:
                self.display.root_group = self.g2
            else:
                self.display.root_group = self.g1

            # Check for stability
            # self.handle_stable_state()

# Matrix configuration
bit_depth_value = 6
unit_width = 64
unit_height = 64
chain_width = 1
chain_height = 1
serpentine_value = True
width_value = unit_width * chain_width
height_value = unit_height * chain_height

# Initialize matrix
matrix = rgbmatrix.RGBMatrix(
    width=width_value, height=height_value, bit_depth=bit_depth_value,
    rgb_pins=[board.GP2, board.GP3, board.GP4, board.GP5, board.GP8, board.GP9],
    addr_pins=[board.GP10, board.GP16, board.GP18, board.GP20, board.GP22],
    clock_pin=board.GP11, latch_pin=board.GP12, output_enable_pin=board.GP13,
    tile=chain_height, serpentine=serpentine_value,
    doublebuffer=True
)

display = framebufferio.FramebufferDisplay(matrix, auto_refresh=False)

# Create and run the game
game = GameOfLifeMatrix(matrix, display)
game.run()

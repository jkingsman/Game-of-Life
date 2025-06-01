#!/bin/bash

# Conway's Game of Life function for .bashrc
# Usage: gameoflife [size] [delay] [alive_char] [dead_char]

gameoflife() {
    # Get parameters from arguments or use defaults
    local SIZE=${1:-20}
    local DELAY=${2:-0.1}
    local ALIVE_CHAR=${3:-● }
    local DEAD_CHAR=${4:-· }

    # Initialize the grid
    declare -A grid
    declare -A next_grid

    # ANSI escape codes
    local clear_screen="\033[2J"
    local move_home="\033[H"
    local hide_cursor="\033[?25l"
    local show_cursor="\033[?25h"

    # Initialize grid with random cells
    for ((i=0; i<SIZE; i++)); do
        for ((j=0; j<SIZE; j++)); do
            if ((RANDOM % 100 < 30)); then
                grid[$i,$j]=1
            else
                grid[$i,$j]=0
            fi
        done
    done

    # Count live neighbors for a cell
    count_neighbors() {
        local row=$1
        local col=$2
        local count=0

        for ((dr=-1; dr<=1; dr++)); do
            for ((dc=-1; dc<=1; dc++)); do
                if ((dr == 0 && dc == 0)); then
                    continue
                fi

                local nr=$(((row + dr + SIZE) % SIZE))
                local nc=$(((col + dc + SIZE) % SIZE))

                if ((grid[$nr,$nc] == 1)); then
                    ((count++))
                fi
            done
        done

        echo $count
    }

    # Cleanup function
    cleanup() {
        printf "$show_cursor"
        printf "$clear_screen"
        trap - INT TERM
        return 0
    }

    # Set up signal handler for clean exit
    trap cleanup INT TERM EXIT

    # Hide cursor and clear screen
    printf "$hide_cursor"
    printf "$clear_screen"

    # Main game loop
    while true; do
        # Move cursor home without clearing
        printf "$move_home"

        # Display the grid
        for ((i=0; i<SIZE; i++)); do
            for ((j=0; j<SIZE; j++)); do
                if ((grid[$i,$j] == 1)); then
                    printf "%s" "$ALIVE_CHAR"
                else
                    printf "%s" "$DEAD_CHAR"
                fi
            done
            printf "\n"
        done

        # Clear any remaining lines from previous frame
        printf "\033[J"

        # Update generation
        for ((i=0; i<SIZE; i++)); do
            for ((j=0; j<SIZE; j++)); do
                local neighbors=$(count_neighbors $i $j)
                local current=${grid[$i,$j]}

                if ((current == 1)); then
                    if ((neighbors == 2 || neighbors == 3)); then
                        next_grid[$i,$j]=1
                    else
                        next_grid[$i,$j]=0
                    fi
                else
                    if ((neighbors == 3)); then
                        next_grid[$i,$j]=1
                    else
                        next_grid[$i,$j]=0
                    fi
                fi
            done
        done

        # Copy next generation to current
        for ((i=0; i<SIZE; i++)); do
            for ((j=0; j<SIZE; j++)); do
                grid[$i,$j]=${next_grid[$i,$j]}
            done
        done

        sleep "$DELAY"
    done
}

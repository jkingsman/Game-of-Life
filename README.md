# Fancy Conway's Life for RP2040 & LED matrix

This is the code (in `game_of_life.ino`) for an LED matrix to display Conway's game of life with a fun dazzle effect as the cells die off.

This was tested and dev'd using Adafruit Protomatter 1.7.0 and Philhower core ([installation instructios](https://learn.adafruit.com/rp2040-arduino-with-the-earlephilhower-core/installing-the-earlephilhower-core)) for RPi Pico 4.5.4 via Arduino IDE. The hardware is a Seengreat RGB Matrix adapter (https://www.amazon.com/dp/B0BC8Y447G) and Waveshare RGB-Matrix-P2.5-64x64 (https://www.amazon.com/dp/B0BQYFRVTR) with a 5V3A USBC PD adapter for power, although broadly this code should be trivially adaptable to any board that can use Protomatter to write pixels out, and not-trivially-but-still-fairly-easily adapted to other output systems -- I've worked hard to keep the code readable.

For the RP2040 (RPi Pico), I needed to underclock CPU to ~100MHz for stability (YMMV; some RP2040s ran happily at 200, others terribly).

See it in action here: https://www.youtube.com/watch?v=OcW1guaPUco

## Bonus Implementations

In `bonus_implementations`, I've included some other versions I wrote or vibe-coded; the final version for microcontroller evolved from these.

The C implementation (`console_life.c`) can be compiled and run with `gcc -O2 -o game_of_life console_life.c; ./game_of_life`.

`circuitPython_slow_but_functional.py` is, as the name describes, a drop in on the above hardware but Python based. It works but it's slow.

`game_of_life.sh` can be sourced (`. ./game_of_life.sh`) and then invoked with `gameoflife`; this made it easy to include in my dotfiles when I need something mesmerizing to look at.

## Stats

I wanted to determine the optimal alive-percentage for a random wrap-around Life board of a given size to maximize the duration before stagnancy. See more on my [blog](https://jacksbrain.com/2025/06/longevity-optimized-conway-s-game-of-life-fill-density-for-square-wrap-around-boards/) for outcomes.

`life_statistics.c` iterates until stagnancy and dumps the data to the console; board size, percentage fill span, and sample size are all customizable.

Compile and run with `gcc -O3 -fopenmp -o life_statistics life_statistics.c -lm; ./life_statistics`.


## License etc.

MIT License, Copyright 2025 Jack Kingsman <jack@jackkingsman.me>

Portions of this codebase were developed in concert with an LLM.

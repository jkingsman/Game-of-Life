# Fancy Conway's Life for 64x64 LED matrix

## This repo is a work in progress

Tested and dev'd using Adafruit Protomatter 1.7.0 and Philhower core ([installation instructios](https://learn.adafruit.com/rp2040-arduino-with-the-earlephilhower-core/installing-the-earlephilhower-core)) for RPi Pico 4.5.4 via Arduino IDE.

Seengreat RGB Matrix adapter (https://www.amazon.com/dp/B0BC8Y447G) and Waveshare RGB-Matrix-P2.5-64x64 (https://www.amazon.com/dp/B0BQYFRVTR) with a 5V3A USBC PD adapter for power.

Need to underclock CPU to ~100MHz for stability (YMMV; some RP2040s ran happily at 200, others terribly).

gcc -O2 -o game_of_life console_life.c; ./game_of_life

gcc -O3 -fopenmp -o life_statistics life_statistics.c -lm; ./life_statistics

/*
 * newtest.c
 *
 * Copyright (c) 2014 Jeremy Garff <jer @ jers.net>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 *     1.  Redistributions of source code must retain the above copyright notice, this list of
 *         conditions and the following disclaimer.
 *     2.  Redistributions in binary form must reproduce the above copyright notice, this list
 *         of conditions and the following disclaimer in the documentation and/or other materials
 *         provided with the distribution.
 *     3.  Neither the name of the owner nor the names of its contributors may be used to endorse
 *         or promote products derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */


static char VERSION[] = "XX.YY.ZZ";

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <signal.h>
#include <stdarg.h>
#include <getopt.h>
#include <time.h>


#include "clk.h"
#include "gpio.h"
#include "dma.h"
#include "pwm.h"
#include "version.h"

#include "ws2811.h"


#define ARRAY_SIZE(stuff)       (sizeof(stuff) / sizeof(stuff[0]))

// defaults for cmdline options
#define TARGET_FREQ             WS2811_TARGET_FREQ
#define GPIO_PIN                18
#define DMA                     10
//#define STRIP_TYPE            WS2811_STRIP_RGB		// WS2812/SK6812RGB integrated chip+leds
#define STRIP_TYPE              WS2811_STRIP_GBR		// WS2812/SK6812RGB integrated chip+leds
//#define STRIP_TYPE            SK6812_STRIP_RGBW		// SK6812RGBW (NOT SK6812RGB)

#define WIDTH                   60
#define HEIGHT                  1
#define LED_COUNT               (WIDTH * HEIGHT)

int width = WIDTH;
int height = HEIGHT;
int led_count = LED_COUNT;

int clear_on_exit = 1;

ws2811_t ledstring =
{
    .freq = TARGET_FREQ,
    .dmanum = DMA,
    .channel =
    {
        [0] =
        {
            .gpionum = GPIO_PIN,
            .count = LED_COUNT,
            .invert = 0,
            .brightness = 255,
            .strip_type = STRIP_TYPE,
        },
        [1] =
        {
            .gpionum = 0,
            .count = 0,
            .invert = 0,
            .brightness = 0,
        },
    },
};

ws2811_led_t *matrix;

static uint8_t running = 1;

void matrix_render(void)
{
    int x, y;

    for (x = 0; x < width; x++)
    {
        for (y = 0; y < height; y++)
        {
            ledstring.channel[0].leds[(y * width) + x] = matrix[y * width + x];
        }
    }
}

void matrix_clear(void)
{
    int x, y;

    for (y = 0; y < (height ); y++)
    {
        for (x = 0; x < width; x++)
        {
            matrix[y * width + x] = 0;
        }
    }
}

static void ctrl_c_handler(int signum)
{
	(void)(signum);
    running = 0;
}

static void setup_handlers(void)
{
    struct sigaction sa =
    {
        .sa_handler = ctrl_c_handler,
    };

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

struct fire_pixel {
    int target;
    int curr;
};
int fire_pixel_next(struct fire_pixel *px) {
    while (px->curr == px->target || px->target == 0) {
        //while (abs(px->target - px->curr) < 10) {
            px->target = rand() % 256;
            // printf("setting target to %d\n", px->target);
        //}
    }

    if (px->curr < px->target) {
        px->curr += 1;
    } else {
        px->curr -= 1;
    }

    return px->curr;
}
uint32_t rgb(int r, int g, int b) {
    // 0xWWBBGGRR
    return (b << 16) | (g << 8) | r;
}
void fire(ws2811_led_t *matrix) {
    #define NUM_FIRE_PX 9
    static struct fire_pixel fire_pxs[NUM_FIRE_PX] = { 0 };
    int len = width * height;

    for (int i = 0; i < NUM_FIRE_PX; i++) {
        int intensity = fire_pixel_next(fire_pxs + i);
        uint32_t color = rgb(intensity, intensity / 4, 0);
        //uint32_t color = rgb(intensity, intensity, intensity);
        //matrix[len - i - 1] = color;
        matrix[i + 12] = color;
    }
}

int main(/*int argc, char *argv[]*/) {
    ws2811_return_t ret;
    sprintf(VERSION, "%d.%d.%d", VERSION_MAJOR, VERSION_MINOR, VERSION_MICRO);
    // parseargs(argc, argv, &ledstring);
    matrix = malloc(sizeof(ws2811_led_t) * width * height);
    setup_handlers();

    if ((ret = ws2811_init(&ledstring)) != WS2811_SUCCESS) {
        fprintf(stderr, "ws2811_init failed: %s\n", ws2811_get_return_t_str(ret));
        return ret;
    }

    srand(time(NULL));
    while (running) {
        // matrix_raise();
        // matrix_bottom();
        fire(matrix);
        matrix_render();

        if ((ret = ws2811_render(&ledstring)) != WS2811_SUCCESS) {
            fprintf(stderr, "ws2811_render failed: %s\n", ws2811_get_return_t_str(ret));
            break;
        }

        // limit fps
        usleep(1000000 / 120);
    }

    if (clear_on_exit) {
        matrix_clear();
        matrix_render();
        ws2811_render(&ledstring);
    }

    ws2811_fini(&ledstring);

    printf ("\n");
    return ret;
}

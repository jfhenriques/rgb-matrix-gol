/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *
 * Using the C-API of this library.
 *
 */
#include "led-matrix-c.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <signal.h>

#define ROWS 64
#define COLS 64
#define ITER_WAIT_MS 100

static volatile int keepRunning = 1;

void intHandler(int dummy) {
    fprintf(stdout, "Interrupted\n");
    keepRunning = 0;
}


int main(int argc, char **argv) {
  struct RGBLedMatrixOptions options;
  struct RGBLedMatrix *matrix;
  struct LedCanvas *offscreen_canvas;
  int width, height;
  int x, y, i;
  short universe[ROWS * COLS];
  struct timeval now, before;
  long elapsed = -1;
  uint8_t c;
  short curUnit;

  signal(SIGINT, intHandler);

  memset(&options, 0, sizeof(options));
  memset(&universe, 0, sizeof(universe));
  options.rows = ROWS;
  options.cols = COLS;
  options.chain_length = 1;
  options.hardware_mapping = "adafruit-hat-pwm";

  /* This supports all the led commandline options. Try --led-help */
  matrix = led_matrix_create_from_options(&options, &argc, &argv);
  if (matrix == NULL)
    return 1;

  /* Let's do an example with double-buffering. We create one extra
   * buffer onto which we draw, which is then swapped on each refresh.
   * This is typically a good aproach for animations and such.
   */
  offscreen_canvas = led_matrix_create_offscreen_canvas(matrix);

  led_canvas_get_size(offscreen_canvas, &width, &height);

  fprintf(stderr, "Size: %dx%d. Hardware gpio mapping: %s\n",
          width, height, options.hardware_mapping);

  while (keepRunning) {


    for (y = 0; y < height; ++y) {
      for (x = 0; x < width; ++x) {

        curUnit = universe[ y + (x * COLS) ];
        c = curUnit == 0 ? 0xff : 0x00 ;
        led_canvas_set_pixel(offscreen_canvas, x, y, c, c, c);
      }
    }

    /* Now, we swap the canvas. We give swap_on_vsync the buffer we
     * just have drawn into, and wait until the next vsync happens.
     * we get back the unused buffer to which we'll draw in the next
     * iteration.
     */

    

//    gettimeofday(&now, 0);

    offscreen_canvas = led_matrix_swap_on_vsync(matrix, offscreen_canvas);
  }

  /*
   * Make sure to always call led_matrix_delete() in the end to reset the
   * display. Installing signal handlers for defined exit is a good idea.
   */
  led_matrix_delete(matrix);

  return 0;
}

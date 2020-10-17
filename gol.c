/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *
 * Using the C-API of this library.
 *
 */
#include "led-matrix-c.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <signal.h>


#define PANEL_W 64
#define PANEL_H 64
#define PANEL_DIV 1

#define WIDTH  (PANEL_W/PANEL_DIV)
#define HEIGHT (PANEL_H/PANEL_DIV)
#define TOTAL_UNITS (WIDTH * HEIGHT)
#define RAND_MOD 3
#define REFRESH_RATE 8
#define SLEEP_MS ((long)(1.0/REFRESH_RATE*1000.0))
#define CHECK_STUCK_SECS 10

#define GET_ARR_X_Y(x,y) ( (x) + ( (y) * WIDTH ) )

static volatile int keepRunning = 1;

void intHandler(int dummy) {
    fprintf(stdout, "Interrupted\n");
    keepRunning = 0;
}

void randomize_universe(short *universe) {
  for(size_t i = 0; i < TOTAL_UNITS; i++)
    universe[i] = ( rand() % RAND_MOD ) == 0 ;
}

typedef struct universe_node {
  short cells[TOTAL_UNITS];
  struct universe_node *next;
} universe_node;

short are_universes_cells_same(universe_node * u1, universe_node * u2) {
  for(int i = 0; i < TOTAL_UNITS; i++)
  {
    if( u1->cells[i] != u2->cells[i] )
      return 0;
  }
  return 1;
}

typedef int (*funcCountNeigh)(universe_node * universe, int x, int y);

int count_cell_neighbours_inf(universe_node * universe, int x, int y) {

  int neigh = 0;
  
  for(int yy=y-1, c_y = 3; c_y > 0; yy++, c_y--) {

    if (yy < 0) yy = HEIGHT - 1;
    else if (yy >= HEIGHT) yy = 0;

    for(int xx=x-1, c_x = 3; c_x > 0; xx++, c_x--) {

      if (xx < 0) xx = WIDTH - 1;
      else if (xx >= WIDTH) xx = 0;

      if( yy == y && xx == x )
        continue;

      if( universe->cells[ GET_ARR_X_Y(xx, yy) ] )
         neigh++;

     // No point on keep counting
     if( neigh > 3 )
       return neigh;
    }
  }

  return neigh;
}


int count_cell_neighbours(universe_node * universe, int x, int y) {

  int neigh = 0;
  
  for(int yy=y-1, c_y = 3; c_y > 0; yy++, c_y--) {

    if (yy < 0 || yy >= HEIGHT)
      continue;

    for(int xx=x-1, c_x = 3; c_x > 0; xx++, c_x--) {

      if (xx < 0 || xx >= WIDTH )
        continue;

      if( yy == y && xx == x )
        continue;

      if( universe->cells[ GET_ARR_X_Y(xx, yy) ] )
         neigh++;

     // No point on keep counting
     if( neigh > 3 )
       return neigh;
    }
  }

  return neigh;
}

long calculate_diff_ms(struct timeval t1, struct timeval t2) {
  return (long) (
      (( t2.tv_sec - t1.tv_sec) * 1000.0 )      // sec to ms
    + ((t2.tv_usec - t1.tv_usec) / 1000.0 ));   // us to ms
}

void compute_next_iteration(universe_node * universe, funcCountNeigh countNeigh ) {

  int liveNeigh;
  int curPos;
  for( int y = 0; y < HEIGHT; y++ ) {
    for( int x = 0; x < WIDTH; x++ ) {

      curPos = GET_ARR_X_Y(x, y);
      liveNeigh = (*countNeigh)(universe, x, y);
    
      // Current cell is alive
      if( universe->cells[ curPos ] )
        // Stay alive only if exactly 2 or 3 neighbours are also alive
        universe->next->cells[ curPos ] = ( liveNeigh == 2 || liveNeigh == 3 ) ;

      // Current cell is dead
      else
        // Become alive if exactly 3 neighbours are alive
        universe->next->cells[ curPos ] = liveNeigh == 3;
    }
  }
}



int main(int argc, char **argv) {

  struct RGBLedMatrixOptions options;
  struct RGBLedMatrix *matrix;
  struct LedCanvas *offscreen_canvas;
  int width, height;
  int x, y; 

  universe_node u1,u2,u3;
  u1.next = &u2;
  u2.next = &u3;
  u3.next = &u1;
  
  universe_node *curUniverse = &u1;

  struct timeval now, before, last_stuck_check;
  uint8_t c;
  time_t t;
  long sleepDiff;

  // seed rng
  srand((unsigned) time(&t));

  // trap SIGINT to do cleanup
  signal(SIGINT, intHandler);

  memset(&options, 0, sizeof(options));
  
  gettimeofday(&last_stuck_check, 0);

  options.rows = PANEL_H;
  options.cols = PANEL_W;
  options.brightness = 40;
  options.chain_length = 1;
  options.hardware_mapping = "adafruit-hat-pwm";


  /* This supports all the led commandline options. Try --led-help */
  matrix = led_matrix_create_from_options(&options, &argc, &argv);
  if (matrix == NULL)
    return 1;
  
  // Create double buffer
  offscreen_canvas = led_matrix_create_offscreen_canvas(matrix);
  led_canvas_get_size(offscreen_canvas, &width, &height);

  fprintf(stderr, "Size: %dx%d. Hardware gpio mapping: %s\n", width, height, options.hardware_mapping);

  if ( width != PANEL_W || height != PANEL_H )
    fprintf(stderr, "Rows and Cols do no match width and height. Aborting...\n");

  else {

    randomize_universe(curUniverse->cells);

    // Start loop
    while (keepRunning) {

      gettimeofday(&before, 0);

      if ( calculate_diff_ms(last_stuck_check, before) >= ( CHECK_STUCK_SECS * 1000 ) ) {
        last_stuck_check = before;

        if ( are_universes_cells_same( curUniverse, curUniverse->next ) )
          randomize_universe(curUniverse->cells);
     }

      for (y = 0; y < PANEL_H; y++) {
        for (x = 0; x < PANEL_W; x++) {

          c = curUniverse->cells[ GET_ARR_X_Y( (int)(x/PANEL_DIV), (int)(y/PANEL_DIV) ) ] ? 0x60 : 0x00 ;
          led_canvas_set_pixel(offscreen_canvas, x, y, 5, c , 5);
        }
      }

      offscreen_canvas = led_matrix_swap_on_vsync(matrix, offscreen_canvas);

      // Compute next universe iteration

      compute_next_iteration(curUniverse, &count_cell_neighbours_inf);
      curUniverse = curUniverse->next;

      gettimeofday(&now, 0);
      sleepDiff = SLEEP_MS - calculate_diff_ms(before, now);

      if( sleepDiff > 0 )
        usleep(sleepDiff * 1000);
    }
  }

  // Cleanup
  led_matrix_delete(matrix);

  return 0;
}



#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <signal.h>
#include <stdbool.h>
#include "led-matrix-c.h"

#define PANEL_W 64
#define PANEL_H 64

#define RAND_MOD 3

#define REFRESH_RATE(r) ((long)(1.0/(r)*1000.0))
#define CHECK_STUCK_SECS 8

#define GET_ARR_X_Y(x,y,w) ( (x) + ( (y) * (w)) )


/****************************************************************************************************/

static volatile int keepRunning = 1;
static int COLOR_LIVE[3][3] = {{0x80, 0, 0}, {0, 0x80, 0}, {0, 0, 0x99}};
static int COLOR_DEAD[3][3] = {{0, 4, 4}, {4, 0, 4}, {4, 4, 0}};

/****************************************************************************************************/

typedef struct p_universe p_universe;
typedef struct universe_context universe_context;
typedef int (*func_countNeigh)(universe_context * c, p_universe * u, int x, int y);

struct p_universe {
  char *cells;
  p_universe *next;
};

struct universe_context {
  int WIDTH;
  int HEIGHT;
  int DIVIDER;
  int TOTAL_CELLS;
  int FRAME_MS;
  func_countNeigh FUNC_COUNT_NEIGH;
  int COLOR_MODE;
};



/****************************************************************************************************/


void int_handler(int signal) {
    fprintf(stdout, "Interrupted...\n");
    
    keepRunning = 0;
}

long calculate_diff_ms(struct timeval t1, struct timeval t2) {
  return (long) (
      (( t2.tv_sec - t1.tv_sec) * 1000.0 )      // sec to ms
    + ((t2.tv_usec - t1.tv_usec) / 1000.0 ));   // us to ms
}

/****************************************************************************************************/

void randomize_universe(p_universe *uni, int uni_cells) {
  for(int i = 0; i < uni_cells; i++)
    uni->cells[i] = ( rand() % RAND_MOD ) == 0 ;
}

p_universe * init_parallel_universes(int total_universes, int uni_cells) {
  if ( total_universes <= 0 || uni_cells <= 0 )
    return NULL;

  p_universe *uni, *cur = NULL, *first = NULL;

  for( int i = 0; i < total_universes; i++ ) {
    uni = malloc(sizeof(p_universe));
    uni->cells = malloc(uni_cells * sizeof(char));

    randomize_universe(uni, uni_cells);

    if( i == 0 )
      cur = first = uni;
    
    else {
      cur->next = uni;
      cur = uni;
    }
  }

  return (cur->next = first);
}


void destroy_parallel_universes(p_universe *uni) {

  if( uni == NULL || uni->cells == NULL )
    return;

  if ( uni->cells != NULL) {
    free( uni->cells ); 
    uni->cells = NULL;
  }

  if ( uni-> next != NULL )
  {
    p_universe *next = uni->next;
    uni->next = NULL;

    destroy_parallel_universes(next);
  }

  free ( uni );
}

bool compare_universes_cells(p_universe * u1, p_universe * u2, int uni_cells) {
  for(int i = 0; i < uni_cells; i++)
  {
    if( u1->cells[i] != u2->cells[i] )
      return false;
  }
  return true;
}

p_universe * init_universes_w_context(universe_context * context) {
  if ( context == NULL )
    return NULL;

  return init_parallel_universes(3, context->TOTAL_CELLS);
}

/****************************************************************************************************/


int count_cell_neighbours_scroll(universe_context * context, p_universe * uni, int x, int y) {

  int neigh = 0;
  
  for(int yy=y-1, c_y = 3; c_y > 0; yy++, c_y--) {

    if (yy < 0) yy = context->HEIGHT - 1;
    else if (yy >= context->HEIGHT) yy = 0;

    for(int xx=x-1, c_x = 3; c_x > 0; xx++, c_x--) {

      if (xx < 0) xx = context->WIDTH - 1;
      else if (xx >= context->WIDTH) xx = 0;

      if( yy == y && xx == x )
        continue;

      if( uni->cells[ GET_ARR_X_Y(xx, yy, context->WIDTH) ] )
         neigh++;

     // No point on keep counting
     if( neigh > 3 )
       return neigh;
    }
  }

  return neigh;
}


int count_cell_neighbours(universe_context * context, p_universe * uni, int x, int y) {

  int neigh = 0;
  
  for(int yy=y-1, c_y = 3; c_y > 0; yy++, c_y--) {

    if (yy < 0 || yy >= context->HEIGHT)
      continue;

    for(int xx=x-1, c_x = 3; c_x > 0; xx++, c_x--) {

      if (xx < 0 || xx >= context->WIDTH )
        continue;

      if( yy == y && xx == x )
        continue;

      if( uni->cells[ GET_ARR_X_Y(xx, yy, context->WIDTH) ] )
         neigh++;

     // No point on keep counting
     if( neigh > 3 )
       return neigh;
    }
  }

  return neigh;
}

void compute_next_iteration(universe_context * context, p_universe * uni ) {

  int liveNeigh;
  int curPos;
  for( int y = 0; y < context->HEIGHT; y++ ) {
    for( int x = 0; x < context->WIDTH; x++ ) {

      curPos = GET_ARR_X_Y(x, y, context->WIDTH);
      liveNeigh = (*context->FUNC_COUNT_NEIGH)(context, uni, x, y);
    
      // Current cell is alive
      if( uni->cells[ curPos ] )
        // Stay alive only if exactly 2 or 3 neighbours are also alive
        uni->next->cells[ curPos ] = ( liveNeigh == 2 || liveNeigh == 3 ) ;

      // Current cell is dead
      else
        // Become alive if exactly 3 neighbours are alive
        uni->next->cells[ curPos ] = liveNeigh == 3;
    }
  }
}

void init_context(universe_context * context, int divider, int fr, func_countNeigh fCountNeigh, int color_mode) {
  if ( context == NULL )
    return;

  if ( divider < 1 )
    divider = 1;

  context->WIDTH = (int)(PANEL_W/divider);
  context->HEIGHT = (int)(PANEL_H/divider);
  context->DIVIDER = divider;
  context->TOTAL_CELLS = context->WIDTH * context->HEIGHT;
  context->FRAME_MS = REFRESH_RATE(fr);
  context->FUNC_COUNT_NEIGH = fCountNeigh;
  context->COLOR_MODE = color_mode;
}
void init_rand_context(universe_context * context) {
  int dividers[] = {1, 2, 4};
  int frs[] = { 10, 15, 20, 30, 60};
  func_countNeigh counters[] = {count_cell_neighbours_scroll, count_cell_neighbours};

  init_context(context,
    dividers[ rand() % 3 ],
    frs[ rand() % 5 ],
    counters[rand() % 2],
    rand() % 3
  );
}

/****************************************************************************************************/


void clean_shutdown(p_universe *uni, struct RGBLedMatrix *matrix) {
  if ( uni != NULL )
    destroy_parallel_universes(uni);

  if ( matrix != NULL )
    led_matrix_delete(matrix);
}

int main(int argc, char **argv) {

  struct RGBLedMatrixOptions options;
  struct RGBLedMatrix *matrix;
  struct LedCanvas *offscreen_canvas;
  
  universe_context context;
  p_universe *universe;

  int width, height;
  int x, y;

  struct timeval now, before, last_stuck_check;
  time_t t;
  long sleepDiff;

  // trap SIGINT to do cleanup
  signal(SIGINT, int_handler);
  
  // seed rng
  srand((unsigned) time(&t));

  // initialize context
  init_rand_context(&context);

  // initialize universes
  universe = init_universes_w_context(&context);
  if ( universe == NULL ) {
    fprintf(stderr, "Error initializing universes. Aborting...\n");
    return 1;
  }

  memset(&options, 0, sizeof(options));
  
  gettimeofday(&last_stuck_check, 0);

  // initialize panel options
  options.rows = PANEL_H;
  options.cols = PANEL_W;
  options.brightness = 40;
  options.chain_length = 1;
  options.hardware_mapping = "adafruit-hat-pwm";


  // This supports all the led commandline options. Try --led-help
  matrix = led_matrix_create_from_options(&options, &argc, &argv);
  if (matrix == NULL) {
    fprintf(stderr, "Error initializing led matrix. Aborting...\n");
    clean_shutdown(universe, NULL);
    return 1;
  }
  
  // Create double buffer
  offscreen_canvas = led_matrix_create_offscreen_canvas(matrix);
  led_canvas_get_size(offscreen_canvas, &width, &height);

  fprintf(stderr, "Size: %dx%d. Hardware gpio mapping: %s\n", width, height, options.hardware_mapping);

  if ( width != PANEL_W || height != PANEL_H ) {
    fprintf(stderr, "Rows and Cols do no match width and height. Aborting...\n");
    clean_shutdown(universe, matrix);
    return 1;
  }

  // Start loop
  while (keepRunning) {

    gettimeofday(&before, 0);

    if ( calculate_diff_ms(last_stuck_check, before) >= ( CHECK_STUCK_SECS * 1000 ) ) {
      last_stuck_check = before;

      if ( compare_universes_cells( universe, universe->next, context.TOTAL_CELLS ) ) {
        destroy_parallel_universes(universe);
        init_rand_context(&context);
        universe = init_universes_w_context(&context);
      }
    }

    for (y = 0; y < PANEL_H; y++) {
      for (x = 0; x < PANEL_W; x++) {

        if( universe->cells[ GET_ARR_X_Y( (int)(x/context.DIVIDER), (int)(y/context.DIVIDER), context.WIDTH ) ] )
          led_canvas_set_pixel(offscreen_canvas, x, y, COLOR_LIVE[context.COLOR_MODE][0], COLOR_LIVE[context.COLOR_MODE][1], COLOR_LIVE[context.COLOR_MODE][2]);
        else
          led_canvas_set_pixel(offscreen_canvas, x, y, COLOR_DEAD[context.COLOR_MODE][0], COLOR_DEAD[context.COLOR_MODE][1], COLOR_DEAD[context.COLOR_MODE][2]);
      }
    }

    offscreen_canvas = led_matrix_swap_on_vsync(matrix, offscreen_canvas);

    // Compute next universe iteration

    compute_next_iteration(&context, universe);
    universe = universe->next;

    gettimeofday(&now, 0);
    sleepDiff = context.FRAME_MS - calculate_diff_ms(before, now);

    if( sleepDiff > 0 )
      usleep(sleepDiff * 1000);
  }

  // Cleanup
  clean_shutdown(universe, matrix);

  return 0;
}


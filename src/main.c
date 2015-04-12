/*
 * main.c
 * Jab - Main (and only) class
 */

#include <pebble.h>

#define TAP_NOT_DATA true
#define THRESHOLD 20
#define SAMPLE_RATE 10

static Window *s_main_window;
static TextLayer *s_pq_layer, *s_pq_label_layer;
static TextLayer *s_average_pq_layer, *s_best_pq_layer, *s_total_punches_layer;
static TextLayer *s_timer_layer;
static TextLayer *s_timer_label_layer, *s_average_freq_layer;
static double q[10];
static int qidx = 11;
static int punches = 0;
static int best_pq = 0;
static int avg_pq = 0;
static int total_pq = 0;
static bool force_mode = true;

static GBitmap *force_image;
static BitmapLayer *force_imageLayer;

static AppTimer* update_timer = NULL;
static double start_time = 0;
static double elapsed_time = 0;

double float_time_ms() {
	time_t seconds;
	uint16_t milliseconds;
	time_ms(&seconds, &milliseconds);
	return (double)seconds + ((double)milliseconds / 1000.0);
}

static double sqrt( double num )
{
  double a, p, e = 0.001, b;
  
  a = num;
  p = a * a;
  while( p - num >= e )
  {
  b = ( a + ( num / a ) ) / 2;
  a = b;
  p = a * a;
  }
  return a;
}

static int calc_freq(int punches, double elapsed_time) {
  double punches_per_ms = punches/elapsed_time;
  return punches_per_ms * 60;
}

static void tap_handler(AccelAxisType axis, int32_t direction) {
  static char s_buffer[128];
  text_layer_set_text(s_pq_layer, "Punch Detected!");
  punches += 1;
  qidx = 0;
  
  snprintf(s_buffer, sizeof(s_buffer), "# Punches: %d", punches);
  text_layer_set_text(s_total_punches_layer, s_buffer);
}

static int sumq() {
  int res = 0;
  for (int i = 0; i < 10; i++) {
    res += q[i];
  }
  return res;
}

static void data_handler(AccelData *data, uint32_t num_samples) {
  // Long lived buffer
  static char s_buffer[128];
  static char s_buffer2[128];
  static char s_buffer3[128];
  
  double cur_ax = 0;
  double cur_ay = 0;
  double cur_az = 0;
  
  for (int i = 0; i < SAMPLE_RATE; i++) {
    cur_ax += data[i].x;
    cur_ay += data[i].y;
    cur_az += data[i].z;
  }
  
  cur_ax /= 10.0;
  cur_ay /= 10.0;
  cur_az /= 10.0;
  
  if (qidx < 10) {
    q[qidx++] = sqrt(cur_ax * cur_ax + cur_ay * cur_ay + cur_az * cur_az);
  }
  
  if (qidx == 10) {
    
  int pq = sumq();
  
  if (pq > best_pq) {
    best_pq = pq;
    snprintf(s_buffer2, sizeof(s_buffer2), "Best PQ: %d", best_pq);
    text_layer_set_text(s_best_pq_layer, s_buffer2);
  }
  
  total_pq += pq;
  snprintf(s_buffer3, sizeof(s_buffer3), "Avg PQ: %d", total_pq/punches);
  text_layer_set_text(s_average_pq_layer, s_buffer3);
    
    snprintf(s_buffer, sizeof(s_buffer), "%d", pq);
    text_layer_set_text(s_pq_layer, s_buffer);
    qidx++;
  }
  
}

static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect window_bounds = layer_get_bounds(window_layer);

  force_image = gbitmap_create_with_resource(RESOURCE_ID_POWER_MODE_IMG);
  force_imageLayer = bitmap_layer_create(GRect(5, 5, window_bounds.size.w - 10, 30));
  bitmap_layer_set_bitmap(force_imageLayer, force_image);
  layer_add_child(window_layer, bitmap_layer_get_layer(force_imageLayer));
  
  // Create PQ Score TextLayer
  s_pq_layer = text_layer_create(GRect(5, 50, window_bounds.size.w - 10, 30));
  text_layer_set_font(s_pq_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24));
  text_layer_set_text(s_pq_layer, "No data yet.");
  text_layer_set_overflow_mode(s_pq_layer, GTextOverflowModeWordWrap);
  text_layer_set_text_alignment(s_pq_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_pq_layer));
  
  // Create PQ Label
  s_pq_label_layer = text_layer_create(GRect(5, 30, window_bounds.size.w - 10, 30));
  text_layer_set_font(s_pq_label_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text(s_pq_label_layer, "Last PQ:");
  text_layer_set_overflow_mode(s_pq_label_layer, GTextOverflowModeWordWrap);
  text_layer_set_text_alignment(s_pq_label_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_pq_label_layer));
  
  s_best_pq_layer = text_layer_create(GRect(5, 82, window_bounds.size.w - 10, 30));
  text_layer_set_font(s_best_pq_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text(s_best_pq_layer, "Best PQ: 0");
  text_layer_set_overflow_mode(s_best_pq_layer, GTextOverflowModeWordWrap);
    text_layer_set_text_alignment(s_best_pq_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_best_pq_layer));
  
  s_average_pq_layer = text_layer_create(GRect(5, 100, window_bounds.size.w - 10, 30));
  text_layer_set_font(s_average_pq_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text(s_average_pq_layer, "Avg PQ: 0");
  text_layer_set_overflow_mode(s_average_pq_layer, GTextOverflowModeWordWrap);
    text_layer_set_text_alignment(s_average_pq_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_average_pq_layer));
  
  s_total_punches_layer = text_layer_create(GRect(5, 120, window_bounds.size.w - 10, 30));
  text_layer_set_font(s_total_punches_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24));
  text_layer_set_text(s_total_punches_layer, "# Punches: 0");
  text_layer_set_overflow_mode(s_total_punches_layer, GTextOverflowModeWordWrap);
  text_layer_set_text_alignment(s_total_punches_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_total_punches_layer));
  
 
  
 
}

static void main_window_unload(Window *window) {
  // Destroy output TextLayer
  if (force_mode) {
  text_layer_destroy(s_pq_layer);
    text_layer_destroy(s_pq_label_layer);
    text_layer_destroy(s_average_pq_layer);
    text_layer_destroy(s_best_pq_layer);
    text_layer_destroy(s_total_punches_layer);
  }
  else {
   app_timer_cancel(update_timer);
    text_layer_destroy(s_timer_layer);
    text_layer_destroy(s_timer_label_layer);
    text_layer_destroy(s_average_freq_layer);
    text_layer_destroy(s_total_punches_layer);
  }
}

void handle_timer(void* data) {
    static char s_buffer[] = "00:00:00";
    static char s_buffer2[128];
		double now = float_time_ms();
    
  
		elapsed_time = now - start_time;
  
    int tenths = (int)(elapsed_time * 10) % 10;
    int seconds = (int)elapsed_time % 60;
    int minutes = (int)elapsed_time / 60 % 60;
	  
    snprintf(s_buffer, sizeof(s_buffer), "%02d:%02d:%02d", minutes, seconds, tenths);
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "Loop index now %d %d %d", minutes, seconds, tenths);
    text_layer_set_text(s_timer_layer, s_buffer);
  
    if (punches > 0) {
      snprintf(s_buffer2, sizeof(s_buffer2), "Freq: %d p/min", calc_freq(punches, elapsed_time));
      text_layer_set_text(s_average_freq_layer, s_buffer2);
    }
  
    update_timer = app_timer_register(100, handle_timer, NULL);
}

static void down_single_click_handler(ClickRecognizerRef recognizer, void *context) {
  Window *window = (Window *)context;
  Layer *window_layer = window_get_root_layer(window);
  GRect window_bounds = layer_get_bounds(window_layer);
  
  if (force_mode) {
    
    punches = 0;
    
    text_layer_destroy(s_pq_layer);
    text_layer_destroy(s_pq_label_layer);
    text_layer_destroy(s_average_pq_layer);
    text_layer_destroy(s_best_pq_layer);
    text_layer_destroy(s_total_punches_layer);
    
    force_image = gbitmap_create_with_resource(RESOURCE_ID_SPEED_MODE_IMG);
  bitmap_layer_set_bitmap(force_imageLayer, force_image);
    
    s_timer_layer = text_layer_create(GRect(5, 50, window_bounds.size.w - 10, 30));
    text_layer_set_font(s_timer_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24));
    text_layer_set_text(s_timer_layer, "No data yet.");
    text_layer_set_overflow_mode(s_timer_layer, GTextOverflowModeWordWrap);
    text_layer_set_text_alignment(s_timer_layer, GTextAlignmentCenter);
    layer_add_child(window_layer, text_layer_get_layer(s_timer_layer));
    
    s_timer_label_layer = text_layer_create(GRect(5, 30, window_bounds.size.w - 10, 30));
    text_layer_set_font(s_timer_label_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
    text_layer_set_text(s_timer_label_layer, "Elapsed Time:");
    text_layer_set_overflow_mode(s_timer_label_layer, GTextOverflowModeWordWrap);
    text_layer_set_text_alignment(s_timer_label_layer, GTextAlignmentCenter);
    layer_add_child(window_layer, text_layer_get_layer(s_timer_label_layer));
    
  
    s_average_freq_layer = text_layer_create(GRect(5, 82, window_bounds.size.w - 10, 30));
    text_layer_set_font(s_average_freq_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
    text_layer_set_text(s_average_freq_layer, "Freq: 0 p/min");
    text_layer_set_overflow_mode(s_average_freq_layer, GTextOverflowModeWordWrap);
    text_layer_set_text_alignment(s_average_freq_layer, GTextAlignmentCenter);
    layer_add_child(window_layer, text_layer_get_layer(s_average_freq_layer));
  
    s_total_punches_layer = text_layer_create(GRect(5, 120, window_bounds.size.w - 10, 30));
    text_layer_set_font(s_total_punches_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24));
    text_layer_set_text(s_total_punches_layer, "# Punches: 0");
    text_layer_set_overflow_mode(s_total_punches_layer, GTextOverflowModeWordWrap);
    text_layer_set_text_alignment(s_total_punches_layer, GTextAlignmentCenter);
    layer_add_child(window_layer, text_layer_get_layer(s_total_punches_layer));
    
    start_time = float_time_ms();
    update_timer = app_timer_register(100, handle_timer, NULL);
    accel_data_service_unsubscribe();
    
  } else {
    app_timer_cancel(update_timer);
    text_layer_destroy(s_timer_layer);
    text_layer_destroy(s_timer_label_layer);
    text_layer_destroy(s_average_freq_layer);
    text_layer_destroy(s_total_punches_layer);
    
    //Reset all variables;
  punches = 0; 
  best_pq = 0;
  avg_pq = 0;
  total_pq = 0;
    
    int num_samples = SAMPLE_RATE;
    accel_data_service_subscribe(num_samples, data_handler);

    // Choose update rate
    accel_service_set_sampling_rate(ACCEL_SAMPLING_100HZ);
    
    force_image = gbitmap_create_with_resource(RESOURCE_ID_POWER_MODE_IMG);
  bitmap_layer_set_bitmap(force_imageLayer, force_image);
    
     // Create PQ Score TextLayer
  s_pq_layer = text_layer_create(GRect(5, 50, window_bounds.size.w - 10, 30));
  text_layer_set_font(s_pq_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24));
  text_layer_set_text(s_pq_layer, "No data yet.");
  text_layer_set_overflow_mode(s_pq_layer, GTextOverflowModeWordWrap);
  text_layer_set_text_alignment(s_pq_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_pq_layer));
  
  // Create PQ Label
  s_pq_label_layer = text_layer_create(GRect(5, 30, window_bounds.size.w - 10, 30));
  text_layer_set_font(s_pq_label_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text(s_pq_label_layer, "Last PQ:");
  text_layer_set_overflow_mode(s_pq_label_layer, GTextOverflowModeWordWrap);
  text_layer_set_text_alignment(s_pq_label_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_pq_label_layer));
  
  s_best_pq_layer = text_layer_create(GRect(5, 82, window_bounds.size.w - 10, 30));
  text_layer_set_font(s_best_pq_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text(s_best_pq_layer, "Best PQ: 0");
  text_layer_set_overflow_mode(s_best_pq_layer, GTextOverflowModeWordWrap);
    text_layer_set_text_alignment(s_best_pq_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_best_pq_layer));
  
  s_average_pq_layer = text_layer_create(GRect(5, 100, window_bounds.size.w - 10, 30));
  text_layer_set_font(s_average_pq_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text(s_average_pq_layer, "Avg PQ: 0");
  text_layer_set_overflow_mode(s_average_pq_layer, GTextOverflowModeWordWrap);
    text_layer_set_text_alignment(s_average_pq_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_average_pq_layer));
  
  s_total_punches_layer = text_layer_create(GRect(5, 120, window_bounds.size.w - 10, 30));
  text_layer_set_font(s_total_punches_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24));
  text_layer_set_text(s_total_punches_layer, "# Punches: 0");
  text_layer_set_overflow_mode(s_total_punches_layer, GTextOverflowModeWordWrap);
  text_layer_set_text_alignment(s_total_punches_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_total_punches_layer));
  }
  force_mode = !force_mode;
  
}


//Handles single click center
static void select_single_click_handler(ClickRecognizerRef recognizer, void *context) {
  
  //Window *window = (Window *)context;
  //Reset all variables;
  
  if (force_mode) {
  
    punches = 0; 
    best_pq = 0;
    avg_pq = 0;
    total_pq = 0;
    
    text_layer_set_text(s_pq_layer, "No data yet.");
    
    text_layer_set_text(s_pq_label_layer, "Last PQ:");
    
    text_layer_set_text(s_best_pq_layer, "Best PQ: 0");
    
    text_layer_set_text(s_average_pq_layer, "Avg PQ: 0");
    
    text_layer_set_text(s_total_punches_layer, "# Punches: 0");
  }
  
  else {
    start_time = float_time_ms();
    punches = 0;
  }
}


//Handles Clicks
static void config_provider(Window *window) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_single_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_single_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, down_single_click_handler);
}

static void init() {
  // Create main Window
  s_main_window = window_create();
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });
  window_stack_push(s_main_window, true);
    
    int num_samples = SAMPLE_RATE;
    accel_data_service_subscribe(num_samples, data_handler);

    // Choose update rate
    accel_service_set_sampling_rate(ACCEL_SAMPLING_100HZ);
  
  accel_tap_service_subscribe(tap_handler);
  window_set_click_config_provider(s_main_window, (ClickConfigProvider) config_provider);
}



static void deinit() {
  // Destroy main Window
  window_destroy(s_main_window);


    accel_tap_service_unsubscribe();

    accel_data_service_unsubscribe();
  

}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
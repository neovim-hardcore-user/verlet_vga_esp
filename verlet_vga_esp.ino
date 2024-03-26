#include <ESP32Lib.h>
#include <Ressources/Font6x8.h>
#include <Ressources/CodePage437_8x16.h>
#include <esp_task_wdt.h>


struct point {
  int x, y, px, py;
  unsigned short color;
};


static constexpr uint8_t FIXED_POINT = 11;
static constexpr uint16_t MAX_POINTS = 1000;

static constexpr int w = 200 << FIXED_POINT;
static constexpr int h = 150 << FIXED_POINT;

static point points[MAX_POINTS];


int constexpr r = 2 << FIXED_POINT;
int constexpr r2 = r * 2;
int constexpr fr = r >> FIXED_POINT;
int constexpr fr2 = fr * 2;

static int16_t circle_lines[fr + 1];

static constexpr int GRID_X = (w / (r * 2 + 1)) + 1;
static constexpr int GRID_Y = (h / (r * 2 + 1)) + 1;


static constexpr uint16_t GRID_MAX = 3;

static uint16_t grid_dict[GRID_X][GRID_Y][GRID_MAX + 1];


static constexpr int boundary_max_x = w - r;
static constexpr int boundary_max_y = h - r;

static uint frame_time = 0;
static uint current_time = 0;
static float delta_time = 0;

static constexpr bool logger = 1;




static constexpr float rotating_speed = 0.01f;
static float rotation = HALF_PI;
static constexpr float gravity = 12.0f;


bool draw_finished = 0;

VGA14Bit vga;

void initialize() {
  for (uint16_t x = 0; x < MAX_POINTS; ++x) {
    auto p = &points[x];

    p->x = (x % (GRID_X - 1) * fr * 2 + fr) << FIXED_POINT;
    p->y = h - ((x / (GRID_X - 1) * fr * 2 + fr) << FIXED_POINT);
    p->px = p->x + (rand() % 2) - 1;
    p->py = p->y + (rand() % 2) - 1;
    p->color = (vga.RGB(((float)p->x / (float)w * 255.0f),
                        ((float)(w - (p->y >> FIXED_POINT)) / (float)(h >> FIXED_POINT) * 255.0f),
                        255) & vga.RGBMask) | vga.SBits;
    //p->color = vga.RGB(rand() % 255, rand() % 255, rand() % 255);
  }
}


void prerender_circle() {
  for (int y = 0; y <= fr; ++y) {
    circle_lines[y] = (int)sqrt(fr * fr - y * y);
  }
}

void draw_fast_circles() {
  int16_t xr;

  for (int a = 1; a <= fr; ++a) {
    xr = circle_lines[a];
    for (int line = -xr; line <= xr; ++line) {
      for (uint16_t i = 0; i < MAX_POINTS; ++i) {
        auto p = &points[i];

        int x = (p->x >> FIXED_POINT) + line;
        int y = (p->y >> FIXED_POINT);

        unsigned int color = p->color;
        int new_x = x ^ 1;

        int new_y = y + a;
        if (new_y < vga.yres && new_y >= 0)
          vga.backBuffer[new_y][new_x] = color;

        new_y = y - a;
        if (new_y < vga.yres && new_y >= 0)
          vga.backBuffer[new_y][new_x] = color;
      }
    }
  }
  xr = *circle_lines;

  for (int line = -xr; line <= xr; ++line) {
    for (int i = 0; i < MAX_POINTS; ++i) {
      auto p = &points[i];
      unsigned int x = p->x >> FIXED_POINT;
      unsigned int y = p->y >> FIXED_POINT;
      if (x < vga.xres && y < vga.yres)
        vga.backBuffer[y][(x + line) ^ 1] = p->color;
    }
  }
}





void draw_function(void *param) {
  while (true) {
    while (draw_finished) delay(0);
    for (int y = 0; y < vga.yres; y++)
      memset(vga.backBuffer[y], 0, vga.xres * sizeof(uint16_t));

    draw_fast_circles();


    if (logger) {
      delta_time = (float)(delta_time * 200 + frame_time) / 201;
      vga.setFont(CodePage437_8x16);


      vga.setCursor(0, 0);
      vga.print("fps: ");
      vga.print((int)(1000.0f / delta_time));

      vga.setFont(Font6x8);

      vga.setCursor(0, 20);
      vga.print("frame_time:");
      vga.print(frame_time);
      vga.print("ms");
    }


    vga.show();

    draw_finished = 1;
  }
}


int isqrt(int num) {
  float x = (float)num;
  float xhalf = x * 0.5f;
  int i = *(int *)&x;
  i = 0x5f3759df - (i >> 1);
  x = *(float *)&i;
  x = 1.0f / (x * (1.5f - (xhalf * x * x)));
  return (int)x;
}


void update_points() {
  memset(grid_dict, 0, sizeof(grid_dict));

  rotation += rotating_speed;

  int16_t ax = cos(rotation) * gravity;
  int16_t ay = sin(rotation) * gravity;
  //ay = 0;

  for (int a = 0; a < MAX_POINTS; ++a) {
    auto pa = &points[a];

    int vx = pa->x - pa->px + ax;
    int vy = pa->y - pa->py + ay;

    pa->px = pa->x;
    pa->py = pa->y;

    pa->x += vx;
    pa->y += vy;


    pa->x = (pa->x < r) ? r : (pa->x > boundary_max_x) ? boundary_max_x : pa->x;
    pa->y = (pa->y < r) ? r : (pa->y > boundary_max_y) ? boundary_max_y : pa->y;


    int grid_x = pa->x / r2;
    int grid_y = pa->y / r2;


    int grid_x_start = max(grid_x - 1, 0);
    int grid_x_end = min(grid_x + 1, GRID_X - 1);
    int grid_y_start = max(grid_y - 1, 0);
    int grid_y_end = min(grid_y + 1, GRID_Y - 1);



    for (int x = grid_x_start; x <= grid_x_end; ++x) {
      for (int y = grid_y_start; y <= grid_y_end; ++y) {
        for (int u = 1; u <= *grid_dict[x][y]; ++u) {
          auto pb = &points[grid_dict[x][y][u]];

          int dx = pa->x - pb->x;
          if (dx >= r2 || dx <= -r2) continue;

          int dy = pa->y - pb->y;
          if (dy >= r2 || dy <= -r2) continue;

          int dot = dx * dx + dy * dy;
          if (dot == 0 || dot >= (r2 * r2)) continue;

          int l = isqrt(dot) - (fr * 10);
          int factor = ((l - r2) << 15) / l;

          dx = (dx * factor) >> 16;
          dy = (dy * factor) >> 16;

          pb->x += dx;
          pb->y += dy;

          pa->x -= dx;
          pa->y -= dy;
        }
      }
    }

    uint16_t *grid_ptr = grid_dict[grid_x][grid_y];
    if (*grid_ptr == GRID_MAX) continue;
    ++*grid_ptr;
    grid_ptr[*grid_ptr] = a;
  }
}

void setup() {
  disableCore0WDT();
  disableCore1WDT();

  vga.setFrameBufferCount(2);
  vga.init(vga.MODE200x150, vga.VGABlackEdition);
  Serial.begin(115200);

  prerender_circle();
  initialize();

  xTaskCreatePinnedToCore(draw_function, "draw_function", 2048, nullptr, 1, nullptr, 0);
}



void loop() {
  uint time = millis();
  frame_time = time - current_time;
  current_time = time;
  while (!draw_finished) delay(0);

  draw_finished = 0;


  update_points();
}

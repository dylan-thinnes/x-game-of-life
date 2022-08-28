#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <X11/Xlib.h>

int active_buffer = 0;
int buffer[2][100][100];
int window_x, window_y = 300;

void randomize () {
  for (int xx = 0; xx < 100; xx++) {
    for (int yy = 0; yy < 100; yy++) {
      buffer[active_buffer][xx][yy] = rand() % 2;
    }
  }
}

void step_state () {
  for (int xx = 0; xx < 100; xx++) {
    for (int yy = 0; yy < 100; yy++) {
      int neighbours = 0;
      for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
          if (dx == 0 && dy == 0) continue;
          int x = (xx + dx) % 100;
          int y = (yy + dy) % 100;
          neighbours += buffer[active_buffer][x][y];
        }
      }
      buffer[!active_buffer][xx][yy] =
        neighbours == 3 || (neighbours == 2 && buffer[active_buffer][xx][yy]);
    }
  }
  active_buffer = !active_buffer;
}

void redraw (Display* dpy, Window* win, GC* graphics_context) {
  int window_min = window_x < window_y ? window_x : window_y;
  int cell_size = window_min / 100;
  int offset_x = (window_x - cell_size * 100) / 2;
  int offset_y = (window_y - cell_size * 100) / 2;
  for (int xx = 0; xx < 100; xx++) {
    for (int yy = 0; yy < 100; yy++) {
      int x = offset_x + xx * cell_size;
      int y = offset_y + yy * cell_size;
      if (buffer[active_buffer][xx][yy]) {
        XFillRectangle(dpy, *win, *graphics_context, x, y, cell_size, cell_size);
      } else {
        XClearArea(dpy, *win, x, y, cell_size, cell_size, 0);
      }
    }
  }
}

int main (int argc, char** argv) {
  Display* dpy;
  int screen;
  Window win;
  XEvent event;
  GC graphics_context;

  dpy = XOpenDisplay(NULL);

  if (dpy == NULL) {
    exit(2);
  }

  screen = DefaultScreen(dpy);

  win = XCreateSimpleWindow(
    dpy, RootWindow(dpy, screen),
    100, 100, 500, 500,
    1, WhitePixel(dpy, screen), BlackPixel(dpy, screen)
  );

  graphics_context = XCreateGC(dpy, win, 0, 0);
  XSetForeground(dpy, graphics_context, WhitePixel(dpy, screen));
  XSetBackground(dpy, graphics_context, BlackPixel(dpy, screen));

  Mask mask = ExposureMask | KeyPressMask | StructureNotifyMask;
  XSelectInput(dpy, win, mask);
  XMapWindow(dpy, win);

  randomize();

  int finished = 0;
  while (!finished) {
    while (XCheckWindowEvent(dpy, win, mask, &event)) {
      switch (event.type) {
        case Expose:
          // Redraw on exposure
          redraw(dpy, &win, &graphics_context);
          break;
        case KeyPress:
          // Break if input char is 'q' or ESC
          char latin_mapping[2];
          XLookupString(&event.xkey, latin_mapping, 2, NULL, NULL);
          if (latin_mapping[0] == 'q' || latin_mapping[0] == '\033') {
            finished = 1;
            break;
          }

          step_state();
          redraw(dpy, &win, &graphics_context);
        case ConfigureNotify:
          // Save window size
          if (event.xconfigure.window == win) {
            window_x = event.xconfigure.width;
            window_y = event.xconfigure.height;
            redraw(dpy, &win, &graphics_context);
          }
          break;
        case DestroyNotify:
          // Exit when window destroyed
          finished = 1;
          break;
      }
    }
  }

  // Free everything at end
  XFreeGC(dpy, graphics_context);
  XDestroyWindow(dpy, win);
  XCloseDisplay(dpy);

  return 0;
}

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <stdint.h>

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
    0, 0, 200, 200, // top-left corner, 200 by 200
    0, // no border
    WhitePixel(dpy, screen), // white foreground
    BlackPixel(dpy, screen) // black background
  );

  graphics_context = XCreateGC(dpy, win, 0, 0);
  XSetForeground(dpy, graphics_context, WhitePixel(dpy, screen));
  XSetBackground(dpy, graphics_context, BlackPixel(dpy, screen));

  Mask mask = ExposureMask | KeyPressMask | StructureNotifyMask;
  XSelectInput(dpy, win, mask);
  XMapWindow(dpy, win);

  XIM xim = XOpenIM(dpy, NULL, NULL, NULL);
  XIC xic = XCreateIC(xim,
    XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
    XNClientWindow, win,
    XNFocusWindow, win, NULL
  );

  randomize();

  int finished = 0;
  int exposed = 0;
  int paused = 0;
  uint64_t elapsed = 0;
  uint64_t interval = 100000000L;
  while (!finished) {
    // Sleep 10ms
    struct timespec req, res;
    req.tv_sec = 0;
    req.tv_nsec = 10000000L;
    res.tv_sec = 0;
    res.tv_nsec = 0L;
    nanosleep(&req, &res);
    elapsed += req.tv_nsec - res.tv_nsec;

    while (XCheckWindowEvent(dpy, win, mask, &event)) {
      switch (event.type) {
        case Expose:
          // Only start drawing on exposure
          exposed = 1;
          break;
        case KeyPress:
          char latin_mapping[2];
          Xutf8LookupString(xic, &event.xkey, latin_mapping, 2, NULL, NULL);
          switch (latin_mapping[0]) {
            case 'q': // quit / escape
            case '\033':
              finished = 1;
              break;
            case ' ': // pause
            case 'p':
              paused = !paused;
              break;
            case 'r': // restart / randomize
              randomize();
              redraw(dpy, &win, &graphics_context);
              break;
            case 'j': // slow down
              interval += 10000000L;
              interval = interval < 1000000000L ? interval : 1000000000L;
              break;
            case 'k': // speed up
              interval -= 10000000L;
              interval = interval > 10000000L ? interval : 10000000L;
              break;
            default:
              break;
          }
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

    // Every 100ms (10 ticks, step state and redraw)
    if (elapsed > interval) {
      elapsed -= interval;
      if (exposed && !paused) {
        step_state();
        redraw(dpy, &win, &graphics_context);
      }
    }
  }

  // Free everything at end
  XFreeGC(dpy, graphics_context);
  XDestroyWindow(dpy, win);
  XCloseDisplay(dpy);

  return 0;
}

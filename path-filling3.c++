#include <iostream>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL2_rotozoom.h>
#include <vector>
#include <sys/time.h>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <unistd.h>

#define STITCH_COST 10
#define MINSTITCH 20
#define MAXSTITCH 70
#define MAXSTITCHERRORSQR 9
#define MINAREA 2
// #define MAXSTITCHERRORSQR 20
int SCALE = 3;
int DEFAULT_STITCH = 10;

using namespace std;

struct Path {
  struct Step {
    int x, y;
  };

  std::vector<Step> steps;
  unsigned int r, g, b;

  void render(SDL_Surface *s, SDL_Renderer *rnd) {
    SDL_SetRenderDrawColor(rnd, r, g, b, 255);

    int x = s->w / 2;
    int y = s->h / 2;

    for(Step &s: steps) {
      SDL_RenderDrawLine(rnd, x, y, x + s.x, y + s.y);
      x += s.x;
      y += s.y;
    }
  }
};

uint32_t &pixel(SDL_Surface *s, int x, int y) {
  return *reinterpret_cast<uint32_t *>(reinterpret_cast<uint8_t *>(s->pixels) + s->pitch * y + 4 * x);
}

uint64_t time() {
  struct timeval tv;
  gettimeofday(&tv, nullptr);

  return tv.tv_sec * 1000000ull + tv.tv_usec;
}

int remainingAreaRec(SDL_Surface *img, int x, int y, int area) {
  if(area <= 0) return 0;

  pixel(img, x, y) |= 0x0100;
  --area;

  for(int dy = -1; dy < 2; ++dy) {
    for(int dx = -1; dx < 2; ++dx) {
      if(y + dy < 0 || y + dy >= img->h || x + dx < 0 || x + dx >= img->w) continue;

      if(pixel(img, x + dx, y + dy) & 0xff && !(pixel(img, x + dx, y + dy) & 0xff00)) {
        area = remainingAreaRec(img, x + dx, y + dy, area);
      }
    }
  }

  return area;
}

bool hasAreaAbove(SDL_Surface *img, int x, int y, int area) {
  for(int y = 0; y < img->h; ++y) {
    for(int x = 0; x < img->w; ++x) {
      pixel(img, x, y) &= 0xffff00fful;
    }
  }

  return remainingAreaRec(img, x, y, area) == 0;
}

void moveToTopLeftRec(SDL_Surface *img, int &x, int &y, int sx, int sy) {
  pixel(img, sx, sy) |= 0x0100;
  if(sy < y || (sy == y && sx < x)) {
    y = sy;
    x = sx;
  }

  for(int dy = -1; dy < 2; ++dy) {
    for(int dx = -1; dx < 2; ++dx) {
      if(sy + dy < 0 || sy + dy >= img->h || sx + dx < 0 || sx + dx >= img->w) continue;

      if(pixel(img, sx + dx, sy + dy) & 0xff && !(pixel(img, sx + dx, sy + dy) & 0xff00)) {
        moveToTopLeftRec(img, x, y, sx + dx, sy + dy);
      }
    }
  }
}

void moveToTopLeft(SDL_Surface *img, int &x, int &y) {
  for(int sy = 0; sy < img->h; ++sy) {
    for(int sx = 0; sx < img->w; ++sx) {
      pixel(img, sx, sy) &= 0xffff00fful;
    }
  }

  moveToTopLeftRec(img, x, y, x, y);
}

bool findStart(SDL_Surface *img, int &cx, int &cy, int &ox, int &oy, Path &path) {
  int bestX = -1;
  int bestY = -1;
  int bestDist = 999999999;

  for(int y = 0; y < img->h; ++y) {
    for(int x = 0; x < img->w; ++x) {
      if(pixel(img, x, y) & 0xff) {
        int dist = (cx - x) * (cx - x) + (cy - y) * (cy - y);
        if(dist < bestDist) {
          bestX = x;
          bestY = y;
          bestDist = dist;
        }
      }
    }
  }

  if(bestX < 0) return false;

  std::cerr << "before: " << bestX << "," << bestY << std::endl;
  moveToTopLeft(img, bestX, bestY);
  std::cerr << "after: " << bestX << "," << bestY << std::endl;
  if(!(pixel(img, bestX, bestY) & 0xff)) {
    std::cerr << "wrong" << pixel(img, bestX, bestY) << std::endl;
  }

  cx = bestX;
  cy = bestY;
  if(ox > 0) path.steps.push_back({cx - ox, cy - oy});
  ox = cx;
  oy = cy;

  if(oy < 0 || ox < 0) {
    ox = cx;
    oy = cy;
  }
  return true;
}

int main(int argc, const char *const argv[]) {
  if(argc != 3) {
    cerr << "usage: ./path-filling <input image> <output.p>" << endl;
    return 1;
  }

  istringstream default_stitch(argv[1]);
  default_stitch >> DEFAULT_STITCH;

  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    cerr << "Could not init SDL: " << SDL_GetError() << endl;
    return 1;
  }

  const int allFormats = IMG_INIT_JPG | IMG_INIT_PNG | IMG_INIT_TIF;
  if(!IMG_Init(allFormats)) {
    cerr << "Could not init SDL_Image" << endl;
    return 1;
  }

  SDL_Surface *const rawImg = IMG_Load(argv[1]);
  if(!rawImg) {
    cerr << "Could not load image" << endl;
    return 1;
  }

  SDL_Surface *const img = SDL_CreateRGBSurface(0, rawImg->w, rawImg->h, 32, 0xff, 0xff00, 0xff0000, 0xff000000u);
  if(!img || !rawImg) {
    cerr << "Could not load image" << endl;
    return 1;
  }
  SDL_BlitSurface(rawImg, 0, img, 0);

  SDL_Window *const win = SDL_CreateWindow("path-guessing", 0, 0, img->w, img->h, 0);
  if(!win) {
    cerr << "Could not open window" << endl;
    return 1;
  }

  SDL_Surface *const screen = SDL_GetWindowSurface(win);
  if(!screen) {
    cerr << "Could not get screen surface" << endl;
    return 1;
  }

  atexit(SDL_Quit);

  Path path;

  SDL_Event event;
  uint64_t last = time();

  int cx = 0;
  int cy = 0;
  int ox = -1;
  int oy = -1;
#define MODE_RIGHT 0
#define MODE_RIGHTOVERSCAN 1
#define MODE_RIGHTBACKSCAN 2
#define MODE_LEFT 3
#define MODE_LEFTOVERSCAN 4
#define MODE_LEFTBACKSCAN 5
  int mode = 0;

  bool running = findStart(img, cx, cy, ox, oy, path);

  while(running) {
    while(SDL_PollEvent(&event)) {
      switch(event.type) {
        case SDL_QUIT: running = false;
      }
    }

    int maxStitch = (rand() % (MAXSTITCH - MINSTITCH)) + MINSTITCH;

    for(int i = 0; i < maxStitch; ++i) {
      switch(mode) {
        case MODE_RIGHT:
          pixel(img, cx, cy) = 0xff800000ul;
          if(pixel(img, cx + 1, cy) & 0xff) {
            ++cx;
          } else {
            ++cy;
            if(pixel(img, cx, cy) & 0xff) {
              mode = MODE_RIGHTOVERSCAN;
            } else {
              mode = MODE_RIGHTBACKSCAN;
            }
          }
          break;
        case MODE_RIGHTOVERSCAN:
          if(pixel(img, cx + 1, cy) & 0xff) {
            ++cx;
          } else {
            i = maxStitch;
            mode = MODE_LEFT;
          }
          break;
        case MODE_RIGHTBACKSCAN:
          if(pixel(img, cx - 1, cy) & 0xff) {
            --cx;
            i = maxStitch;
            mode = MODE_LEFT;
          } else if(pixel(img, cx - 1, cy - 1) & 0xff0000) {
            --cx;
          } else {
            running = findStart(img, cx, cy, ox, oy, path);
            mode = MODE_RIGHT;
          }
          break;
        case MODE_LEFT:
          pixel(img, cx, cy) = 0xff800000ul;
          if(pixel(img, cx - 1, cy) & 0xff) {
            --cx;
          } else {
            ++cy;
            if(pixel(img, cx, cy) & 0xff) {
              mode = MODE_LEFTOVERSCAN;
            } else {
              mode = MODE_LEFTBACKSCAN;
            }
          }
          break;
        case MODE_LEFTOVERSCAN:
          if(pixel(img, cx - 1, cy) & 0xff) {
            --cx;
          } else {
            i = maxStitch;
            mode = MODE_RIGHT;
          }
          break;
        case MODE_LEFTBACKSCAN:
          if(pixel(img, cx + 1, cy) & 0xff) {
            ++cx;
            i = maxStitch;
            mode = MODE_RIGHT;
          } else if(pixel(img, cx + 1, cy - 1) & 0xff0000) {
            ++cx;
          } else {
            running = findStart(img, cx, cy, ox, oy, path);
            mode = MODE_RIGHT;
          }
          break;
      }
    }

    path.steps.push_back({cx - ox, cy - oy});
    ox = cx;
    oy = cy;
    
//    RGBW n = rgbw;
//    n.mutate(tmp, stage);
//    ++stage;
//    n.render(tmp);
//    SDL_Surface *zoomed = simulateRGBView(tmp);
//    int64_t quality2 = rate(zoomedImg, zoomed) - rgbw.stitchCount() * STITCH_COST;

    if(time() > last + 1000000ull) {
      // SDL_BlitSurface(zoomedImg, 0, screen, 0);
      SDL_BlitSurface(img, 0, screen, 0);
      SDL_UpdateWindowSurface(win);

      last = time();
    }
  }

  ofstream vp3(argv[2]);
  vp3 << "White" << endl;
  for(Path::Step &s: path.steps) vp3 << s.x << " " << s.y << endl;

  SDL_BlitSurface(img, 0, screen, 0);
  SDL_UpdateWindowSurface(win);

  std::cout << "Total stitch count: " << path.steps.size() << std::endl;

  sleep(5);
}

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
#define MINAREA 9
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

int dirx(int dir) {
  static const int DX[8] = { -1, -1, 0, 1, 1, 1, 0, -1 };
  return DX[dir % 8];
}

int diry(int dir) {
  static const int DY[8] = { 0, -1, -1, -1, 0, 1, 1, 1 };
  return DY[dir % 8];
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
  bool running = true;
  uint64_t last = time();

  int cx = 0;
  int cy = 0;
  int ox = -1;
  int oy = -1;
  int dir = 0;

  while(running) {
    while(SDL_PollEvent(&event)) {
      switch(event.type) {
        case SDL_QUIT: running = false;
      }
    }

    if(!(pixel(img, cx, cy) & 0xff)) {
      bool found;

      int bestX;
      int bestY;
      int bestDist;

      do {
        found = true;

        bestX = -1;
        bestY = -1;
        bestDist = 999999999;

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

        if(bestX >= 0) {
          if(!hasAreaAbove(img, bestX, bestY, MINAREA)) {
            pixel(img, bestX, bestY) = 0xff00ff00ul;
            found = false;
          }
        }
      } while(!found);

      if(bestX >= 0) {
        cx = bestX;
        cy = bestY;
        if(oy < 0 || ox < 0) {
          ox = cx;
          oy = cy;
        }
      } else {
        break;
      }
    }

    int startDir = dir;
    int startX = cx;
    int startY = cy;

    int maxStitch = (rand() % (MAXSTITCH - MINSTITCH)) + MINSTITCH;

    for(int i = 0; i < maxStitch; ++i) {
      pixel(img, cx, cy) = 0xff800000ul;

      for(int j = 0; !(pixel(img, cx + dirx(dir), cy + diry(dir)) & 0xff) && j < 9; ++j, dir = (dir + 7) % 8);
      for(; pixel(img, cx + dirx(dir + 1), cy + diry(dir + 1)) & 0xff; dir = (dir + 1) % 8);
      cx += dirx(dir);
      cy += diry(dir);

      int straightX = startX + i * dirx(startDir);
      int straightY = startY + i * diry(startDir);

      if((straightX - cx) * (straightX - cx) + (straightY - cy) * (straightY - cy) > MAXSTITCHERRORSQR) {
        break;
      } else {
        if(!(pixel(img, cx, cy) & 0xff)) break;
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

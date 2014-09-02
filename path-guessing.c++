#include <iostream>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL2_rotozoom.h>
#include <vector>
#include <sys/time.h>
#include <algorithm>
#include <fstream>

#define STITCH_COST 10
#define SCALE 3
#define MAXSTITCH 20

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

  void mutate(SDL_Surface *) {
    if(steps.size() < 20) return;
    
    int i = 1 + (rand() % (steps.size() - 2));

    int dx = (rand() % (2 * MAXSTITCH + 1)) - MAXSTITCH;
    int dy = (rand() % (2 * MAXSTITCH + 1)) - MAXSTITCH;

    steps[i].x += dx;
    steps[i].y += dy;
    steps[i + 1].x -= dx;
    steps[i + 1].y -= dy;
  }

  void grow(SDL_Surface *) {
    int dx = (rand() % (2 * MAXSTITCH + 1)) - MAXSTITCH;
    int dy = (rand() % (2 * MAXSTITCH + 1)) - MAXSTITCH;

    int insert = 0;
    if(steps.size()) {
      insert = rand() % steps.size();
    }

    steps.push_back({0, 0});
    copy(steps.begin() + insert, steps.end(), steps.begin() + insert + 1);
    steps[insert] = {dx, dy};
    steps[insert + 1].x -= dx;
    steps[insert + 1].y -= dy;
  }
};

struct RGBW {
  Path r, g, b, w;

  RGBW() {
    r.r = 255; r.g =   0; r.b =   0;
    g.r =   0; g.g = 255; g.b =   0;
    b.r =   0; b.g =   0; b.b = 255;
    w.r = 255; w.g = 255; w.b = 255;
  }

  void render(SDL_Surface *s) {
    SDL_Renderer *rnd = SDL_CreateSoftwareRenderer(s);
    SDL_SetRenderDrawColor(rnd, 0, 0, 0, 255);
    SDL_RenderClear(rnd);

    w.render(s, rnd);
    r.render(s, rnd);
    g.render(s, rnd);
    b.render(s, rnd);

    SDL_RenderPresent(rnd);
    SDL_DestroyRenderer(rnd);
  }

  void mutate(SDL_Surface *s, int stage) {
    int max;

    switch(stage) {
      case 0: max = 2; break;
      case 1: max = 4; break;
      case 2: max = 6; break;
      case 3: max = 8; break;
      default: max = 8; break;
    }

    switch(rand() % max) {
      case 0: for(int i = 0; i < 1; ++i) w.grow(s); break;
      case 1: for(int i = 0; i < 1; ++i) w.mutate(s); break;
      case 2: for(int i = 0; i < 1; ++i) r.grow(s); break;
      case 3: for(int i = 0; i < 1; ++i) r.mutate(s); break;
      case 4: for(int i = 0; i < 1; ++i) g.grow(s); break;
      case 5: for(int i = 0; i < 1; ++i) g.mutate(s); break;
      case 6: for(int i = 0; i < 1; ++i) b.grow(s); break;
      case 7: for(int i = 0; i < 1; ++i) b.mutate(s); break;
    }
  }

  uint64_t stitchCount() {
    return r.steps.size() + g.steps.size() + b.steps.size() + w.steps.size();
  }

  void initPath(Path &p, SDL_Surface *s) {
    int distance = 50;

    float x = s->w / 2;
    float y = s->h / 2;
    int steps = 1;

    while(1) {
      for(int i = 0; i < steps; ++i) {
        p.steps.push_back({distance, 0});
        x += distance;
      }
      for(int i = 0; i < steps; ++i) {
        p.steps.push_back({0, distance});
        y += distance;
      }
      ++steps;
      for(int i = 0; i < steps; ++i) {
        p.steps.push_back({-distance, 0});
        x -= distance;
      }
      for(int i = 0; i < steps; ++i) {
        p.steps.push_back({0, -distance});
        y -= distance;
      }
      ++steps;
      if(x < 0 || x > s->w || y < 0 || y > s->h) break;
    }
  }

  void initPaths(SDL_Surface *s) {
    initPath(w, s);
    initPath(r, s);
    initPath(g, s);
    initPath(b, s);
  }

  void savePaths(ostream &o) {
    o << "White" << endl;
    for(Path::Step &s: w.steps) o << s.x << " " << s.y << endl;
    o << "Red" << endl;
    for(Path::Step &s: r.steps) o << s.x << " " << s.y << endl;
    o << "Green" << endl;
    for(Path::Step &s: g.steps) o << s.x << " " << s.y << endl;
    o << "Blue" << endl;
    for(Path::Step &s: b.steps) o << s.x << " " << s.y << endl;
  }
};

uint32_t &pixel(SDL_Surface *s, int x, int y) {
  return *reinterpret_cast<uint32_t *>(reinterpret_cast<uint8_t *>(s->pixels) + s->pitch * y + 4 * x);
}

int64_t rate(SDL_Surface *reference, SDL_Surface *rendered) {
  int64_t value = 0;

  SDL_LockSurface(rendered);
  SDL_LockSurface(reference);

  for(int y = 1; y < reference->h - 1; ++y) {
    for(int x = 1; x < reference->w - 1; ++x) {
      int r1 = (pixel(reference, x, y) & 0xff);
      int r2 = (pixel(rendered, x, y) & 0xff);
      int g1 = (pixel(reference, x, y) & 0xff00) >> 8;
      int g2 = (pixel(rendered, x, y) & 0xff00) >> 8;
      int b1 = (pixel(reference, x, y) & 0xff0000) >> 16;
      int b2 = (pixel(rendered, x, y) & 0xff0000) >> 16;

      value -= (r1 - r2) * (r1 - r2) +
               (g1 - g2) * (g1 - g2) +
               (b1 - b2) * (b1 - b2);
    }
  }

  SDL_UnlockSurface(reference);
  SDL_UnlockSurface(rendered);

  return value;
}

Path generateRandomPath(SDL_Surface *s, int r, int g, int b) {
  Path ret;
  ret.r = r;
  ret.g = g;
  ret.b = b;

  int x = s->w / 2;
  int y = s->h / 2;

  for(int i = 0; i < s->w * s->h / 10; ++i) {
    int rx = (rand() % 21) - 10;
    int ry = (rand() % 21) - 10;

    if(x + rx >= 0 && x + rx < s->w &&
        y + ry >= 0 && y + ry < s->h) {
      x += rx;
      y += ry;

      ret.steps.push_back({rx, ry});
    }
  }

  return ret;
}

uint64_t time() {
  struct timeval tv;
  gettimeofday(&tv, nullptr);

  return tv.tv_sec * 1000000ull + tv.tv_usec;
}

SDL_Surface *simulateRGBView(SDL_Surface *src) {
  SDL_Surface *ret = SDL_CreateRGBSurface(0, src->w / SCALE, src->h / SCALE, 32, 0xff, 0xff00, 0xff0000, 0xff000000u);

  SDL_LockSurface(ret);

  for(int y = 0; y < ret->h; ++y) {
    for(int x = 0; x < ret->w; ++x) {
      int r = 0, g = 0, b = 0;

      for(int yy = 0; yy < SCALE; ++yy) {
        for(int xx = 0; xx < SCALE; ++xx) {
          r += pixel(src, x * SCALE + xx, y * SCALE + yy) & 0xff;
          g += (pixel(src, x * SCALE + xx, y * SCALE + yy) & 0xff00) >> 8;
          b += (pixel(src, x * SCALE + xx, y * SCALE + yy) & 0xff0000) >> 16;
        }
      }

      pixel(ret, x, y) =
        r / (SCALE * SCALE) + 
        g / (SCALE * SCALE) * 0x100 + 
        b / (SCALE * SCALE) * 0x10000 +
        0xff000000ull;
    }
  }

  SDL_UnlockSurface(ret);
  return ret;
}

int main(int argc, const char *const argv[]) {
  if(argc != 3) {
    cerr << "usage: ./path-guessing <input image> <output.p>" << endl;
    return 1;
  }

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
  SDL_Surface *const img = SDL_CreateRGBSurface(0, rawImg->w, rawImg->h, 32, 0xff, 0xff00, 0xff0000, 0xff000000u);
  if(!img || !rawImg) {
    cerr << "Could not load image" << endl;
    return 1;
  }
  SDL_BlitSurface(rawImg, 0, img, 0);

  SDL_Surface *zoomedImg = simulateRGBView(img);

  SDL_Window *const win = SDL_CreateWindow("path-guessing", 0, 0, img->w / 3, img->h / 3, 0);
  if(!win) {
    cerr << "Could not open window" << endl;
    return 1;
  }

  SDL_Surface *const screen = SDL_GetWindowSurface(win);
  if(!screen) {
    cerr << "Could not get screen surface" << endl;
    return 1;
  }

  SDL_Surface *const tmp = SDL_CreateRGBSurface(0, img->w, img->h, 32, 0xff, 0xff00, 0xff0000, 0xff000000u);

  RGBW rgbw;
  rgbw.initPaths(tmp);

  int64_t quality = -9999999999ll;

  SDL_Event event;
  bool running = true;
  uint64_t last = time();

  int stage = 0;

  while(running) {
    while(SDL_PollEvent(&event)) {
      switch(event.type) {
        case SDL_QUIT: running = false;
      }
    }

    RGBW n = rgbw;
    n.mutate(tmp, stage);
    ++stage;
    n.render(tmp);
    SDL_Surface *zoomed = simulateRGBView(tmp);
    int64_t quality2 = rate(zoomedImg, zoomed) - rgbw.stitchCount() * STITCH_COST;

    if(quality2 > quality) {
      rgbw = n;
      quality = quality2;
    }

    if(time() > last + 1000000ull) {
      cout << quality << " / " << quality2 << " / " << stage << " / " << rgbw.stitchCount() << endl;

      // SDL_BlitSurface(zoomedImg, 0, screen, 0);
      SDL_BlitSurface(zoomed, 0, screen, 0);
      SDL_UpdateWindowSurface(win);

      ofstream vp3(argv[2]);
      rgbw.savePaths(vp3);

      last = time();
    }
    
    SDL_FreeSurface(zoomed);
  }

  atexit(SDL_Quit);
}

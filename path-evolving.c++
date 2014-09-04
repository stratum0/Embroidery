#include <iostream>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL2_rotozoom.h>
#include <vector>
#include <sys/time.h>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdlib.h>

#define STITCH_COST 10
#define MAXSTITCH 20
#define POPULATION_SIZE 128
#define NO_QUALITY -999999999999ll
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
    copy(steps.begin() + insert, steps.end() - 1, steps.begin() + insert + 1);
    steps[insert] = {dx, dy};
    steps[insert + 1].x -= dx;
    steps[insert + 1].y -= dy;
  }

  void shrink(SDL_Surface *) {
    if(steps.size() < 2) return;

    int i = rand() % (steps.size() - 1);
    steps[i + 1].x += steps[i].x;
    steps[i + 1].y += steps[i].y;
    copy(steps.begin() + i + 1, steps.end(), steps.begin() + i);
    steps.pop_back();
  }
};

struct RGBW {
  Path r, g, b, w;
  int64_t quality;

  RGBW() {
    r.r = 255; r.g =   0; r.b =   0;
    g.r =   0; g.g = 255; g.b =   0;
    b.r =   0; b.g =   0; b.b = 255;
    w.r = 255; w.g = 255; w.b = 255;

    quality = NO_QUALITY;
  }

  int64_t getQuality() const { return quality; }
  void setQuality(int64_t q) { quality = q; }

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

  void mutate(SDL_Surface *s) {
    switch(rand() % 4) {
      case 0: w.mutate(s); break;
      case 1: r.mutate(s); break;
      case 2: g.mutate(s); break;
      case 3: b.mutate(s); break;
    }

    setQuality(NO_QUALITY);
  }

  void grow(SDL_Surface *s) {
    switch(rand() % 4) {
      case 0: w.grow(s); break;
      case 1: r.grow(s); break;
      case 2: g.grow(s); break;
      case 3: b.grow(s); break;
    }

    setQuality(NO_QUALITY);
  }

  void shrink(SDL_Surface *s) {
    switch(rand() % 4) {
      case 0: w.shrink(s); break;
      case 1: r.shrink(s); break;
      case 2: g.shrink(s); break;
      case 3: b.shrink(s); break;
    }

    setQuality(NO_QUALITY);
  }

  uint64_t stitchCount() {
    return r.steps.size() + g.steps.size() + b.steps.size() + w.steps.size();
  }

  void initPath(Path &p, SDL_Surface *s) {
    int distance = 5 * DEFAULT_STITCH;

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

    for(int i = 0; i < 100; ++i) p.mutate(s);
    for(int i = 0; i < 200; ++i) p.grow(s);
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

uint32_t badpixel;

uint32_t &pixel(SDL_Surface *s, int x, int y) {
  if(x < 0 || y < 0 || x >= s->w || y >= s->h) return badpixel;
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

  for(int i = 0; i < s->w * s->h / DEFAULT_STITCH; ++i) {
    int rx = (rand() % (2 * DEFAULT_STITCH + 1)) - DEFAULT_STITCH;
    int ry = (rand() % (2 * DEFAULT_STITCH + 1)) - DEFAULT_STITCH;

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

void simulateRGBView(SDL_Surface *src, SDL_Surface *dst) {
  SDL_LockSurface(src);
  SDL_LockSurface(dst);

  for(int y = 0; y < dst->h; ++y) {
    for(int x = 0; x < dst->w; ++x) {
      int r = 0, g = 0, b = 0;

      for(int yy = 0; yy < SCALE; ++yy) {
        for(int xx = 0; xx < SCALE; ++xx) {
          r += pixel(src, x * SCALE + xx, y * SCALE + yy) & 0xff;
          g += (pixel(src, x * SCALE + xx, y * SCALE + yy) & 0xff00) >> 8;
          b += (pixel(src, x * SCALE + xx, y * SCALE + yy) & 0xff0000) >> 16;
        }
      }

      pixel(dst, x, y) =
        r / (SCALE * SCALE) + 
        g / (SCALE * SCALE) * 0x100 + 
        b / (SCALE * SCALE) * 0x10000 +
        0xff000000ull;
    }
  }

  SDL_UnlockSurface(dst);
  SDL_UnlockSurface(src);
}

RGBW cross(SDL_Surface *, RGBW &a, RGBW &b) {
  RGBW ret = a;
  ret.setQuality(NO_QUALITY);

  Path *ap = 0, *bp = 0, *rp = 0;

  switch(rand() % 4) {
    case 0: ap = &a.w; bp = &b.w; rp = &ret.w; break;
    case 1: ap = &a.r; bp = &b.r; rp = &ret.r; break;
    case 2: ap = &a.g; bp = &b.g; rp = &ret.g; break;
    case 3: ap = &a.b; bp = &b.b; rp = &ret.b; break;
  }

  vector<pair<size_t, size_t>> possiblePoints;

  int ax = 0;
  int ay = 0;

  for(size_t ai = 0; ai < ap->steps.size(); ++ai) {
    ax += ap->steps[ai].x;
    ay += ap->steps[ai].y;

    int bx = 0;
    int by = 0;

    for(size_t bi = 0; bi < bp->steps.size(); ++bi) {
      bx += bp->steps[bi].x;
      by += bp->steps[bi].y;

      if((ax - bx) * (ax - bx) < DEFAULT_STITCH * DEFAULT_STITCH &&
         (ay - by) * (ay - by) < DEFAULT_STITCH * DEFAULT_STITCH) {
        possiblePoints.push_back({ai, bi});
      }
    }
  }

  if(possiblePoints.empty()) return ret;

  auto &crossover = possiblePoints[rand() % possiblePoints.size()];

  while(rp->steps.size() > crossover.first) {
    rp->steps.pop_back();
  }

  ax = ay = 0;

  int bx = 0;
  int by = 0;

  for(auto &s: rp->steps) {
    ax += s.x;
    ay += s.y;
  }

  for(size_t i = 0; i < crossover.second; ++i) {
    bx += bp->steps[i].x;
    by += bp->steps[i].y;
  }

  rp->steps.push_back({bx - ax, by - ay});

  for(size_t i = crossover.second; i < bp->steps.size(); ++i) {
    rp->steps.push_back(bp->steps[i]);
  }

  return ret;
}

int main(int argc, const char *const argv[]) {
  if(argc != 5) {
    cerr << "usage: ./path-evolving <scale reduction (try 3)> <default stitch size (try 10)> <input image> <output.p>" << endl;
    return 1;
  }

  istringstream scale(argv[1]);
  scale >> SCALE;

  istringstream default_stitch(argv[2]);
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

  SDL_Surface *const rawImg = IMG_Load(argv[3]);
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

  SDL_Surface *const zoomedImg = SDL_CreateRGBSurface(0, img->w / SCALE, img->h / SCALE, 32, 0xff, 0xff00, 0xff0000, 0xff000000u);
  simulateRGBView(img, zoomedImg);

  SDL_Window *const win = SDL_CreateWindow("path-evolving", 0, 0, img->w / SCALE, img->h / SCALE, 0);
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
  SDL_Surface *const zoomed = SDL_CreateRGBSurface(0, img->w / SCALE, img->h / SCALE, 32, 0xff, 0xff00, 0xff0000, 0xff000000u);

  std::vector<RGBW> population;

  for(int i = 0; i < POPULATION_SIZE; ++i) {
    population.push_back(RGBW());
    population.back().initPaths(tmp);
  }

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

    for(RGBW &rgbw: population) {
      if(rgbw.getQuality() == NO_QUALITY) {
        rgbw.render(tmp);
        simulateRGBView(tmp, zoomed);
        rgbw.setQuality(rate(zoomedImg, zoomed) - rgbw.stitchCount() * STITCH_COST);
      }
    }

    sort(population.begin(), population.end(), [](const RGBW &a, const RGBW &b) {
        return a.getQuality() < b.getQuality();
    });

    if(time() > last + 1000000ull) {
      cout << population.front().getQuality() << " - " << population.back().getQuality() <<
        " / " << stage << " / " << population.back().stitchCount() << endl;

      population.back().render(tmp);
      simulateRGBView(tmp, zoomed);
      SDL_BlitSurface(zoomed, 0, screen, 0);
      SDL_UpdateWindowSurface(win);

      ofstream vp3(argv[4]);
      population.back().savePaths(vp3);

      last = time();
    }

    for(int i = 0; i < POPULATION_SIZE / 2; ++i) {
      int sel = rand() % 100;
      int ai = POPULATION_SIZE / 2 + (rand() % (POPULATION_SIZE / 2));

      if(sel < 10) {
        population[i] = population[ai];
        for(int j = 0; j < 10; ++j) population[i].mutate(img);
      } else if(sel < 20) {
        population[i] = population[ai];
        for(int j = 0; j < 10; ++j) population[i].grow(img);
      } else if(sel < 40) {
        population[i] = population[ai];
        population[i].mutate(img);
      } else if(sel < 60) {
        population[i] = population[ai];
        population[i].grow(img);
      } else if(sel < 80) {
        population[i] = population[ai];
        population[i].shrink(img);
      } else {
        int bi = POPULATION_SIZE / 2 + (rand() % (POPULATION_SIZE / 2));

        population[i] = cross(img, population[ai], population[bi]);
      }
    }
  }

  atexit(SDL_Quit);
}

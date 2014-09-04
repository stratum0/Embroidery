#include <iostream>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL2_rotozoom.h>
#include <vector>
#include <sys/time.h>
#include <algorithm>
#include <fstream>
#include <sstream>

#define STITCH_COST 50
#define PIXEL_COST 10
#define MAXSTITCH 20
#define EDGEDETECT_SMEAR 0.7
#define EDGEDETECT_NEW 0.8
int SCALE = 2;
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
    copy(steps.begin() + insert, steps.end(), steps.begin() + insert + 1);
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
      default: max = 12; break;
    }

    switch(rand() % max) {
      case  0: for(int i = 0; i < 1; ++i) w.grow(s); break;
      case  1: for(int i = 0; i < 1; ++i) w.mutate(s); break;
      case  2: for(int i = 0; i < 1; ++i) w.shrink(s); break;
      case  3: for(int i = 0; i < 1; ++i) r.grow(s); break;
      case  4: for(int i = 0; i < 1; ++i) r.mutate(s); break;
      case  5: for(int i = 0; i < 1; ++i) r.shrink(s); break;
      case  6: for(int i = 0; i < 1; ++i) g.grow(s); break;
      case  7: for(int i = 0; i < 1; ++i) g.mutate(s); break;
      case  8: for(int i = 0; i < 1; ++i) g.shrink(s); break;
      case  9: for(int i = 0; i < 1; ++i) b.grow(s); break;
      case 10: for(int i = 0; i < 1; ++i) b.mutate(s); break;
      case 11: for(int i = 0; i < 1; ++i) b.shrink(s); break;
    }
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

uint32_t value(SDL_Surface *s, int x, int y) {
  return (pixel(s, x, y) & 0xff) +
    ((pixel(s, x, y) & 0xff00) >> 8) +
    ((pixel(s, x, y) & 0xff0000) >> 16);
}

int64_t rateStitchDirection(const Path &p, SDL_Surface *edgedImg) {
  int x = 0;
  int y = 0;
  int64_t result = 0;

  for(auto &s: p.steps) {
    int mx = x + s.x / 2;
    int my = y + s.y / 2;

    if(mx < 0 || mx >= edgedImg->w || my < 0 || my > edgedImg->h) {
      result -= 1000 * STITCH_COST;
    } else {
      float mag = pixel(edgedImg, mx, my) & 0xff;
      float dir = ((pixel(edgedImg, mx, my) & 0xff00) >> 8) * 2 * M_PI / 255 - M_PI;

      // normal vector
      float edgeX = mag * cosf(dir);
      float edgeY = mag * sinf(dir);

      float dot = fabs(s.x * edgeX + s.y * edgeY);
      float delta;

      if(mag < 10) {
        delta = STITCH_COST;
      } else {
        delta = STITCH_COST * dot / mag / sqrtf(s.x * s.x + s.y * s.y);
      }

      int64_t idelta = delta;

      if(idelta < 0 || idelta > 1000 * STITCH_COST) {
        // things have gone wrong with the floating point, this is probably not a good stitch
        idelta = 1000 * STITCH_COST;
      }

      result -= idelta;
    }

    x += s.x;
    y += s.y;
  }

  if(result > 0) {
    cout << result << endl;
  }

  return result;
}

int64_t rateStitchDirection(const RGBW &rgbw, SDL_Surface *edges) {
  return
    rateStitchDirection(rgbw.w, edges) +
    rateStitchDirection(rgbw.r, edges) +
    rateStitchDirection(rgbw.g, edges) +
    rateStitchDirection(rgbw.b, edges);
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

      int delta = (r1 - r2) * (r1 - r2) +
                  (g1 - g2) * (g1 - g2) +
                  (b1 - b2) * (b1 - b2);

      value -= PIXEL_COST * delta;
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

SDL_Surface *simulateRGBView(SDL_Surface *src) {
  SDL_Surface *ret = SDL_CreateRGBSurface(0, src->w / SCALE, src->h / SCALE, 32, 0xff, 0xff00, 0xff0000, 0xff000000u);

  SDL_LockSurface(ret);

  for(int y = 0; y < ret->h; ++y) {
    for(int x = 0; x < ret->w; ++x) {
      int r = 0, g = 0, b = 0;

      int count = 0;

      for(int yy = -SCALE; yy < SCALE + 1; ++yy) {
        for(int xx = -SCALE; xx < SCALE + 1; ++xx) {
          const int px = x * SCALE + xx;
          const int py = y * SCALE + yy;

          if(px < 0 || py < 0 || px >= src->w || py >= src->h) continue;

          r += pixel(src, px, py) & 0xff;
          g += (pixel(src, px, py) & 0xff00) >> 8;
          b += (pixel(src, px, py) & 0xff0000) >> 16;
          ++count;
        }
      }

      pixel(ret, x, y) =
        r / count + 
        g / count * 0x100 + 
        b / count * 0x10000 +
        0xff000000ull;
    }
  }

  SDL_UnlockSurface(ret);
  return ret;
}

SDL_Surface *findEdges(SDL_Surface *src) {
  SDL_Surface *ret = SDL_CreateRGBSurface(0, src->w, src->h, 32, 0xff, 0xff00, 0xff0000, 0xff000000u);

  SDL_LockSurface(ret);

  for(int y = 1; y < ret->h - 1; ++y) {
    for(int x = 1; x < ret->w - 1; ++x) {
      float dx, dy;
      int r, g, b;

      dx = 0.0 + value(src, x - 1, y) + value(src, x - 1, y - 1) + value(src, x - 1, y + 1) -
          (value(src, x + 1, y) + value(src, x + 1, y - 1) + value(src, x + 1, y + 1));

      dy = 0.0 + value(src, x - 1, y - 1) + value(src, x, y - 1) + value(src, x + 1, y - 1) -
          (value(src, x - 1, y + 1) + value(src, x, y + 1) + value(src, x + 1, y + 1));

      r = sqrt(dx * dx + dy * dy) / 6;
      g = (M_PI + atan2f(dy, dx)) * 255 / (2*M_PI);
      b = 0;

      pixel(ret, x, y) =
        r + 
        g * 0x100 + 
        b * 0x10000 +
        0xff000000ull;
    }
  }

  for(int i = 0; i < 10; ++i) {
    for(int y = 1; y < ret->h - 1; ++y) {
      for(int x = 1; x < ret->w - 1; ++x) {
        int bx = -1;
        int by = -1;
        int bv = pixel(ret, x, y) & 0xff;

        for(int yy = -1; yy < 2; ++yy) {
          for(int xx = -1; xx < 2; ++xx) {
            if((pixel(ret, x + xx, y + yy) & 0xff) * EDGEDETECT_SMEAR > bv) {
              bx = x + xx;
              by = y + yy;
              bv = (pixel(ret, x + xx, y + yy) & 0xff) * EDGEDETECT_SMEAR;
            }
          }
        }

        if(bx >= 0) {
          pixel(ret, x, y) =
            (static_cast<int>((pixel(ret, bx, by) & 0xff) * EDGEDETECT_NEW) & 0xff) +
            (static_cast<int>((pixel(ret, x, y) & 0xff) * (1 - EDGEDETECT_NEW)) & 0xff) +
            (static_cast<int>((pixel(ret, bx, by) & 0xff00) * EDGEDETECT_NEW) & 0xff00) +
            (static_cast<int>((pixel(ret, x, y) & 0xff00) * (1 - EDGEDETECT_NEW)) & 0xff00) +
            (static_cast<int>((pixel(ret, bx, by) & 0xff0000) * EDGEDETECT_NEW) & 0xff0000) +
            (static_cast<int>((pixel(ret, x, y) & 0xff0000) * (1 - EDGEDETECT_NEW)) & 0xff0000) +
            0xff000000ull;
        }
      }
    }
  }

  SDL_UnlockSurface(ret);
  return ret;
}

int main(int argc, const char *const argv[]) {
  if(argc != 5) {
    cerr << "usage: ./path-guessing2 <scale reduction (try 2)> <default stitch size (try 10)> <input image> <output.p>" << endl;
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

  SDL_Surface *zoomedImg = simulateRGBView(img);
  SDL_Surface *edgedImg = findEdges(img);

  SDL_Window *const win = SDL_CreateWindow("path-guessing2", 0, 0, img->w / SCALE, img->h / SCALE, 0);
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

  int64_t quality = -999999999999999ll;

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
    int64_t pixelQuality = rate(zoomedImg, zoomed);
    int64_t stitchQuality = rateStitchDirection(rgbw, edgedImg);
    int64_t quality2 = pixelQuality + stitchQuality;

    if(quality2 > quality) {
      rgbw = n;
      quality = quality2;
    }

    if(time() > last + 1000000ull) {
      cout << quality << " / " << quality2 << " / " << stage << " / " << rgbw.stitchCount() << "  " <<
        pixelQuality << "|" << stitchQuality << " => " << pixelQuality * 1.0 / stitchQuality << endl;

      // SDL_BlitSurface(edgedImg, 0, screen, 0);
      // SDL_BlitSurface(zoomedImg, 0, screen, 0);
      SDL_BlitSurface(zoomed, 0, screen, 0);
      SDL_UpdateWindowSurface(win);

      ofstream vp3(argv[4]);
      rgbw.savePaths(vp3);

      last = time();
    }
    
    SDL_FreeSurface(zoomed);
  }

  atexit(SDL_Quit);
}

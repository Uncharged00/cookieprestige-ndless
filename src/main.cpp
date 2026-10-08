#include <SDL/SDL.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define W 320
#define H 240
#define BUILDINGS 20
#define UPGRADES 24
#define SAVEFILE "cookieprestige.sav"

struct Building {
    const char *name;
    double base;
    double cps;
    int owned;
};

struct Upgrade {
    const char *name;
    int cost;
    int type;
    bool bought;
};

static Building b[BUILDINGS] = {
    {"Cursor",15,0.1,0},
    {"Grandma",100,1,0},
    {"Farm",1100,8,0},
    {"Mine",12000,47,0},
    {"Factory",130000,260,0},
    {"Bank",1400000,1400,0},
    {"Temple",20000000,7800,0},
    {"Wizard Tower",330000000,44000,0},
    {"Shipment",5100000000.0,260000,0},
    {"Alchemy Lab",75000000000.0,1600000,0},
    {"Portal",1000000000000.0,10000000,0},
    {"Time Machine",14000000000000.0,65000000,0},
    {"Antimatter Condenser",170000000000000.0,430000000,0},
    {"Prism",2100000000000000.0,2900000000,0},
    {"Chancemaker",7700000000000000.0,21000000000.0,0},
    {"Fractal Engine",51000000000000000.0,150000000000.0,0},
    {"Javascript Console",750000000000000000.0,1100000000000.0,0},
    {"Idleverse",12000000000000000000.0,8300000000000.0,0},
    {"Cortex Baker",190000000000000000000.0,64000000000000.0,0},
    {"You",3300000000000000000000.0,510000000000000.0,0}
};

static Upgrade u[UPGRADES] = {
    {"Reinforced index finger",100,0,false},
    {"Carpal tunnel prevention cream",400,0,false},
    {"Ambidextrous",10000,0,false},
    {"Thousand fingers",100000,0,false},
    {"Forwards from grandma",10000,1,false},
    {"Steel-plated rolling pins",50000,1,false},
    {"Lubricated dentures",1000000,1,false},
    {"Prune juice",5000000,1,false},
    {"Cheap hoes",110000,2,false},
    {"Fertilizer",550000,2,false},
    {"Cookie trees",5500000,2,false},
    {"Ritual rolling pins",55000000,2,false},
    {"Sugar gas",1200000,3,false},
    {"Megadrill",12000000,3,false},
    {"Ultimadrill",120000000,3,false},
    {"Bore again",1200000000,3,false},
    {"Sturdier conveyor belts",1300000,4,false},
    {"Child labor",13000000,4,false},
    {"Sweatshop",130000000,4,false},
    {"Radium reactors",14000000,5,false},
    {"Recombobulators",140000000,5,false},
    {"Deep-bake process",1400000000,5,false},
    {"Cyborg workforce",14000000000,5,false},
    {"Megacorps",140000000000,5,false}
};

static SDL_Surface *screen;
static double cookies = 0;
static double allTime = 0;
static double clicks = 0;
static double prestige = 0;
static double heavenly = 0;

static bool golden = false;
static bool goldenSwitch = false;
static Uint32 goldenUntil = 0;
static Uint32 nextGolden = 30000;

static int tab = 0;
static int cursor = 0;
static Uint32 lastTick = 0;
static time_t lastSave = 0;

static double cps() {
    double x = 0;

    for (int i=0;i<BUILDINGS;i++)
        x += b[i].owned * b[i].cps;

    for (int i=0;i<UPGRADES;i++)
        if (u[i].bought)
            x *= 1.1;

    if (goldenSwitch)
        x *= 1.5;

    return x;
}

static double buildingCost(int i) {
    return b[i].base * pow(1.15, b[i].owned);
}

static double prestigeFor(double x) {
    if (x < 1000000000000.0)
        return 0;
    return floor(pow(x / 1000000000000.0, 1.0/3.0));
}

static void text(SDL_Surface *s, int x, int y, const char *str) {
    /* Minimal calculator-safe text renderer.
       Replaceable by nSDL font rendering later. */
    (void)s;
    (void)x;
    (void)y;
    (void)str;
}

static void saveGame() {
    FILE *f = fopen(SAVEFILE,"wb");
    if (!f) return;

    fwrite(&cookies,sizeof(cookies),1,f);
    fwrite(&allTime,sizeof(allTime),1,f);
    fwrite(&clicks,sizeof(clicks),1,f);
    fwrite(&prestige,sizeof(prestige),1,f);
    fwrite(&heavenly,sizeof(heavenly),1,f);

    fwrite(&goldenSwitch,sizeof(goldenSwitch),1,f);

    for (int i=0;i<BUILDINGS;i++)
        fwrite(&b[i].owned,sizeof(int),1,f);

    for (int i=0;i<UPGRADES;i++)
        fwrite(&u[i].bought,sizeof(bool),1,f);

    lastSave = time(NULL);
    fwrite(&lastSave,sizeof(lastSave),1,f);

    fclose(f);
}

static void loadGame() {
    FILE *f = fopen(SAVEFILE,"rb");
    if (!f) return;

    fread(&cookies,sizeof(cookies),1,f);
    fread(&allTime,sizeof(allTime),1,f);
    fread(&clicks,sizeof(clicks),1,f);
    fread(&prestige,sizeof(prestige),1,f);
    fread(&heavenly,sizeof(heavenly),1,f);

    fread(&goldenSwitch,sizeof(goldenSwitch),1,f);

    for (int i=0;i<BUILDINGS;i++)
        fread(&b[i].owned,sizeof(int),1,f);

    for (int i=0;i<UPGRADES;i++)
        fread(&u[i].bought,sizeof(bool),1,f);

    time_t saved;
    fread(&saved,sizeof(saved),1,f);

    time_t now = time(NULL);
    double offline = difftime(now,saved);

    if (offline > 0 && offline < 604800) {
        double gain = cps() * offline * 0.85;
        cookies += gain;
        allTime += gain;
    }

    fclose(f);
}

static void clickCookie() {
    double gain = 1.0;

    if (u[0].bought) gain *= 2;
    if (u[1].bought) gain *= 2;
    if (u[2].bought) gain *= 2;

    cookies += gain;
    allTime += gain;
    clicks++;
}

static void buyBuilding() {
    int i = cursor;
    double c = buildingCost(i);

    if (cookies >= c) {
        cookies -= c;
        b[i].owned++;
    }
}

static void buyUpgrade() {
    if (cursor < 0 || cursor >= UPGRADES)
        return;

    Upgrade &x = u[cursor];

    if (!x.bought && cookies >= x.cost) {
        cookies -= x.cost;
        x.bought = true;
    }
}

static void goldenCookie() {
    if (!golden) return;

    golden = false;

    double gain = cps() * 60.0;

    if (gain < 777.0)
        gain = 777.0;

    cookies += gain;
    allTime += gain;
}

static void spawnGolden(Uint32 now) {
    if (golden) return;

    if ((int)(now - lastTick) > (int)nextGolden) {
        golden = true;
        goldenUntil = now + 12000;
        nextGolden = 20000 + (rand() % 25000);
        lastTick = now;
    }
}

static void ascend() {
    double newPrestige = prestigeFor(allTime);

    if (newPrestige <= prestige)
        return;

    heavenly += newPrestige - prestige;
    prestige = newPrestige;

    cookies = 0;

    for (int i=0;i<BUILDINGS;i++)
        b[i].owned = 0;

    for (int i=0;i<UPGRADES;i++)
        u[i].bought = false;
}

static void draw() {
    SDL_FillRect(screen,NULL,SDL_MapRGB(screen->format,255,255,255));

    /*
       The game is intentionally keyboard-driven so it works on the
       CX II-T without requiring touchscreen interaction.

       Actual nSDL text/font drawing will be added after the core
       game is compiled and tested on the calculator.
    */

    SDL_Flip(screen);
}

int main() {
    srand((unsigned)time(NULL));

    if (SDL_Init(SDL_INIT_VIDEO) < 0)
        return 1;

    screen = SDL_SetVideoMode(W,H,16,SDL_SWSURFACE);

    if (!screen)
        return 1;

    loadGame();

    bool running = true;
    lastTick = SDL_GetTicks();

    while (running) {
        Uint32 now = SDL_GetTicks();

        double dt = (now - lastTick) / 1000.0;

        if (dt > 0 && dt < 10) {
            double gain = cps() * dt;
            cookies += gain;
            allTime += gain;
        }

        lastTick = now;

        spawnGolden(now);

        if (golden && now > goldenUntil)
            golden = false;

        SDL_Event e;

        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT)
                running = false;

            if (e.type == SDL_KEYDOWN) {
                SDLKey k = e.key.keysym.sym;

                if (k == SDLK_ESCAPE) {
                    saveGame();
                    running = false;
                }

                else if (k == SDLK_SPACE) {
                    if (golden)
                        goldenCookie();
                    else
                        clickCookie();
                }

                else if (k == SDLK_RETURN) {
                    if (tab == 0)
                        buyBuilding();
                    else
                        buyUpgrade();
                }

                else if (k == SDLK_UP) {
                    cursor--;

                    if (cursor < 0)
                        cursor = (tab == 0 ? BUILDINGS : UPGRADES) - 1;
                }

                else if (k == SDLK_DOWN) {
                    cursor++;

                    if (cursor >= (tab == 0 ? BUILDINGS : UPGRADES))
                        cursor = 0;
                }

                else if (k == SDLK_LEFT) {
                    tab = 0;
                    cursor = 0;
                }

                else if (k == SDLK_RIGHT) {
                    tab = 1;
                    cursor = 0;
                }

                else if (k == SDLK_a) {
                    ascend();
                }

                else if (k == SDLK_s) {
                    saveGame();
                }

                else if (k == SDLK_g) {
                    goldenSwitch = !goldenSwitch;
                }
            }
        }

        draw();

        SDL_Delay(20);
    }

    saveGame();
    SDL_Quit();

    return 0;
}

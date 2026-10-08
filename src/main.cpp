#include <SDL/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <ctype.h>

#define W 320
#define H 240
#define BUILDINGS 20
#define NORMAL_UPGRADES 30
#define HEAVENLY_UPGRADES 9
#define ACHIEVEMENTS 20
#define SAVEFILE "cookieprestige.sav"

struct Num {
    double m;
    int e;

    Num(double v = 0.0) {
        if (v <= 0.0) { m = 0.0; e = 0; }
        else {
            e = (int)floor(log10(v));
            m = v / pow(10.0, e);
            normalize();
        }
    }

    Num(double mm, int ee) : m(mm), e(ee) { normalize(); }

    void normalize() {
        if (m == 0.0) { e = 0; return; }
        while (m >= 10.0) { m /= 10.0; ++e; }
        while (m < 1.0) { m *= 10.0; --e; }
    }
};

static Num numAdd(Num a, Num b) {
    if (a.m == 0.0) return b;
    if (b.m == 0.0) return a;
    if (a.e < b.e) { Num t = a; a = b; b = t; }
    int d = a.e - b.e;
    if (d > 15) return a;
    return Num(a.m + b.m / pow(10.0, d), a.e);
}

static Num numSub(Num a, Num b) {
    if (a.m == 0.0 || (a.e < b.e) || (a.e == b.e && a.m < b.m))
        return Num();
    int d = a.e - b.e;
    if (d > 15) return a;
    return Num(a.m - b.m / pow(10.0, d), a.e);
}

static Num numMul(Num a, double x) {
    if (a.m == 0.0 || x <= 0.0) return Num();
    return Num(a.m * x, a.e);
}

static bool numGE(Num a, Num b) {
    return a.e > b.e || (a.e == b.e && a.m >= b.m);
}

static double numLog10(Num a) {
    return a.m == 0.0 ? -INFINITY : log10(a.m) + a.e;
}

static Num numFromLog10(double x) {
    if (!isfinite(x)) return Num();
    double e = floor(x);
    return Num(pow(10.0, x - e), (int)e);
}

static void formatNum(Num n, char *out, int cap) {
    if (n.m == 0.0) {
        snprintf(out, cap, "0");
        return;
    }
    if (n.e < 6) {
        double v = n.m * pow(10.0, n.e);
        if (v >= 1000.0) snprintf(out, cap, "%.0f", v);
        else if (v >= 100.0) snprintf(out, cap, "%.1f", v);
        else snprintf(out, cap, "%.2f", v);
    } else {
        static const char *names[] = {
            "million","billion","trillion","quadrillion","quintillion",
            "sextillion","septillion","octillion","nonillion","decillion",
            "undecillion","duodecillion","tredecillion","quattuordecillion"
        };
        int group = n.e / 3;
        if (group >= 2 && group <= 15) {
            double v = n.m * pow(10.0, n.e - group * 3);
            snprintf(out, cap, "%.2f%s", v, names[group - 2]);
        } else {
            snprintf(out, cap, "%.2fe%d", n.m, n.e);
        }
    }
}

/* 5x7 bitmap font. Characters are A-Z, 0-9 and basic punctuation. */
static const unsigned char font[][7] = {
    {14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{14,17,16,16,16,17,14},
    {30,17,17,17,17,17,30},{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},
    {14,17,16,23,17,17,15},{17,17,17,31,17,17,17},{14,4,4,4,4,4,14},
    {7,2,2,2,2,18,12},{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},
    {17,27,21,21,17,17,17},{17,25,21,19,17,17,17},{14,17,17,17,17,17,14},
    {30,17,17,30,16,16,16},{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},
    {15,16,16,14,1,1,30},{31,4,4,4,4,4,4},{17,17,17,17,17,17,14},
    {17,17,17,17,17,10,4},{17,17,17,21,21,21,10},{17,17,10,4,10,17,17},
    {17,17,10,4,4,4,4},{31,1,2,4,8,16,31},
    {14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},
    {30,1,1,14,1,1,30},{2,6,10,18,31,2,2},{31,16,16,30,1,1,30},
    {14,16,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},
    {14,17,17,15,1,1,14},{0,0,0,0,0,0,0},
    {0,0,0,4,0,4,0},{0,0,0,31,0,0,0},{0,0,4,0,0,4,0},
    {0,0,0,14,0,0,0},{0,0,0,4,0,0,0},{0,0,0,0,0,0,4},
    {0,0,0,4,0,0,0}
};

static int glyphIndex(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= '0' && c <= '9') return 26 + (c - '0');
    if (c == ' ') return 36;
    if (c == '.') return 37;
    if (c == '-') return 38;
    if (c == '+') return 39;
    if (c == ':') return 40;
    if (c == '/') return 41;
    if (c == '%') return 42;
    if (c == '!') return 43;
    return 36;
}

static SDL_Surface *screen;

static void putText(int x, int y, const char *s, int scale) {
    SDL_Rect r;
    for (int i = 0; s[i]; ++i) {
        int gi = glyphIndex((char)toupper((unsigned char)s[i]));
        for (int yy = 0; yy < 7; ++yy) {
            for (int xx = 0; xx < 5; ++xx) {
                if (font[gi][yy] & (1 << (4 - xx))) {
                    r.x = x + i * 6 * scale + xx * scale;
                    r.y = y + yy * scale;
                    r.w = scale;
                    r.h = scale;
                    SDL_FillRect(screen, &r, SDL_MapRGB(screen->format, 0, 0, 0));
                }
            }
        }
    }
}

struct Building {
    const char *name;
    double baseCost;
    double baseCps;
    int owned;
};

static Building buildings[BUILDINGS] = {
    {"Cursor",15,0.1,0},{"Grandma",100,1,0},{"Farm",1100,8,0},
    {"Mine",12000,47,0},{"Factory",130000,260,0},{"Bank",1400000,1400,0},
    {"Temple",20000000,7800,0},{"Wizard Tower",330000000,44000,0},
    {"Shipment",5100000000.0,260000,0},{"Alchemy Lab",75000000000.0,1600000,0},
    {"Portal",1000000000000.0,10000000,0},{"Time Machine",14000000000000.0,65000000,0},
    {"Antimatter Condenser",170000000000000.0,430000000,0},
    {"Prism",2100000000000000.0,2900000000,0},
    {"Chancemaker",7700000000000000.0,21000000000.0,0},
    {"Fractal Engine",51000000000000000.0,150000000000.0,0},
    {"Javascript Console",71000000000000000000.0,1100000000000.0,0},
    {"Idleverse",12000000000000000000000.0,8300000000000.0,0},
    {"Cortex Baker",1900000000000000000000000.0,64000000000000.0,0},
    {"You",540000000000000000000000000.0,510000000000000.0,0}
};

struct Upgrade {
    const char *name;
    double cost;
    int building;
    int unlock;
    bool bought;
};

static Upgrade upgrades[NORMAL_UPGRADES] = {
    {"Reinforced index finger",100,0,1,false},
    {"Carpal tunnel prevention cream",500,0,1,false},
    {"Ambidextrous",10000,0,10,false},
    {"Thousand fingers",100000,0,25,false},
    {"Million fingers",10000000,0,50,false},
    {"Billion fingers",100000000,0,100,false},

    {"Forwards from grandma",1000,1,1,false},
    {"Steel-plated rolling pins",5000,1,5,false},
    {"Lubricated dentures",50000,1,25,false},
    {"Prune juice",5000000,1,50,false},
    {"Double-thick glasses",500000000,1,100,false},
    {"Aging agents",50000000000.0,1,150,false},

    {"Cheap hoes",11000,2,1,false},
    {"Fertilizer",55000,2,5,false},
    {"Cookie trees",550000,2,25,false},
    {"Genetically-modified crops",5500000,2,50,false},
    {"Gingerbread scarecrows",550000000,2,100,false},
    {"Pesticide-free farms",55000000000.0,2,150,false},

    {"Sugar gas",120000,3,1,false},
    {"Megadrill",600000,3,5,false},
    {"Ultradrill",6000000,3,25,false},
    {"Ultimadrill",600000000,3,50,false},
    {"H-bomb mining",60000000000.0,3,100,false},
    {"Coreforge",6000000000000.0,3,150,false},

    {"Sturdier conveyor belts",1300000,4,1,false},
    {"Child labor",6500000,4,5,false},
    {"Sweatshop",65000000,4,25,false},
    {"Radium reactors",6500000000.0,4,50,false},
    {"Recombobulators",650000000000.0,4,100,false},
    {"Deep-bake process",65000000000000.0,4,150,false}
};

struct HeavenlyUpgrade {
    const char *name;
    double cost;
    int kind;
    bool bought;
};

static HeavenlyUpgrade hu[HEAVENLY_UPGRADES] = {
    {"Legacy",1,0,false},
    {"Heavenly cookies",3,1,false},
    {"How to bake your dragon",9,2,false},
    {"Tin of british tea biscuits",25,3,false},
    {"Box of macarons",25,3,false},
    {"Box of brand biscuits",25,3,false},
    {"Heavenly luck",77,4,false},
    {"Permanent upgrade slot I",100,5,false},
    {"Golden switch",999,7,false}
};

static bool achievements[ACHIEVEMENTS];
static const char *achievementNames[ACHIEVEMENTS] = {
    "Wake and bake","Making some dough","So baked right now","Fledgling bakery",
    "Affluent bakery","World-famous baker","Hardcore","Casual baking",
    "Clicktastic","Clickathlon","Uncanny clicker","Builder",
    "Architect","Enhancer","The cookie ages","Doughnut",
    "Lovely cookies","Golden touch","Lucky cookie","Just plain lucky"
};

static Num cookies;
static Num allTime;
static long clicks = 0;
static long goldenClicks = 0;
static long prestige = 0;
static Num heavenly;
static bool goldenSwitch = false;
static bool goldenActive = false;
static Uint32 goldenExpire = 0;
static Uint32 nextGoldenAt = 0;
static int goldenType = 0;
static double frenzyUntil = 0.0;
static double clickFrenzyUntil = 0.0;
static double buildingSpecialUntil = 0.0;
static int specialBuilding = 0;

static int tab = 0;
static int cursor = 0;
static Uint32 lastFrame = 0;
static time_t savedAt = 0;

static Num buildingCost(int i) {
    double logc = log10(buildings[i].baseCost) + buildings[i].owned * log10(1.15);
    return numFromLog10(logc);
}

static double buildingFactor(int b) {
    double factor = 1.0;
    for (int i=0;i<NORMAL_UPGRADES;i++)
        if (upgrades[i].bought && upgrades[i].building == b)
            factor *= 2.0;
    return factor;
}

static double baseCps() {
    double x = 0.0;
    for (int i=0;i<BUILDINGS;i++)
        x += buildings[i].owned * buildings[i].baseCps * buildingFactor(i);
    return x;
}

static bool hasHU(int k) {
    return hu[k].bought;
}

static double cps() {
    double x = baseCps();

    for (int i=0;i<NORMAL_UPGRADES;i++) {
        if (upgrades[i].bought && upgrades[i].building >= 0)
            x *= 1.0;
    }

    /* The first six upgrades of each building are 2x building efficiency. */
    for (int i=0;i<NORMAL_UPGRADES;i++) {
        if (upgrades[i].bought && upgrades[i].building >= 0)
            x += 0.0;
    }

    /* Apply exact 2x tiering per building without multiplying unrelated buildings. */
    for (int b=0;b<BUILDINGS;b++) {
        double factor = 1.0;
        for (int i=0;i<NORMAL_UPGRADES;i++)
            if (upgrades[i].bought && upgrades[i].building == b)
                factor *= 2.0;

        if (factor > 1.0) {
            /* remove original contribution and add upgraded contribution */
            x -= buildings[b].owned * buildings[b].baseCps;
            x += buildings[b].owned * buildings[b].baseCps * factor;
        }
    }

    if (prestige > 0) x *= 1.0 + prestige * 0.01;
    if (hasHU(1)) x *= 1.10;
    if (hasHU(8) && goldenSwitch) x *= 1.50;
    if (frenzyUntil > 0.0) x *= 7.0;
    if (buildingSpecialUntil > 0.0) x *= 1.0 + buildings[specialBuilding].owned * 0.10;

    return x;
}

static double clickPower() {
    double x = 1.0;

    for (int i=0;i<3;i++)
        if (upgrades[i].bought) x *= 2.0;

    int nonCursor = 0;
    for (int i=1;i<BUILDINGS;i++) nonCursor += buildings[i].owned;

    if (upgrades[3].bought) x += nonCursor * 0.1;
    if (upgrades[4].bought) x += nonCursor * 0.5;
    if (upgrades[5].bought) x += nonCursor * 5.0;

    if (clickFrenzyUntil > 0.0) x *= 777.0;
    return x;
}

static long prestigeFor(Num baked) {
    double l = numLog10(baked);
    if (l < 12.0) return 0;
    double p = pow(10.0, (l - 12.0) / 3.0);
    return (long)floor(p + 1e-10);
}

static bool upgradeAvailable(int i) {
    return !upgrades[i].bought && buildings[upgrades[i].building].owned >= upgrades[i].unlock;
}

static bool heavenlyAvailable(int i) {
    if (hu[i].bought) return false;
    if (i == 0) return prestige >= 1;
    if (i == 1 || i == 2 || i == 3 || i == 4 || i == 5 || i == 6 || i == 7 || i == 8)
        return hasHU(0);
    return false;
}

static void addCookies(Num n) {
    cookies = numAdd(cookies, n);
    allTime = numAdd(allTime, n);
}

static void clickBigCookie() {
    double gain = clickPower();
    addCookies(Num(gain));
    ++clicks;
}

static void spawnGolden(Uint32 now) {
    if (goldenSwitch || goldenActive) return;
    if (now < nextGoldenAt) return;

    goldenActive = true;
    goldenExpire = now + (hasHU(6) ? 26000 : 13000);
    nextGoldenAt = now + (hasHU(6) ? 20000 : 40000) + (rand() % 30000);
    goldenType = rand() % 4;
}

static void clickGolden() {
    if (!goldenActive) return;

    goldenActive = false;
    ++goldenClicks;

    if (goldenType == 0) {
        Num bankPart = numMul(cookies, 0.15);
        Num cpsPart = Num(cps() * 900.0 + 13.0);
        Num gain = numGE(bankPart, cpsPart) ? cpsPart : bankPart;
        if (gain.m == 0.0) gain = Num(13.0);
        addCookies(gain);
    } else if (goldenType == 1) {
        frenzyUntil = (double)time(NULL) + (hasHU(6) ? 84.7 : 77.0);
    } else if (goldenType == 2) {
        clickFrenzyUntil = (double)time(NULL) + (hasHU(6) ? 14.3 : 13.0);
    } else {
        specialBuilding = rand() % BUILDINGS;
        buildingSpecialUntil = (double)time(NULL) + (hasHU(6) ? 33.0 : 30.0);
    }
}

static void buyBuilding() {
    int i = cursor;
    Num cost = buildingCost(i);
    if (numGE(cookies, cost)) {
        cookies = numSub(cookies, cost);
        ++buildings[i].owned;
    }
}

static void buyUpgrade() {
    if (cursor < 0 || cursor >= NORMAL_UPGRADES) return;
    if (!upgradeAvailable(cursor)) return;

    Num cost(upgrades[cursor].cost);
    if (numGE(cookies, cost)) {
        cookies = numSub(cookies, cost);
        upgrades[cursor].bought = true;
    }
}

static void buyHeavenly() {
    if (cursor < 0 || cursor >= HEAVENLY_UPGRADES) return;
    if (!heavenlyAvailable(cursor)) return;

    Num cost(hu[cursor].cost);
    if (numGE(heavenly, cost)) {
        heavenly = numSub(heavenly, cost);
        hu[cursor].bought = true;
        if (cursor == 8) goldenSwitch = true;
    }
}

static void updateAchievements() {
    double baked = numLog10(allTime);
    if (baked >= 0) achievements[0] = true;
    if (baked >= 3) achievements[1] = true;
    if (baked >= 6) achievements[2] = true;
    if (baked >= 9) achievements[3] = true;
    if (baked >= 12) achievements[4] = true;
    if (baked >= 15) achievements[5] = true;
    if (numLog10(allTime) >= 9 && prestige == 0 && clicks == 0)
        achievements[6] = true;
    if (clicks >= 100) achievements[8] = true;
    if (clicks >= 1000) achievements[9] = true;
    if (clicks >= 10000) achievements[10] = true;

    int total = 0;
    for (int i=0;i<BUILDINGS;i++) total += buildings[i].owned;
    if (total >= 100) achievements[11] = true;
    if (total >= 500) achievements[12] = true;

    int bought = 0;
    for (int i=0;i<NORMAL_UPGRADES;i++) if (upgrades[i].bought) ++bought;
    if (bought >= 5) achievements[13] = true;
    if (bought >= 20) achievements[14] = true;
    if (goldenClicks >= 1) achievements[17] = true;
    if (goldenClicks >= 7) achievements[18] = true;
    if (goldenClicks >= 77) achievements[19] = true;
}

static void resetRun() {
    for (int i=0;i<BUILDINGS;i++) buildings[i].owned = 0;
    for (int i=0;i<NORMAL_UPGRADES;i++) upgrades[i].bought = false;
    cookies = Num();
    frenzyUntil = 0.0;
    clickFrenzyUntil = 0.0;
    buildingSpecialUntil = 0.0;
    goldenActive = false;
    goldenSwitch = hasHU(8);
}

static void ascend() {
    long p = prestigeFor(allTime);
    if (p <= prestige) return;

    heavenly = numAdd(heavenly, Num((double)(p - prestige)));
    prestige = p;
    resetRun();
}

static void saveGame() {
    FILE *f = fopen(SAVEFILE, "wb");
    if (!f) return;

    const unsigned int version = 2;
    fwrite(&version, sizeof(version), 1, f);

    fwrite(&cookies, sizeof(cookies), 1, f);
    fwrite(&allTime, sizeof(allTime), 1, f);
    fwrite(&heavenly, sizeof(heavenly), 1, f);
    fwrite(&prestige, sizeof(prestige), 1, f);
    fwrite(&clicks, sizeof(clicks), 1, f);
    fwrite(&goldenClicks, sizeof(goldenClicks), 1, f);
    fwrite(&goldenSwitch, sizeof(goldenSwitch), 1, f);
    fwrite(&savedAt, sizeof(savedAt), 1, f);

    for (int i=0;i<BUILDINGS;i++) fwrite(&buildings[i].owned, sizeof(int), 1, f);
    for (int i=0;i<NORMAL_UPGRADES;i++) fwrite(&upgrades[i].bought, sizeof(bool), 1, f);
    for (int i=0;i<HEAVENLY_UPGRADES;i++) fwrite(&hu[i].bought, sizeof(bool), 1, f);
    for (int i=0;i<ACHIEVEMENTS;i++) fwrite(&achievements[i], sizeof(bool), 1, f);

    fclose(f);
}

static void loadGame() {
    FILE *f = fopen(SAVEFILE, "rb");
    if (!f) return;

    unsigned int version = 0;
    if (fread(&version, sizeof(version), 1, f) != 1 || version != 2) {
        fclose(f);
        return;
    }

    fread(&cookies, sizeof(cookies), 1, f);
    fread(&allTime, sizeof(allTime), 1, f);
    fread(&heavenly, sizeof(heavenly), 1, f);
    fread(&prestige, sizeof(prestige), 1, f);
    fread(&clicks, sizeof(clicks), 1, f);
    fread(&goldenClicks, sizeof(goldenClicks), 1, f);
    fread(&goldenSwitch, sizeof(goldenSwitch), 1, f);
    fread(&savedAt, sizeof(savedAt), 1, f);

    for (int i=0;i<BUILDINGS;i++) fread(&buildings[i].owned, sizeof(int), 1, f);
    for (int i=0;i<NORMAL_UPGRADES;i++) fread(&upgrades[i].bought, sizeof(bool), 1, f);
    for (int i=0;i<HEAVENLY_UPGRADES;i++) fread(&hu[i].bought, sizeof(bool), 1, f);
    for (int i=0;i<ACHIEVEMENTS;i++) fread(&achievements[i], sizeof(bool), 1, f);

    fclose(f);

    time_t now = time(NULL);
    double offline = difftime(now, savedAt);
    if (offline > 0.0) {
        if (offline > 604800.0) offline = 604800.0;
        if (offline > 0.0) {
            Num gain = numMul(Num(cps() * offline), 0.85);
            addCookies(gain);
        }
    }
}

static void drawHeader() {
    char s[64];
    SDL_Rect r = {0,0,W,24};
    SDL_FillRect(screen, &r, SDL_MapRGB(screen->format, 235,235,235));

    formatNum(cookies, s, sizeof(s));
    putText(4,3,"COOKIES:",1);
    putText(58,3,s,1);

    snprintf(s,sizeof(s),"CPS %.1f",cps());
    putText(185,3,s,1);

    snprintf(s,sizeof(s),"P %ld HC",prestige);
    putText(245,13,s,1);
}

static void drawBigCookie() {
    SDL_Rect r = {18,48,125,125};
    Uint32 c = SDL_MapRGB(screen->format, 210, 160, 90);
    SDL_FillRect(screen,&r,c);

    if (goldenActive) {
        SDL_Rect g = {45,78,72,60};
        SDL_FillRect(screen,&g,SDL_MapRGB(screen->format,255,205,40));
        putText(53,101,"GOLD!",1);
    } else {
        putText(47,104,"COOKIE",1);
    }

    char s[48];
    snprintf(s,sizeof(s),"+%.2f",clickPower());
    putText(51,182,s,1);
}

static void drawList() {
    int x = 158;
    SDL_Rect bg = {x,24,W-x,H-24};
    SDL_FillRect(screen,&bg,SDL_MapRGB(screen->format,248,248,248));

    if (tab == 0) putText(x+4,28,"BUILDINGS",1);
    else if (tab == 1) putText(x+4,28,"UPGRADES",1);
    else if (tab == 2) putText(x+4,28,"HEAVENLY",1);

    int start = cursor;
    int count = tab == 0 ? BUILDINGS : (tab == 1 ? NORMAL_UPGRADES : HEAVENLY_UPGRADES);

    if (start > count - 1) start = 0;

    for (int row=0; row<7; ++row) {
        int i = (start + row) % count;
        int y = 48 + row*25;

        if (row == 0) {
            SDL_Rect sel = {x+1,y-2,W-x-2,21};
            SDL_FillRect(screen,&sel,SDL_MapRGB(screen->format,220,220,220));
        }

        char line[64];

        if (tab == 0) {
            char cost[32];
            formatNum(buildingCost(i),cost,sizeof(cost));
            snprintf(line,sizeof(line),"%s %d",buildings[i].name,buildings[i].owned);
            putText(x+4,y,line,1);
            putText(x+4,y+9,cost,1);
        } else if (tab == 1) {
            if (upgrades[i].bought) snprintf(line,sizeof(line),"BOUGHT %s",upgrades[i].name);
            else snprintf(line,sizeof(line),"%s",upgrades[i].name);
            putText(x+4,y,line,1);
        } else {
            if (hu[i].bought) snprintf(line,sizeof(line),"BOUGHT %s",hu[i].name);
            else snprintf(line,sizeof(line),"%s",hu[i].name);
            putText(x+4,y,line,1);
        }
    }
}

static void drawFooter() {
    putText(3,220,"SPACE CLICK  ENTER BUY  UP/DN",1);
    putText(3,229,"L/R TAB  A ASCEND  G SWITCH  S SAVE",1);
}

static void draw() {
    SDL_FillRect(screen,NULL,SDL_MapRGB(screen->format,255,255,255));
    drawHeader();
    drawBigCookie();
    drawList();
    drawFooter();
    SDL_Flip(screen);
}

int main() {
    srand((unsigned)time(NULL));

    if (SDL_Init(SDL_INIT_VIDEO) < 0) return 1;

    screen = SDL_SetVideoMode(W,H,16,SDL_SWSURFACE);
    if (!screen) {
        SDL_Quit();
        return 1;
    }

    loadGame();

    savedAt = time(NULL);
    lastFrame = SDL_GetTicks();
    nextGoldenAt = lastFrame + 30000;

    bool running = true;

    while (running) {
        Uint32 now = SDL_GetTicks();
        double dt = (now - lastFrame) / 1000.0;
        if (dt < 0.0 || dt > 10.0) dt = 0.0;
        lastFrame = now;

        if (dt > 0.0) {
            addCookies(Num(cps() * dt));

            double t = (double)time(NULL);
            if (frenzyUntil > 0.0 && t >= frenzyUntil) frenzyUntil = 0.0;
            if (clickFrenzyUntil > 0.0 && t >= clickFrenzyUntil) clickFrenzyUntil = 0.0;
            if (buildingSpecialUntil > 0.0 && t >= buildingSpecialUntil) buildingSpecialUntil = 0.0;
        }

        spawnGolden(now);
        if (goldenActive && now >= goldenExpire) goldenActive = false;

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;

            if (e.type == SDL_KEYDOWN) {
                SDLKey k = e.key.keysym.sym;

                if (k == SDLK_ESCAPE) {
                    savedAt = time(NULL);
                    saveGame();
                    running = false;
                } else if (k == SDLK_SPACE) {
                    if (goldenActive) clickGolden();
                    else clickBigCookie();
                } else if (k == SDLK_RETURN) {
                    if (tab == 0) buyBuilding();
                    else if (tab == 1) buyUpgrade();
                    else buyHeavenly();
                } else if (k == SDLK_UP) {
                    --cursor;
                    int count = tab == 0 ? BUILDINGS : (tab == 1 ? NORMAL_UPGRADES : HEAVENLY_UPGRADES);
                    if (cursor < 0) cursor = count - 1;
                } else if (k == SDLK_DOWN) {
                    ++cursor;
                    int count = tab == 0 ? BUILDINGS : (tab == 1 ? NORMAL_UPGRADES : HEAVENLY_UPGRADES);
                    if (cursor >= count) cursor = 0;
                } else if (k == SDLK_LEFT) {
                    tab = (tab + 2) % 3;
                    cursor = 0;
                } else if (k == SDLK_RIGHT) {
                    tab = (tab + 1) % 3;
                    cursor = 0;
                } else if (k == SDLK_a) {
                    ascend();
                } else if (k == SDLK_g) {
                    if (hasHU(9)) goldenSwitch = !goldenSwitch;
                } else if (k == SDLK_s) {
                    savedAt = time(NULL);
                    saveGame();
                }
            }
        }

        updateAchievements();
        draw();
        SDL_Delay(25);
    }

    savedAt = time(NULL);
    saveGame();
    SDL_Quit();
    return 0;
}

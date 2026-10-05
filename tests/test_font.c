/* Renders the UI status text to a BMP via the real font renderer. */
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_render.h>
#include <SDL_surface.h>
#include <SDL_filesystem.h>
#include "font5x7.h"
#include <stdio.h>
int main(int argc, char **argv) {
    (void) argc; (void) argv;
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO) != 0) { fprintf(stderr, "init: %s\n", SDL_GetError()); return 1; }
    SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, 800, 200, 32, SDL_PIXELFORMAT_RGBA32);
    if (!s) { fprintf(stderr, "surface: %s\n", SDL_GetError()); return 1; }
    SDL_FillRect(s, NULL, SDL_MapRGB(s->format, 8, 8, 12));

    /* font5x7_draw takes a renderer; for the smoke test, draw via a software
     * renderer on the surface */
    SDL_Renderer *r = SDL_CreateSoftwareRenderer(s);
    if (!r) { fprintf(stderr, "renderer: %s\n", SDL_GetError()); return 1; }

    SDL_Color white = {230, 230, 235, 255};
    SDL_Color gray = {140, 140, 150, 255};
    font5x7_draw(r, "Steam View - waiting for stream (UDP:5000)", 40, 40, white);
    font5x7_draw(r, "VIDEO 1280x720  FRAMES 100  I 2  P 98", 40, 60, gray);
    font5x7_draw(r, "UDP:5000  PKTS 124  MB 7.73", 40, 80, gray);
    font5x7_draw(r, "ABCDEFGHIJKLMNOPQRSTUVWXYZ abcdefghijklmnopqrstuvwxyz", 40, 100, gray);
    font5x7_draw(r, "0123456789 !\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~", 40, 120, gray);
    SDL_SaveBMP(s, "font_preview.bmp");
    printf("saved font_preview.bmp\n");
    SDL_DestroyRenderer(r);
    SDL_FreeSurface(s);
    SDL_Quit();
    return 0;
}

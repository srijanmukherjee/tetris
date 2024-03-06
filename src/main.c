#include <SDL2/SDL.h>
#include <SDL2/SDL_audio.h>
#include <SDL2/SDL_error.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_keycode.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL_mouse.h>
#include <SDL2/SDL_rect.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_stdinc.h>
#include <SDL2/SDL_surface.h>
#include <SDL2/SDL_timer.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_video.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define PIECE_WIDTH 30
#define PIECE_HEIGHT 30
#define PIECE_PADDING 1
#define BOARD_WIDTH 10
#define BOARD_HEIGHT 20

#define SCREEN_EXIT -1
#define SCREEN_HOME 0
#define SCREEN_PLAY 1
#define SCREEN_GAME_OVER 2

const char tetriminos[7][16] = {"..x...x...x...x.", ".xx..xx.........",
                                ".....xx.xx......", "....xx...xx.....",
                                ".....x...x...xx.", "......x...x..xx.",
                                "....xxx..x......"};

unsigned char *board = NULL;
int lines[BOARD_HEIGHT];
int line_ptr = 0;

typedef struct {
    const char *shape;
    int color;
    int x;
    int y;
    int rotation;
} Piece;

typedef struct {
    Piece piece;
    Piece next_piece;
    bool over;
    float y;
    int yspeed;
    uint32_t board_xoff;
    uint32_t board_yoff;
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *pieces_texture;
    SDL_Texture *play_btn_texture;
    Mix_Music *bg_music;
    TTF_Font *font;
    uint32_t score;
} Game;

// returns the index of (x, y) corresponding to the rotated shape
int rotate(int x, int y, int rotation) {
    switch (rotation) {
    // 0 deg
    case 0:
        return y * 4 + x;
    // 90 deg
    case 1:
        return 12 + y - 4 * x;
    // 180 deg
    case 2:
        return 15 - x - 4 * y;
    // 270 deg
    case 3:
        return 3 - y + 4 * x;
    }

    return 0;
}

// checks if the piece fits into the board
bool does_piece_fit(const char *shape, int x, int y, int rotation) {
    for (int px = 0; px < 4; px++) {
        for (int py = 0; py < 4; py++) {
            // get index into piece
            int pi = rotate(px, py, rotation);

            // get index into board
            int bi = (py + y) * BOARD_WIDTH + (px + x);

            if (shape[pi] == 'x') {
                if (px + x < 0 || px + x >= BOARD_WIDTH || py + y < 0 ||
                    py + y >= BOARD_HEIGHT) {
                    return false;
                }
            }

            if (px + x >= 0 && px + x < BOARD_WIDTH && py + y >= 0 &&
                py + y < BOARD_HEIGHT) {
                if (shape[pi] == 'x' && board[bi] != 0) {
                    return false;
                }
            }
        }
    }
    return true;
}

// get time elapsed since last frame
float get_delta() {
    static Uint32 last_time = 0;
    Uint32 curr_time = SDL_GetTicks();
    float dt = (curr_time - last_time) / 1000.0f;
    last_time = curr_time;
    return dt;
}

int home_screen(Game *game) {
    static int y = 0;
    static double counter = 0;

    int play_btn_width = 40;
    int play_btn_height = 40;
    int px = WINDOW_WIDTH / 2 - play_btn_width / 2;
    int py = WINDOW_HEIGHT / 2 + 100 - play_btn_height / 2;
    bool clicked = false;

    SDL_Event e;
    if (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            return SCREEN_EXIT;
        }

        // detect mouse click
        if (e.type == SDL_MOUSEBUTTONUP) {
            clicked = true;
        }
    }

    int mx;
    int my;
    SDL_GetMouseState(&mx, &my);

    if (mx >= px && mx <= px + play_btn_width && my >= py &&
        my <= py + play_btn_height) {
        if (clicked)
            return SCREEN_PLAY;
        play_btn_width *= 2;
        play_btn_height *= 2;
        px = WINDOW_WIDTH / 2 - play_btn_width / 2;
        py = WINDOW_HEIGHT / 2 + 100 - play_btn_height / 2;
    }

    SDL_SetRenderDrawColor(game->renderer, 51, 51, 51, 255);
    SDL_RenderClear(game->renderer);

    float dt = get_delta();
    counter = (counter + 5 * dt);
    if (counter > INT_MAX)
        counter = 0;
    y = 10 * sin(counter);
    TTF_SetFontSize(game->font, 80);
    SDL_Surface *surface = TTF_RenderText_Solid(
        game->font, "The Tetris", (SDL_Color){255, 255, 255, 255});
    SDL_Texture *texture =
        SDL_CreateTextureFromSurface(game->renderer, surface);

    SDL_RenderCopy(game->renderer, texture, NULL,
                   &(SDL_Rect){WINDOW_WIDTH / 2 - 150,
                               WINDOW_HEIGHT / 2 - 75 - 100 + y, 300, 150});

    SDL_RenderCopy(game->renderer, game->play_btn_texture, NULL,
                   &(SDL_Rect){px, py, play_btn_width, play_btn_height});

    SDL_RenderPresent(game->renderer);

    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
    return SCREEN_HOME;
}

int play_screen(Game *game) {
    float dt = get_delta();
    SDL_Event e;
    if (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            return SCREEN_EXIT;
        }

        if (e.type == SDL_KEYDOWN) {
            switch (e.key.keysym.sym) {
            case SDLK_w:
            case SDLK_UP: {
                int nextRot = (game->piece.rotation + 1) % 4;
                if (does_piece_fit(game->piece.shape, game->piece.x,
                                   game->piece.y, nextRot)) {
                    game->piece.rotation = nextRot;
                }
                break;
            }
            case SDLK_d:
            case SDLK_RIGHT:
                if (does_piece_fit(game->piece.shape, game->piece.x + 1,
                                   game->piece.y, game->piece.rotation))
                    game->piece.x += 1;
                break;
            case SDLK_a:
            case SDLK_LEFT:
                if (does_piece_fit(game->piece.shape, game->piece.x - 1,
                                   game->piece.y, game->piece.rotation))
                    game->piece.x -= 1;
                break;
            case SDLK_s:
            case SDLK_DOWN:
                game->y += 1;
                break;
            }
        }
    }

    if (game->over)
        return SCREEN_GAME_OVER;

    SDL_SetRenderDrawColor(game->renderer, 51, 51, 51, 255);
    SDL_RenderClear(game->renderer);

    // update
    if (does_piece_fit(game->piece.shape, game->piece.x, game->piece.y + 1,
                       game->piece.rotation)) {
        game->y += dt * game->yspeed;
        game->piece.y = game->y;
    } else {
        // piece can't fit, so copy it onto the board
        for (int px = 0; px < 4; px++) {
            for (int py = 0; py < 4; py++) {
                if (game->piece.shape[rotate(px, py, game->piece.rotation)] ==
                    'x') {
                    board[(game->piece.y + py) * BOARD_WIDTH +
                          (game->piece.x + px)] = game->piece.color + 1;
                }
            }
        }

        // check if line is formed
        for (int py = 0; py < 4; py++) {
            if (game->piece.y + py >= BOARD_HEIGHT)
                break;
            bool line = true;

            for (int px = 0; px < BOARD_WIDTH; px++) {
                line &= board[(game->piece.y + py) * BOARD_WIDTH + px] != 0;
            }

            if (line) {
                for (int px = 0; px < BOARD_WIDTH; px++) {
                    board[(game->piece.y + py) * BOARD_WIDTH + px] = 127;
                }

                lines[line_ptr++] = game->piece.y + py;
            }
        }

        int shape = rand() % 7;
        game->piece = game->next_piece;
        game->next_piece.shape = tetriminos[shape];
        game->y = 0;
        game->next_piece.y = 0;
        game->next_piece.color = shape;
        game->next_piece.x = BOARD_WIDTH / 2;
        game->next_piece.rotation = 0;

        game->over = !does_piece_fit(game->piece.shape, game->piece.x,
                                     game->piece.y, game->piece.rotation);

        if (game->over) {
            return SCREEN_PLAY;
        }
    }

    // draw board
    for (int x = 0; x < BOARD_WIDTH; x++) {
        for (int y = 0; y < BOARD_HEIGHT; y++) {
            int val = board[y * BOARD_WIDTH + x];
            if (val == 0) {
                SDL_SetRenderDrawColor(game->renderer, 28, 28, 28, 255);
                SDL_RenderFillRect(
                    game->renderer,
                    &(SDL_Rect){game->board_xoff + x * PIECE_WIDTH,
                                game->board_yoff + y * PIECE_HEIGHT,
                                PIECE_WIDTH, PIECE_HEIGHT});
            } else if (val == 127) {
                SDL_SetRenderDrawColor(game->renderer, 255, 255, 255, 255);
                SDL_RenderFillRect(
                    game->renderer,
                    &(SDL_Rect){game->board_xoff + x * PIECE_WIDTH,
                                game->board_yoff + y * PIECE_HEIGHT,
                                PIECE_WIDTH, PIECE_HEIGHT});
            } else {
                SDL_RenderCopy(
                    game->renderer, game->pieces_texture,
                    &(SDL_Rect){(val - 1) * 30, 0, 30, 30},
                    &(SDL_Rect){
                        game->board_xoff + x * PIECE_WIDTH + PIECE_PADDING,
                        game->board_yoff + y * PIECE_HEIGHT + PIECE_PADDING,
                        PIECE_WIDTH - 2 * PIECE_PADDING,
                        PIECE_HEIGHT - 2 * PIECE_PADDING});
            }
        }
    }

    // draw current piece
    for (int px = 0; px < 4; px++) {
        for (int py = 0; py < 4; py++) {
            int idx = rotate(px, py, game->piece.rotation);
            if (game->piece.shape[idx] == 'x') {
                SDL_RenderCopy(
                    game->renderer, game->pieces_texture,
                    &(SDL_Rect){game->piece.color * 30, 0, 30, 30},
                    &(SDL_Rect){
                        game->board_xoff + (game->piece.x + px) * PIECE_WIDTH +
                            PIECE_PADDING,
                        game->board_yoff + (game->piece.y + py) * PIECE_HEIGHT,
                        PIECE_WIDTH - 2 * PIECE_PADDING,
                        PIECE_HEIGHT - 2 * PIECE_PADDING});
            }
        }
    }

    // animate line completion
    if (line_ptr > 0) {
        SDL_RenderPresent(game->renderer);
        SDL_Delay(100);
        for (int i = 0; i < line_ptr; i++) {
            for (int px = 0; px < BOARD_WIDTH; px++) {
                for (int py = lines[i]; py > 0; py--)
                    board[py * BOARD_WIDTH + px] =
                        board[(py - 1) * BOARD_WIDTH + px];
                board[px] = 0;
            }
            game->score += 100;
        }
        line_ptr = 0;
        get_delta();
    }

    // draw score and next piece

    int xoff = game->board_xoff + PIECE_WIDTH * BOARD_WIDTH;
    int section_width = WINDOW_WIDTH - xoff;
    int next_piece_padding = 10;

    // make sure we have enough space to draw the next piece
    if (section_width > 4 * PIECE_WIDTH + 2 * next_piece_padding) {

        int w = 4 * PIECE_WIDTH + 2 * next_piece_padding;
        int x = xoff + (section_width - w) / 2;
        int y = 50;

        // draw next piece
        SDL_SetRenderDrawColor(game->renderer, 255, 255, 255, 255);
        SDL_RenderFillRect(game->renderer,
                           &(SDL_Rect){x, y, w + next_piece_padding, 2});

        SDL_RenderFillRect(game->renderer,
                           &(SDL_Rect){x, y, 2, w + next_piece_padding});

        SDL_RenderFillRect(game->renderer,
                           &(SDL_Rect){x + next_piece_padding + w, y, 2,
                                       w + next_piece_padding});

        SDL_RenderFillRect(game->renderer,
                           &(SDL_Rect){x, y + next_piece_padding + w,
                                       w + next_piece_padding, 2});
        for (int px = 0; px < 4; px++) {
            for (int py = 0; py < 4; py++) {
                int idx = rotate(px, py, game->next_piece.rotation);
                if (game->next_piece.shape[idx] == 'x') {
                    SDL_RenderCopy(
                        game->renderer, game->pieces_texture,
                        &(SDL_Rect){game->next_piece.color * 30, 0, 30, 30},
                        &(SDL_Rect){x + next_piece_padding + px * PIECE_WIDTH +
                                        PIECE_PADDING,
                                    y + next_piece_padding + py * PIECE_HEIGHT,
                                    PIECE_WIDTH - 2 * PIECE_PADDING,
                                    PIECE_HEIGHT - 2 * PIECE_PADDING});
                }
            }
        }

        // draw score
    }

    SDL_RenderPresent(game->renderer);
    return SCREEN_PLAY;
}

int game_over_screen(Game *game) {
    static int y = 0;
    static double counter = 0;
    static char score[100];

    int back_h = 80;
    int back_w = 150;
    int back_x = WINDOW_WIDTH / 2 - back_w / 2;
    int back_y = WINDOW_HEIGHT / 2 + 150 - back_h / 2;
    bool clicked = false;

    SDL_Event event;
    if (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            return SCREEN_EXIT;
        }

        if (event.type == SDL_MOUSEBUTTONUP) {
            clicked = true;
        }
    }

    int mx, my;
    SDL_GetMouseState(&mx, &my);

    if (mx >= back_x && mx <= back_x + back_w && my >= back_y &&
        my <= back_y + back_w) {
        back_w = 200;
        back_x = WINDOW_WIDTH / 2 - back_w / 2;
        if (clicked)
            return SCREEN_HOME;
    }

    float dt = get_delta();
    counter = (counter + 5 * dt);
    if (counter > INT_MAX)
        counter = 0;
    y = 10 * sin(counter);

    SDL_SetRenderDrawColor(game->renderer, 51, 51, 51, 255);
    SDL_RenderClear(game->renderer);

    TTF_SetFontSize(game->font, 80);
    SDL_Surface *surface = TTF_RenderText_Solid(
        game->font, "Game Over", (SDL_Color){255, 255, 255, 255});
    SDL_Texture *texture =
        SDL_CreateTextureFromSurface(game->renderer, surface);

    SDL_RenderCopy(game->renderer, texture, NULL,
                   &(SDL_Rect){WINDOW_WIDTH / 2 - 150,
                               WINDOW_HEIGHT / 2 - 75 - 100 + y, 300, 150});

    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);

    sprintf(score, "Score: %d", game->score);

    TTF_SetFontSize(game->font, 50);
    surface = TTF_RenderText_Solid(game->font, score,
                                   (SDL_Color){255, 255, 255, 255});
    texture = SDL_CreateTextureFromSurface(game->renderer, surface);

    SDL_RenderCopy(game->renderer, texture, NULL,
                   &(SDL_Rect){WINDOW_WIDTH / 2 - 75,
                               WINDOW_HEIGHT / 2 + 50 - 40, 150, 80});

    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);

    TTF_SetFontSize(game->font, 50);
    surface = TTF_RenderText_Solid(game->font, "Go Back",
                                   (SDL_Color){255, 255, 255, 255});
    texture = SDL_CreateTextureFromSurface(game->renderer, surface);

    SDL_RenderCopy(game->renderer, texture, NULL,
                   &(SDL_Rect){back_x, back_y, back_w, back_h});

    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);

    SDL_RenderPresent(game->renderer);

    return SCREEN_GAME_OVER;
}

int main() {
    srand(time(NULL));
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_AUDIO) < 0) {
        fprintf(stderr, "ERROR: failed to initialize SDL: %s\n",
                SDL_GetError());
        exit(1);
    }

    if (IMG_Init(IMG_INIT_PNG) == 0) {
        fprintf(stderr, "ERROR: failed to initialize SDL Image: %s\n",
                SDL_GetError());
        exit(1);
    }

    if (TTF_Init() < 0) {
        fprintf(stderr, "ERROR: failed to initialize ttf library: %s\n",
                SDL_GetError());
        exit(1);
    }

    SDL_Window *window = SDL_CreateWindow("Tetris", SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH,
                                          WINDOW_HEIGHT, SDL_WINDOW_OPENGL);
    if (window == NULL) {
        fprintf(stderr, "ERROR: failed to create window: %s\n", SDL_GetError());
        exit(1);
    }

    SDL_Renderer *renderer =
        SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == NULL) {
        fprintf(stderr, "ERROR: failed to create renderer: %s\n",
                SDL_GetError());
        exit(1);
    }

    board = calloc(BOARD_HEIGHT * BOARD_WIDTH, sizeof(unsigned char));
    if (board == NULL) {
        fprintf(stderr, "ERROR: failed to initialize board: %s\n",
                strerror(errno));
        exit(1);
    }

    if (Mix_OpenAudio(44100, AUDIO_S16SYS, 2, 512) < 0) {
        fprintf(stderr, "ERROR: failed to open audio stream: %s\n",
                SDL_GetError());
        exit(1);
    }

    if (Mix_AllocateChannels(4) < 0) {
        fprintf(stderr, "ERROR: failed to allocate channels: %s\n",
                SDL_GetError());
        exit(1);
    }

    Mix_VolumeMusic(10);

    SDL_Surface *color_pieces_surface = IMG_Load("./assets/img/pallete.png");
    SDL_Texture *color_pieces_texture =
        SDL_CreateTextureFromSurface(renderer, color_pieces_surface);

    SDL_Surface *play_btn_surface = IMG_Load("./assets/img/play_btn.png");
    SDL_Texture *play_btn_texture =
        SDL_CreateTextureFromSurface(renderer, play_btn_surface);
    SDL_FreeSurface(play_btn_surface);

    int p1 = rand() % 7;
    int p2 = rand() % 7;

    Game game = {.piece = {.shape = tetriminos[1],
                           .color = p1,
                           .x = BOARD_WIDTH / 2,
                           .y = 0},
                 .next_piece = {.shape = tetriminos[2],
                                .color = p2,
                                .x = BOARD_WIDTH / 2,
                                .y = 0},
                 .board_xoff = (WINDOW_WIDTH - BOARD_WIDTH * PIECE_WIDTH) / 2,
                 .board_yoff =
                     (WINDOW_HEIGHT - BOARD_HEIGHT * PIECE_HEIGHT) / 2,
                 .over = false,
                 .pieces_texture = color_pieces_texture,
                 .play_btn_texture = play_btn_texture,
                 .renderer = renderer,
                 .window = window,
                 .y = 0,
                 .yspeed = 3,
                 .score = 0};

    game.bg_music = Mix_LoadMUS("./assets/music/bg.mp3");

    Mix_FadeInMusic(game.bg_music, -1, 100);

    // load font
    game.font = TTF_OpenFont("./assets/font/SuperFunky.ttf", 80);
    if (game.font == NULL) {
        fprintf(stderr, "ERROR: failed to load font: %s\n", SDL_GetError());
        exit(1);
    }

    int screen = SCREEN_HOME;
    int prev_screen = SCREEN_HOME;

    while (true) {
        switch (screen) {
        case SCREEN_HOME:
            screen = home_screen(&game);
            break;
        case SCREEN_PLAY:
            screen = play_screen(&game);
            break;
        case SCREEN_GAME_OVER:
            screen = game_over_screen(&game);
            break;
        }

        if (screen == SCREEN_EXIT) {
            break;
        }

        if (prev_screen == SCREEN_HOME && screen == SCREEN_PLAY) {
            Mix_VolumeMusic(50);
        }

        if (prev_screen == SCREEN_PLAY && screen == SCREEN_GAME_OVER) {
            Mix_VolumeMusic(10);
        }

        if (prev_screen == SCREEN_GAME_OVER && screen == SCREEN_HOME) {
            // reset game state
            game.over = false;
            game.piece = (Piece){.shape = tetriminos[1],
                                 .color = p1,
                                 .x = BOARD_WIDTH / 2,
                                 .y = 0};
            game.next_piece = (Piece){.shape = tetriminos[2],
                                      .color = p2,
                                      .x = BOARD_WIDTH / 2,
                                      .y = 0};
            game.y = 0;
            game.yspeed = 3;
            game.score = 0;
            memset(board, 0,
                   BOARD_WIDTH * BOARD_HEIGHT * sizeof(unsigned char));
        }

        prev_screen = screen;
    }
    TTF_CloseFont(game.font);
    SDL_DestroyTexture(color_pieces_texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
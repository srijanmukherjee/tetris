#include <SDL2/SDL.h>
#include <SDL2/SDL_audio.h>
#include <SDL2/SDL_error.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_keycode.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL_rect.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_stdinc.h>
#include <SDL2/SDL_surface.h>
#include <SDL2/SDL_timer.h>
#include <SDL2/SDL_video.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define PIECE_WIDTH 30
#define PIECE_HEIGHT 30
#define PIECE_PADDING 1
#define BOARD_WIDTH 10
#define BOARD_HEIGHT 20

const char tetriminos[7][16] = {"..x...x...x...x.", ".....xx..xx.....",
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

int main() {
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

    Mix_VolumeMusic(50);

    Mix_Music *bg_music = Mix_LoadMUS("./assets/music/bg.mp3");

    Mix_FadeInMusic(bg_music, -1, 100);

    SDL_Surface *color_pieces_surface = IMG_Load("./assets/img/pallete.png");
    SDL_Texture *color_pieces_texture =
        SDL_CreateTextureFromSurface(renderer, color_pieces_surface);

    Piece piece = {.shape = tetriminos[4], .color = 4, .x = 0, .y = 0};
    float y = 0;
    int yspeed = 5;
    int board_xoff = (WINDOW_WIDTH - BOARD_WIDTH * PIECE_WIDTH) / 2;
    int board_yoff = (WINDOW_HEIGHT - BOARD_HEIGHT * PIECE_HEIGHT) / 2;

    bool game_over = false;

    while (true) {
        float dt = get_delta();
        SDL_Event e;
        if (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                break;
            }

            if (e.type == SDL_MOUSEBUTTONUP) {
                piece.shape = tetriminos[rand() % 7];
                piece.color = rand() % 7;
            }

            if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
                case SDLK_w:
                case SDLK_UP: {
                    int nextRot = (piece.rotation + 1) % 4;
                    if (does_piece_fit(piece.shape, piece.x, piece.y,
                                       nextRot)) {
                        piece.rotation = nextRot;
                    }
                    break;
                }
                case SDLK_d:
                case SDLK_RIGHT:
                    if (does_piece_fit(piece.shape, piece.x + 1, piece.y,
                                       piece.rotation))
                        piece.x += 1;
                    break;
                case SDLK_a:
                case SDLK_LEFT:
                    if (does_piece_fit(piece.shape, piece.x - 1, piece.y,
                                       piece.rotation))
                        piece.x -= 1;
                    break;
                case SDLK_s:
                case SDLK_DOWN:
                    y += 1;
                    break;
                }
            }
        }

        if (game_over)
            continue;

        SDL_SetRenderDrawColor(renderer, 51, 51, 51, 255);
        SDL_RenderClear(renderer);

        // update
        if (does_piece_fit(piece.shape, piece.x, piece.y + 1, piece.rotation)) {
            y += dt * yspeed;
            piece.y = y;
        } else {
            // piece can't fit, so copy it onto the board
            for (int px = 0; px < 4; px++) {
                for (int py = 0; py < 4; py++) {
                    if (piece.shape[rotate(px, py, piece.rotation)] == 'x') {
                        board[(piece.y + py) * BOARD_WIDTH + (piece.x + px)] =
                            piece.color + 1;
                    }
                }
            }

            // check if line is formed
            for (int py = 0; py < 4; py++) {
                if (piece.y + py >= BOARD_HEIGHT)
                    break;
                bool line = true;

                for (int px = 0; px < BOARD_WIDTH; px++) {
                    line &= board[(piece.y + py) * BOARD_WIDTH + px] != 0;
                }

                if (line) {
                    for (int px = 0; px < BOARD_WIDTH; px++) {
                        board[(piece.y + py) * BOARD_WIDTH + px] = 127;
                    }

                    lines[line_ptr++] = piece.y + py;
                }
            }

            int shape = rand() % 7;
            piece.shape = tetriminos[shape];
            y = 0;
            piece.y = 0;
            piece.color = shape;
            piece.x = BOARD_WIDTH / 2;
            piece.rotation = 0;

            game_over =
                !does_piece_fit(piece.shape, piece.x, piece.y, piece.rotation);

            if (game_over) {
                // TODO : draw only overlapping portion
                break;
            }
        }

        // draw board
        for (int x = 0; x < BOARD_WIDTH; x++) {
            for (int y = 0; y < BOARD_HEIGHT; y++) {
                int val = board[y * BOARD_WIDTH + x];
                if (val == 0) {
                    SDL_SetRenderDrawColor(renderer, 28, 28, 28, 255);
                    SDL_RenderFillRect(
                        renderer, &(SDL_Rect){board_xoff + x * PIECE_WIDTH,
                                              board_yoff + y * PIECE_HEIGHT,
                                              PIECE_WIDTH, PIECE_HEIGHT});
                } else if (val == 127) {
                    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
                    SDL_RenderFillRect(
                        renderer, &(SDL_Rect){board_xoff + x * PIECE_WIDTH,
                                              board_yoff + y * PIECE_HEIGHT,
                                              PIECE_WIDTH, PIECE_HEIGHT});
                } else {
                    SDL_RenderCopy(
                        renderer, color_pieces_texture,
                        &(SDL_Rect){(val - 1) * 30, 0, 30, 30},
                        &(SDL_Rect){
                            board_xoff + x * PIECE_WIDTH + PIECE_PADDING,
                            board_yoff + y * PIECE_HEIGHT + PIECE_PADDING,
                            PIECE_WIDTH - 2 * PIECE_PADDING,
                            PIECE_HEIGHT - 2 * PIECE_PADDING});
                }
            }
        }

        // draw current piece
        for (int px = 0; px < 4; px++) {
            for (int py = 0; py < 4; py++) {
                int idx = rotate(px, py, piece.rotation);
                if (piece.shape[idx] == 'x') {
                    SDL_RenderCopy(
                        renderer, color_pieces_texture,
                        &(SDL_Rect){piece.color * 30, 0, 30, 30},
                        &(SDL_Rect){board_xoff + (piece.x + px) * PIECE_WIDTH +
                                        PIECE_PADDING,
                                    board_yoff + (piece.y + py) * PIECE_HEIGHT,
                                    PIECE_WIDTH - 2 * PIECE_PADDING,
                                    PIECE_HEIGHT - 2 * PIECE_PADDING});
                }
            }
        }

        // animate line completion
        if (line_ptr > 0) {
            SDL_RenderPresent(renderer);
            SDL_Delay(100);
            for (int i = 0; i < line_ptr; i++) {
                for (int px = 0; px < BOARD_WIDTH; px++) {
                    for (int py = lines[i]; py > 0; py--)
                        board[py * BOARD_WIDTH + px] =
                            board[(py - 1) * BOARD_WIDTH + px];
                    board[px] = 0;
                }
            }
            line_ptr = 0;
            get_delta();
        }

        SDL_RenderPresent(renderer);
    }

    SDL_DestroyTexture(color_pieces_texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
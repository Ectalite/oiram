#include "defines.h"
#include "powerups.h"
#include "loadscreen.h"
#include "enemies.h"
#include "events.h"
#include "oiram.h"
#include "lower.h"
#include "images.h"
#include "simple_mover.h"
#include "tile_handlers.h"

#include <stdbool.h>
#include <stdint.h>

#include <tice.h>
#include <graphx.h>
#include <keypadc.h>
#include <fileioc.h>

map_t level_map;
tiles_struct_t tiles;
gfx_tilemap_t tilemap;
oiram_t oiram;
game_t game;

bool easter_egg1;
bool easter_egg2;
bool easter_egg3;
bool easter_egg4;

void missing_appvars(void) {
    gfx_End();
    os_SetCursorPos(0, 0);
    os_PutStrFull("Err: Missing Oiram AppVars");
    while(!os_GetCSC());
    exit(0);
}

// this handles the timer
void handler_timer(void) {
    game.seconds--;
   
    // if no more time, fail -- only need to trigger keypad interrupt now
    if (!game.seconds) {
        oiram.failed = true;
    }
    
    if (game.blue_item_count) {
        game.blue_item_count--;
        if (!game.blue_item_count) {
            show_blue_items(false);
        }
    }

    draw_time();
}

// called when user presses or releases a key
void handler_keypad_alternate(void) {
    bool press_up, pressed_s;
    kb_key_t g1_key, g2_key, g7_key;
    static bool pressed_special = false;
    
    kb_Scan();
    
    // read keypad data
    g1_key = kb_Data[1];
    g2_key = kb_Data[2];
    g7_key = kb_Data[7];
    
    pressed_s       = (g2_key & kb_Alpha);
    pressed_2nd     = (g1_key & kb_2nd);
    
    pressed_down    = (g7_key & kb_Down);
    pressed_left    = (g7_key & kb_Left);
    pressed_right   = (g7_key & kb_Right);
    
    press_up        = (g7_key & kb_Up);
    
    if (allow_up_press) {
        pressed_up = press_up;
    } else if (!press_up) {
        allow_up_press = true;
        if (oiram.vy < -5) {
            oiram.vy = -5;
        }
    }
    
    if (pressed_s && !pressed_special) {
        pressed_alpha = true;
    } else {
        pressed_alpha = false;
    }
    pressed_special = pressed_s;
    
    if (g1_key & kb_Del) {
        if (!oiram.failed) {
            game.exit = true;
            game.fastexit = true;
        }
    }
}

// called when user presses or releases a key
void handler_keypad(void) {
    bool press_up, pressed_s;
    kb_key_t g1_key, g2_key, g7_key;
    static bool pressed_special = false;
    
    kb_Scan();
    
    // read keypad data
    g1_key = kb_Data[1];
    g2_key = kb_Data[2];
    g7_key = kb_Data[7];
    
    pressed_2nd     = (g2_key & kb_Alpha);
    
    pressed_down    = (g7_key & kb_Down);
    pressed_left    = (g7_key & kb_Left);
    pressed_right   = (g7_key & kb_Right);
    
    pressed_s       = (g7_key & kb_Up);
    
    press_up        = (g1_key & kb_2nd);
    
    if (allow_up_press) {
        pressed_up = press_up;
    } else if (!press_up) {
        allow_up_press = true;
        if (oiram.vy < -5) {
            oiram.vy = -5;
        }
    }
    
    if (pressed_s && !pressed_special) {
        pressed_alpha = true;
    } else {
        pressed_alpha = false;
    }
    pressed_special = pressed_s;
    
    if (g1_key & kb_Del) {
        if (!oiram.failed) {
            game.exit = true;
            game.fastexit = true;
        }
    }
}

static void (*handle_keypad)(void);

void black_circles(void) {
    uint8_t radius;
    for(radius = 5; radius < 255; radius += 5) {
        gfx_FillCircle(160, 60, radius);
        gfx_BlitBuffer();
    }
}

void double_rectangle(uint24_t x, uint8_t y, uint24_t width, uint8_t height) {
    gfx_SetColor(WHITE_INDEX);
    gfx_Rectangle_NoClip(x, y, width, height);
    gfx_Rectangle_NoClip(x + 1, y + 1, width - 2, height - 2);
    gfx_SetColor(DARK_BLUE_INDEX);
    gfx_FillRectangle_NoClip(x + 2, y + 2, width - 4, height - 4);
    gfx_SetColor(BLACK_INDEX);
    gfx_SetPixel(x,y);
    gfx_SetPixel(x + width - 1, y);
    gfx_SetPixel(x + width - 1, y + height - 1);
    gfx_SetPixel(x, y + height - 1);
    gfx_SetColor(WHITE_INDEX);
    gfx_SetPixel(x + 2, y + 2);
    gfx_SetPixel(x + width - 3, y + 2);
    gfx_SetPixel(x + width - 3, y + height - 3);
    gfx_SetPixel(x + 2, y + height - 3);
}

static void print_centered(const char *string, const uint8_t ypos) {
    gfx_PrintStringXY(string, (LCD_WIDTH - gfx_GetStringWidth(string)) / 2, ypos);
}

static void extract_images(void) {
    uint8_t j;
    
    // extract palette and tiles -- this is also where palette information is stored
    extract_tiles();
    
    // extract sprites from sprite file
    extract_sprites();
    
    gfx_palette[BLACK_INDEX] = gfx_RGBTo1555(0, 0, 0);
    gfx_palette[WHITE_INDEX] = gfx_RGBTo1555(255, 255, 255);
    gfx_palette[DARK_BLUE_INDEX] = gfx_RGBTo1555(24, 120, 184);
    if (easter_egg3) {
        for (j=0; ++j;) {
            gfx_palette[j] = ~gfx_palette[j];
        }
    }
}

void main(void) {
    real_t *real_in;
    unsigned int delay;
    size_t pack_author_len;
    char end_str[100];
    pack_info_t *pack;
    bool cntr;
    
    // initialize the 8bpp graphics
    gfx_Begin( gfx_8bpp );
    gfx_SetDrawBuffer();
    
    // init the state of the levels
    tilemap.map = NULL;
    load_progress();
    
    // if we have no packs, error
    if (!num_packs) {
        missing_appvars();
    }
    
    // easter egg setup
    if(!ti_RclVar(TI_REAL_TYPE, ti_Ans, &real_in)) {
        int in = os_RealToInt24(real_in);
        if (in == 1337) { easter_egg1 = true; }
        if (in == 1202) { easter_egg2 = true; }
        if (in == 505)  { easter_egg3 = true; }
        if (in == 101)  { easter_egg4 = true; }
    }
    
    // extract palette and tiles/sprites just to make sure they exist
    extract_images();
    
    main_start:
    
    // load the splash screen
    set_load_screen();
    
    draw_level:
    
    // extract palette and tiles/sprites
    extract_images();
    
    // draw the splash starting items
    gfx_FillScreen(BLACK_INDEX);
    
    gfx_TransparentSprite(oiram_logo, 116, 55);
    gfx_TransparentSprite(oiram_start, 165, 55);
    
    gfx_SetTextFGColor(WHITE_INDEX);
    gfx_SetTextBGColor(DARK_BLUE_INDEX);
    gfx_SetTextTransparentColor(BLACK_INDEX);
    
    // init the structs
    game.exit = false;
    game.fastexit = false;
    game.end_count = 10;
    game.enter_end = false;
    game.seconds = 600;
    
    memset(&oiram, 0, sizeof oiram);
    oiram.fly_count = 9;
    oiram.sprite = oiram_right[0];
    oiram.direction = FACE_RIGHT;
    
    pack = &pack_info[game.pack];
    
    oiram.flags = pack->flags;
    oiram.lives = pack->lives;
    
    game.score = pack->score;
    game.coins = pack->coins;
    
    if (easter_egg1) {
        oiram_0_small = easter_egg_0;
        oiram_1_small = easter_egg_1;
    }
    
    // copy the oiram sprites to the temporary bufferes
    set_normal_oiram_sprites();
    
    // load all the enemies in the level
    set_level(game.pack, game.level);
    get_enemies();
    oiram_start_location();
    
    gfx_SetColor(WHITE_INDEX);
    double_rectangle(4, 133, 312, 48);
    double_rectangle(4, 194, 80, 33);
    gfx_TransparentSprite_NoClip(coin_sprite, 10, 140);
    gfx_TransparentSprite_NoClip(oiram_lives, 10, 164);
    gfx_TransparentSprite_NoClip(clock, 270, 143);
    
    gfx_Rectangle_NoClip(118, 145, 81, 4);
    
    draw_coins();
    draw_time();
    draw_score();
    draw_lives();
    draw_level();
    
    gfx_BlitBuffer();
    
    // disable the timer so it doesn't run when we don't want it to be running
    timer_Control = TIMER1_DISABLE;
    
    // by using the 32768 kHz clock, we can count for exactly a one second
    timer_1_ReloadValue = timer_1_Counter = 32768;
    
    // enable the timer, set it to the 32768 kHz clock, enable an interrupt once it reaches 0, and make it count down
    timer_Control = TIMER1_ENABLE | TIMER1_32K | TIMER1_0INT | TIMER1_DOWN;
    
    // show the start screen for a bit
    for(delay=0; delay<2; delay++) {
        if (timer_IntStatus & TIMER1_RELOADED) {
            delay++;
            timer_IntAcknowledge = TIMER1_RELOADED;
        }
    }
    
    // setup keypad handlers
    if (game.alternate_keypad) {
        handle_keypad = handler_keypad_alternate;
    } else {
        handle_keypad = handler_keypad;
    }
    
    // reset animations
    tiles.animation_3_count = 0;
    tiles.animation_4_count = 0;
    warp.style = WARP_NONE;
    warp.enter = false;
    
    // ensure sprites are filled for easter_egg2
    for(delay=0; delay<4; delay++) {
        tiles.animation_count = 3;
        animate();
    }
    
    // wait until the clear key is pressed
    while(!game.exit) {
        
        // handle keypad presses
        handle_keypad();
        
        // move oiram if required
        move_oiram();
        
        // draw the tilemap at the current oiram offsets
        gfx_Tilemap(&tilemap, oiram.scrollx, oiram.scrolly);
        
        // handle outstanding events, such as showing number of coins
        handle_pending_events();
        
        // handle timer every second
        if (timer_IntStatus & TIMER1_RELOADED) {
            handler_timer();
            timer_IntAcknowledge = TIMER1_RELOADED;
        }
        
        // blit the draw buffer
        gfx_BlitLines(gfx_buffer, 0, 146);
        
        // animate the things
        if (!easter_egg2) {
            animate();
        }
    }
    
    timer_Control = TIMER1_DISABLE;
    
    // deallocate
    while(num_simple_enemies) { remove_simple_enemy(0); }
    while(num_simple_movers)  { remove_simple_mover(0); }
    while(num_chompers)       { remove_chomper(0);      }
    while(num_thwomps)        { remove_thwomp(0);       }
    while(num_flames)         { remove_flame(0);        }
    while(num_boos)           { remove_boo(0);          }
    while(num_poofs)          { remove_poof(0);         }
    while(num_fireballs)      { remove_fireball(0);     }
    while(num_bumped_tiles)   { remove_bumped_tile(0);  }
    free(tilemap.map);
    
    gfx_SetColor(BLACK_INDEX);
    
    if (!game.fastexit) {
        pack->coins = game.coins;
        pack->score = game.score;
        pack->lives = oiram.lives;
        pack->flags = oiram.flags;
        
        black_circles();
        
        // if we entered the end pipe, proceed to next level if it exists
        if (game.enter_end) {
            if (game.level < game.num_levels) {
                game.level++;
                if (pack->progress < game.level) { pack->progress = game.level; }
                if (game.level == game.num_levels) {
                    goto pack_finish;
                }
                goto draw_level;
            } else {
                if (pack->progress == game.num_levels) {
                    goto pack_finish;
                }
                goto main_start;
            }
        } else {
            if (oiram.failed) {
                pack->lives--;
                pack->flags = FLAG_OIRAM_RESET;
                if (!pack->lives) {
                    goto game_over;
                } else {
                    goto draw_level;
                }
            }
        }
    } else {
        goto main_start;
    }

    goto exit;
    
game_over:
    gfx_SetClipRegion(0, 0, LCD_WIDTH, LCD_HEIGHT);
    gfx_SetTextBGColor(BLACK_INDEX);
    memset(pack, 0, sizeof(pack_info_t));
    pack->lives = 15;
    gfx_PrintStringXY("GAME      OVER", 120, 55);
    gfx_BlitBuffer();
    for(delay=0; delay<700000; delay++);
    goto main_start;
    
pack_finish:
    pack_author_len = strlen(pack_author);
    memset(end_str, 0, sizeof end_str);
    memcpy(end_str, pack_author, pack_author_len);
    memcpy(end_str + pack_author_len, " thanks you for playing!", 24);
    
    gfx_SetClipRegion(0, 0, LCD_WIDTH, LCD_HEIGHT);
    for(delay=0; delay<130; delay++) {
        gfx_ShiftDown(1);
        gfx_BlitBuffer();
    }
    gfx_SetTextBGColor(BLACK_INDEX);
    print_centered("Pack Compelete!", 55);
    print_centered(end_str, 71);
    print_centered("Press [enter] to continue", 91);
    gfx_BlitBuffer();
    while(os_GetCSC() != sk_Enter);
    
    goto main_start;
    
exit:
    // save the pack states
    save_progress();
}


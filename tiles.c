/*
 * tiles.c
 * program which demonstraes tile mode 0
 */

/* palette is always 256 colors */
#define PALETTE_SIZE 256

/* include the image we are using */
#include "SpaceBackgroundImage.h"

#include "Sprites/Merged.h"

/* include the tile map we are using */
#include "SpaceBackgroundMap.h"
/* the width and height of the screen */
#define WIDTH 240
#define HEIGHT 160

/* the three tile modes */
#define MODE0 0x00
#define MODE1 0x01
#define MODE2 0x02

/* enable bits for the four tile layers */
#define BG0_ENABLE 0x100
#define BG1_ENABLE 0x200
#define BG2_ENABLE 0x400
#define BG3_ENABLE 0x800

/* define start of tile block for each sprite */
#define Boss 8
#define Enemy1 16
#define Enemy2 24
#define Score 32
#define EnemyBullet 36
#define PlayerBullet 38
#define Zero 40
#define One 42
#define Two 44
#define Three 46
#define Four 48
#define Five 50
#define Six 52
#define Seven 54
#define Eight 56
#define Nine 58

/* flags to set sprite handling in display control register */
#define SPRITE_MAP_2D 0x0
#define SPRITE_MAP_1D 0x40
#define SPRITE_ENABLE 0x1000

/* there are 128 sprites on the GBA */
#define NUM_SPRITES 128

/* the memory location which controls sprite attributes */
volatile unsigned short* sprite_attribute_memory = 
    (volatile unsigned short*) 0x7000000;

/* the memory location which stores sprite image data */
volatile unsigned short* sprite_image_memory = 
    (volatile unsigned short*) 0x6010000;

volatile unsigned short* sprite_palette = 
    (volatile unsigned short*) 0x5000200;

/* flag for turning on DMA */
#define DMA_ENABLE 0x80000000

/* flags for the sizes to transfer, 16 or 32 bits */
#define DMA_16 0x00000000
#define DMA_32 0x04000000

/* pointer to the DMA source location */
volatile unsigned int* dma_source = (volatile unsigned int*) 0x40000D4;

/* pointer to the DMA destination location */
volatile unsigned int* dma_destination = (volatile unsigned int*) 0x40000D8;

/* pointer to the DMA count/control */
volatile unsigned int* dma_count = (volatile unsigned int*) 0x40000DC;

/* copy data using DMA */
void memcpy16_dma(unsigned short* dest, unsigned short* source ,int amount){
    *dma_source = (unsigned int) source;
    *dma_destination = (unsigned int) dest;
    *dma_count = amount | DMA_16 | DMA_ENABLE;
}

/* a sprite is a moveable image on the screen */
struct Sprite {
    unsigned short attribute0;
    unsigned short attribute1;
    unsigned short attribute2;
    unsigned short attribute3;
};

/* a struct for the Player's logic and behavior */
struct Player {
    /* the actual sprite attribute info */
    struct Sprite* sprite;

    /* the x and y postion in pixels */
    int x, y;

    /* the koopa's y velocity in 1/256 pixels/second */
    int yvel;

    /* which frame of the animation he is on */
    int frame;

    /* the number of frames to wait before flipping */
    int animation_delay;

    /* the animation counter counts how many frames until we flip */
    int counter;

    /* whether the koopa is moving right now or not */
    int move;

    /* the number of pixels away from the edge of the screen the koopa stays */
    int border;

    int health;
};

/* used for numbers and Score*/
struct Number{
    struct Sprite* sprite;
    int x,y;
};

/* a struct for an enemies's logic and behavior */
struct Enemy {
    /* the actual sprite attribute info */
    struct Sprite* sprite;

    /* the x and y postion in pixels */
    int x, y;

    /* the koopa's y velocity in 1/256 pixels/second */
    int yvel;

    /* which frame of the animation he is on */
    int frame;

    /* the number of frames to wait before flipping */
    int animation_delay;

    /* the animation counter counts how many frames until we flip */
    int counter;

    /* whether the koopa is moving right now or not */
    int move;

    /* the number of pixels away from the edge of the screen the koopa stays */
    int border;

    int health;
};

/* used for Bullets*/
struct Bullet{
    struct Sprite* sprite;
    int x,y;
};


/* array of all the sprites available on the GBA */
struct Sprite sprites[NUM_SPRITES];
int next_sprite_index = 0;

/* the different sizes of sprites which are possible */
enum SpriteSize {
    SIZE_8_8,
    SIZE_16_16,
    SIZE_32_32,
    SIZE_64_64,
    SIZE_16_8,
    SIZE_32_8,
    SIZE_32_16,
    SIZE_64_32,
    SIZE_8_16,
    SIZE_8_32,
    SIZE_16_32,
    SIZE_32_64
};


/* function to initialize a sprite with its properties, and return a pointer */
struct Sprite* sprite_init(int x, int y, enum SpriteSize size,
    int horizontal_flip, int vertical_flip, int tile_index, int priority) {

    /* grab the next index */
    int index = next_sprite_index++;

    /* tile_index=1; */

    /* setup the bits used for each shape/size possible */
    int size_bits, shape_bits;
    switch (size) {
        case SIZE_8_8:   size_bits = 0; shape_bits = 0; break;
        case SIZE_16_16: size_bits = 1; shape_bits = 0; break;
        case SIZE_32_32: size_bits = 2; shape_bits = 0; break;
        case SIZE_64_64: size_bits = 3; shape_bits = 0; break;
        case SIZE_16_8:  size_bits = 0; shape_bits = 1; break;
        case SIZE_32_8:  size_bits = 1; shape_bits = 1; break;
        case SIZE_32_16: size_bits = 2; shape_bits = 1; break;
        case SIZE_64_32: size_bits = 3; shape_bits = 1; break;
        case SIZE_8_16:  size_bits = 0; shape_bits = 2; break;
        case SIZE_8_32:  size_bits = 1; shape_bits = 2; break;
        case SIZE_16_32: size_bits = 2; shape_bits = 2; break;
        case SIZE_32_64: size_bits = 3; shape_bits = 2; break;
    }

    int h = horizontal_flip ? 1 : 0;
    int v = vertical_flip ? 1 : 0;

    /* set up the first attribute */
    sprites[index].attribute0 = y |             /* y coordinate */
        (0 << 8) |          /* rendering mode */
        (0 << 10) |         /* gfx mode */
        (0 << 12) |         /* mosaic */
        (1 << 13) |         /* color mode, 0:16, 1:256 */
        (shape_bits << 14); /* shape */

    /* set up the second attribute */
    sprites[index].attribute1 = x |             /* x coordinate */
        (0 << 9) |          /* affine flag */
        (h << 12) |         /* horizontal flip flag */
        (v << 13) |         /* vertical flip flag */
        (size_bits << 14);  /* size */

    /* setup the second attribute */
    sprites[index].attribute2 = tile_index |   // tile index */
        (priority << 10) | // priority */
        (0 << 12);         // palette bank (only 16 color)*/

    /* return pointer to this sprite */
    return &sprites[index];
}

/* initialize the koopa */
void player_init(struct Player* koopa) {
    koopa->x = 100;
    koopa->y = 113;
    koopa->yvel = 0;
    koopa->border = 40;
    koopa->frame = 0;
    koopa->move = 0;
    koopa->counter = 0;
    koopa->animation_delay = 8;
    koopa->sprite = sprite_init(koopa->x, koopa->y, SIZE_16_16, 0, 0, 
            koopa->frame, 0);
}

/* initialize an enemy */
void enemy_init(struct Enemy* koopa, int x, int y,int offset) {
    koopa->x = x;
    koopa->y = y;
    koopa->yvel = 0;
    koopa->border = 40;
    koopa->frame = 0;
    koopa->move = 0;
    koopa->counter = 0;
    koopa->animation_delay = 8;
    koopa->sprite = sprite_init(koopa->x, koopa->y, SIZE_16_16, 0, 0, 
            offset, 0);
}

void num_init(struct Number* num,int x, int y, int offset){
    num->x=x;
    num->y=y;
    num->sprite=sprite_init(num->x, num->y, SIZE_8_8, 0, 0, 
            offset, 0);
}

void bullet_init(struct Bullet* num,int x, int y,int offset){
    num->x=x;
    num->y=y;
    num->sprite=sprite_init(num->x, num->y, SIZE_8_8, 0, 0, 
            offset, 0);
}

void score_init(struct Number* num,int x, int y){
    num->x=x;
    num->y=y;
    num->sprite=sprite_init(num->x, num->y, SIZE_8_32, 0, 0, 
        Score, 0);
}

/* update all of the sprites on the screen */
void sprite_update_all() {
    /* copy them all over */
    memcpy16_dma((unsigned short*) sprite_attribute_memory, 
            (unsigned short*) sprites, NUM_SPRITES * 4);
}

/* setup all sprites */
void sprite_clear() {
    /* clear the index counter */
    next_sprite_index = 0;

    /* move all sprites offscreen to hide them */
    for(int i = 0; i < NUM_SPRITES; i++) {
        sprites[i].attribute0 = HEIGHT;
        sprites[i].attribute1 = WIDTH;
    }
}

/* set a sprite postion */
void sprite_position(struct Sprite* sprite, int x, int y) {
    /* clear out the y coordinate */
    sprite->attribute0 &= 0xff00;

    /* set the new y coordinate */
    sprite->attribute0 |= (y & 0xff);

    /* clear out the x coordinate */
    sprite->attribute1 &= 0xfe00;

    /* set the new x coordinate */
    sprite->attribute1 |= (x & 0x1ff);
}

/* move a sprite in a direction */
void sprite_move(struct Sprite* sprite, int dx, int dy) {
    /* get the current y coordinate */
    int y = sprite->attribute0 & 0xff;

    /* get the current x coordinate */
    int x = sprite->attribute1 & 0x1ff;

    /* move to the new location */
    sprite_position(sprite, x + dx, y + dy);
}

/* change the vertical flip flag */
void sprite_set_vertical_flip(struct Sprite* sprite, int vertical_flip) {
    if (vertical_flip) {
        /* set the bit */
        sprite->attribute1 |= 0x2000;
    } else {
        /* clear the bit */
        sprite->attribute1 &= 0xdfff;
    }
}

/* change the vertical flip flag */
void sprite_set_horizontal_flip(struct Sprite* sprite, int horizontal_flip) {
    if (horizontal_flip) {
        /* set the bit */
        sprite->attribute1 |= 0x1000;
    } else {
        /* clear the bit */
        sprite->attribute1 &= 0xefff;
    }
}

/* change the tile offset of a sprite */
void sprite_set_offset(struct Sprite* sprite, int offset) {
    /* clear the old offset */
    sprite->attribute2 &= 0xfc00;

    /* apply the new one */
    sprite->attribute2 |= (offset & 0x03ff);
}

/* setup the sprite image and palette */
void setup_sprite_image() {
    /* load the palette from the image into palette memory size=45*/
    memcpy16_dma((unsigned short*) sprite_palette, 
            (unsigned short*) Merged_palette, PALETTE_SIZE);

    /* load the image into sprite image memory */
    memcpy16_dma((unsigned short*) sprite_image_memory, 
    (unsigned short*) Merged_data, (Merged_width * Merged_height) / 2);
}


void spawn_formation1(struct Enemy* enemy1s[], int size) {
    struct Enemy* enemy1_A = enemy1s[0];
    enemy1_A->x = 52;
    enemy1_A->y = 0;
    sprite_position(enemy1_A->sprite,52,0);
    struct Enemy* enemy1_B = enemy1s[1];
    enemy1_B->x = 112;
    enemy1_B->y = 0;
    sprite_position(enemy1_B->sprite,112,0);
    struct Enemy* enemy1_C = enemy1s[2];
    enemy1_C->x = 172;
    enemy1_C->y = 0;
    sprite_position(enemy1_C->sprite,172,0);
}

void spawn_formation2() {
    struct Enemy* enemy1_A;
    enemy_init(enemy1_A,40,0,Enemy1);
    struct Enemy* enemy1_B;
    enemy_init(enemy1_B,80,0,Enemy1);
    struct Enemy* enemy1_C;
    enemy_init(enemy1_C,120,0,Enemy1);
    struct Enemy* enemy1_D;
    enemy_init(enemy1_D,160,0,Enemy1);
    struct Enemy* enemy1_E;
    enemy_init(enemy1_E,200,0,Enemy1);
}

void spawn_formation3() {
    struct Enemy* enemy1_A;
    enemy_init(enemy1_A,40,0,Enemy1);
    struct Enemy* enemy1_B;
    enemy_init(enemy1_B,80,0,Enemy1);
    struct Enemy* enemy1_C;
    enemy_init(enemy1_C,120,0,Enemy1);
    struct Enemy* enemy1_D;
    enemy_init(enemy1_D,160,0,Enemy1);
    struct Enemy* enemy1_E;
    enemy_init(enemy1_E,200,0,Enemy1);
    struct Enemy* enemy1_F;
    enemy_init(enemy1_F,60,10,Enemy1);
    struct Enemy* enemy1_G;
    enemy_init(enemy1_G,100,10,Enemy1);
    struct Enemy* enemy1_H;
    enemy_init(enemy1_H,140,10,Enemy1);
    struct Enemy* enemy1_I;
    enemy_init(enemy1_I,180,10,Enemy1);
}

void spawn_formation4() {
    struct Enemy* enemy1_A;
    enemy_init(enemy1_A,30,0,Enemy1);
    struct Enemy* enemy1_B;
    enemy_init(enemy1_B,60,0,Enemy1);
    struct Enemy* enemy1_C;
    enemy_init(enemy1_C,90,0,Enemy1);
    struct Enemy* enemy1_D;
    enemy_init(enemy1_D,120,0,Enemy1);
    struct Enemy* enemy1_E;
    enemy_init(enemy1_E,150,0,Enemy1);
    struct Enemy* enemy1_F;
    enemy_init(enemy1_F,180,0,Enemy1);
    struct Enemy* enemy1_G;
    enemy_init(enemy1_G,210,0,Enemy1);
    struct Enemy* enemy2_H;
    enemy_init(enemy2_H,0,0,Enemy2);
    struct Enemy* enemy2_I;
    enemy_init(enemy2_I,140,0,Enemy2);
    struct Enemy* enemy2_J;
    enemy_init(enemy2_J,180,0,Enemy2);
}


/* finds which tile a screen coordinate maps to, 
 * taking scroll into account */
unsigned short tile_lookup(int x, int y, int xscroll, int yscroll,
        const unsigned short* tilemap, int tilemap_w, int tilemap_h) {

    /* adjust for the scroll */
    x += xscroll;
    y += yscroll;

    /* convert from screen coordinates to tile coordinates */
    x >>= 3;
    y >>= 3;

    /* account for wraparound */
    while (x >= tilemap_w) {
        x -= tilemap_w;
    }
    while (y >= tilemap_h) {
        y -= tilemap_h;
    }
    while (x < 0) {
        x += tilemap_w;
    }
    while (y < 0) {
        y += tilemap_h;
    }

    /* the larger screen maps (bigger than 32x32) are made of multiple 
     * stitched
       together - the offset is used for finding which screen block we 
       are in
       for these cases */
    int offset = 0;

    /* if the width is 64, add 0x400 offset to get to tile maps on right  */
    if (tilemap_w == 64 && x >= 32) {
        x -= 32;
        offset += 0x400;
    }

    /* if height is 64 and were down there */
    if (tilemap_h == 64 && y >= 32) {
        y -= 32;

        /* if width is also 64 add 0x800, else just 0x400 */
        if (tilemap_w == 64) {
            offset += 0x800;
        } else {
            offset += 0x400;
        }
    }

    /* find the index in this tile map */
    int index = y * 32 + x;

    /* return the tile */
    return tilemap[index + offset];
}

/* the control registers for the four tile layers */
volatile unsigned short* bg0_control = (volatile unsigned short*) 0x4000008;
volatile unsigned short* bg1_control = (volatile unsigned short*) 0x400000a;
volatile unsigned short* bg2_control = (volatile unsigned short*) 0x400000c;
volatile unsigned short* bg3_control = (volatile unsigned short*) 0x400000e;


/* the display control pointer points to the gba graphics register */
volatile unsigned long* display_control = (volatile unsigned long*) 0x4000000;

/* the address of the color palette */
volatile unsigned short* bg_palette = (volatile unsigned short*) 0x5000000;

/* the button register holds the bits which indicate whether each button has
 * been pressed - this has got to be volatile as well
 */
volatile unsigned short* buttons = (volatile unsigned short*) 0x04000130;

/* scrolling registers for backgrounds */
volatile short* bg0_x_scroll = (unsigned short*) 0x4000010;
volatile short* bg0_y_scroll = (unsigned short*) 0x4000012;
volatile short* bg1_x_scroll = (unsigned short*) 0x4000014;
volatile short* bg1_y_scroll = (unsigned short*) 0x4000016;
volatile short* bg2_x_scroll = (unsigned short*) 0x4000018;
volatile short* bg2_y_scroll = (unsigned short*) 0x400001a;
volatile short* bg3_x_scroll = (unsigned short*) 0x400001c;
volatile short* bg3_y_scroll = (unsigned short*) 0x400001e;


/* the bit positions indicate each button - the first bit is for A, second for
 * B, and so on, each constant below can be ANDED into the register to get the
 * status of any one button */
#define BUTTON_A (1 << 0)
#define BUTTON_B (1 << 1)
#define BUTTON_SELECT (1 << 2)
#define BUTTON_START (1 << 3)
#define BUTTON_RIGHT (1 << 4)
#define BUTTON_LEFT (1 << 5)
#define BUTTON_UP (1 << 6)
#define BUTTON_DOWN (1 << 7)
#define BUTTON_R (1 << 8)
#define BUTTON_L (1 << 9)

/* the scanline counter is a memory cell which is updated to indicate how
 * much of the screen has been drawn */
volatile unsigned short* scanline_counter = (volatile unsigned short*) 0x4000006;

/* wait for the screen to be fully drawn so we can do something during vblank */
void wait_vblank() {
    /* wait until all 160 lines have been updated */
    while (*scanline_counter < 160) { }
}


/* this function checks whether a particular button has been pressed */
unsigned char button_pressed(unsigned short button) {
    /* and the button register with the button constant we want */
    unsigned short pressed = *buttons & button;

    /* if this value is zero, then it's not pressed */
    if (pressed == 0) {
        return 1;
    } else {
        return 0;
    }
}


/* return a pointer to one of the 4 character blocks (0-3) */
volatile unsigned short* char_block(unsigned long block) {
    /* they are each 16K big */
    return (volatile unsigned short*) (0x6000000 + (block * 0x4000));
}

/* return a pointer to one of the 32 screen blocks (0-31) */
volatile unsigned short* screen_block(unsigned long block) {
    /* they are each 2K big */
    return (volatile unsigned short*) (0x6000000 + (block * 0x800));
}


/* function to setup background 0 for this program */
void setup_background() {

    /* load the palette from the image into palette memory*/
    for (int i = 0; i < PALETTE_SIZE; i++) {
        bg_palette[i] = SpaceBackgroundImage_palette[i];
    }

    /* load the image into char block 0 (16 bits at a time) */
    volatile unsigned short* dest = char_block(0);
    unsigned short* image = (unsigned short*) SpaceBackgroundImage_data;
    for (int i = 0; i < ((SpaceBackgroundImage_width * SpaceBackgroundImage_height) / 2); i++) {
        dest[i] = image[i];
    }

    /* set all control the bits in this register */
    *bg0_control = 1 |    /* priority, 0 is highest, 3 is lowest */
        (0 << 2)  |       /* the char block the image data is stored in */
        (0 << 6)  |       /* the mosaic flag */
        (1 << 7)  |       /* color mode, 0 is 16 colors, 1 is 256 colors */
        (16 << 8) |       /* the screen block the tile data is stored in */
        (1 << 13) |       /* wrapping flag */
        (0 << 14);        /* bg size, 0 is 256x256 */
     
    *bg1_control = 0 |
        (0 << 2)  |       /* the char block the image data is stored in */
        (0 << 6)  |       /* the mosaic flag */
        (1 << 7)  |       /* color mode, 0 is 16 colors, 1 is 256 colors */
        (15 << 8) |       /* the screen block the tile data is stored in */
        (1 << 13) |       /* wrapping flag */
        (0 << 14);



    /* load the tile data into screen block 16 */
    dest = screen_block(16);
    for (int i = 0; i < (SpaceBackgroundMap_width * SpaceBackgroundMap_height); i++) {
        dest[i] = SpaceBackgroundMap[i];
    }
}


/* just kill time */
void delay(unsigned int amount) {
    for (int i = 0; i < amount * 10; i++);
}

/*void handle_buttons(){
     scroll with the arrow keys 
    if (button_pressed(BUTTON_DOWN)) {
        yscroll++;
    }
    if (button_pressed(BUTTON_UP)) {
        yscroll--;

    }
    if (button_pressed(BUTTON_RIGHT)) {
        xscroll++;
    }
    if (button_pressed(BUTTON_LEFT)) {
        xscroll--;
    } 
} */

void scrollBG1(int* xscroll, int* yscroll){
    *bg1_x_scroll = *xscroll * 2;
    *bg1_y_scroll = *yscroll * 2;
}

/* automatically scroll background 0 */
void scrollBG0(int* xscroll, int* yscroll, int* count){
    *count+=1;
    if ((*count) == 3){
        *xscroll+= 0;
        *yscroll-= 1;
        *bg0_x_scroll = *xscroll;
        *bg0_y_scroll = *yscroll;
        *count=0;
    }
}

void player_update(struct Player* player) {
    sprite_position(player->sprite, player->x, player->y);
}





/* the main function */
int main() {
    /* we set the mode to mode 0 with bg0 on */
    *display_control = MODE0 | BG0_ENABLE | SPRITE_ENABLE | SPRITE_MAP_1D; 
        /* we don't use BG1 yet | BG1_ENABLE;*/

    /* setup the background 0 */
    setup_background();

    setup_sprite_image();
    sprite_clear();
    struct Player* player;
    player_init(player);

    /*
    struct Enemy* boss;
    enemy_init(boss,96,0,Boss);
    
    struct Enemy* enemy1;
    enemy_init(enemy1,32,0,Enemy1);

    struct Enemy* enemy2;
    enemy_init(enemy2,64,0,Enemy2);
    
    struct Enemy* enemy1s[15];
    initializeAll_Enemy1(enemy1s, 15);
    struct Enemy* enemy2s[15];
    initializeAll_Enemy2(enemy2s, 15);
    struct Enemy* bosses[5];
    initializeAll_Boss(bosses, 5);    
    
    //spawn_formation1(enemy1s, 15);

    */
    
    // Initialize all enemy 1s
    struct Enemy* enemy1_A;
    enemy_init(enemy1_A,WIDTH,HEIGHT,Enemy1);
    struct Enemy* enemy1_B;
    enemy_init(enemy1_B,WIDTH,HEIGHT,Enemy1);
    /*
    struct Enemy* enemy1_C;
    enemy_init(enemy1_C,WIDTH,HEIGHT,Enemy1);
    struct Enemy* enemy1_D;
    enemy_init(enemy1_D,WIDTH,HEIGHT,Enemy1);
    struct Enemy* enemy1_E;
    enemy_init(enemy1_E,WIDTH,HEIGHT,Enemy1);
    struct Enemy* enemy1_F;
    enemy_init(enemy1_F,WIDTH,HEIGHT,Enemy1);
    struct Enemy* enemy1_G;
    enemy_init(enemy1_G,WIDTH,HEIGHT,Enemy1);
    struct Enemy* enemy1_H;
    enemy_init(enemy1_H,WIDTH,HEIGHT,Enemy1);
    struct Enemy* enemy1_I;
    enemy_init(enemy1_I,WIDTH,HEIGHT,Enemy1);
    struct Enemy* enemy1_J;
    enemy_init(enemy1_J,WIDTH,HEIGHT,Enemy1);
    struct Enemy* enemy1_K;
    enemy_init(enemy1_K,WIDTH,HEIGHT,Enemy1);
    struct Enemy* enemy1_L;
    enemy_init(enemy1_L,WIDTH,HEIGHT,Enemy1);
    struct Enemy* enemy1_M;
    enemy_init(enemy1_M,WIDTH,HEIGHT,Enemy1);
    struct Enemy* enemy1_N;
    enemy_init(enemy1_N,WIDTH,HEIGHT,Enemy1);
    struct Enemy* enemy1_O;
    enemy_init(enemy1_O,WIDTH,HEIGHT,Enemy1);

    // Initialize all enemy 2s
    struct Enemy* enemy2_A;
    enemy_init(enemy2_A,WIDTH,HEIGHT,Enemy2);
    struct Enemy* enemy2_B;
    enemy_init(enemy2_B,WIDTH,HEIGHT,Enemy2);
    struct Enemy* enemy2_C;
    enemy_init(enemy2_C,WIDTH,HEIGHT,Enemy2);
    struct Enemy* enemy2_D;
    enemy_init(enemy2_D,WIDTH,HEIGHT,Enemy2);
    struct Enemy* enemy2_E;
    enemy_init(enemy2_E,WIDTH,HEIGHT,Enemy2);
    struct Enemy* enemy2_F;
    enemy_init(enemy2_F,WIDTH,HEIGHT,Enemy2);
    struct Enemy* enemy2_G;
    enemy_init(enemy2_G,WIDTH,HEIGHT,Enemy2);
    struct Enemy* enemy2_H;
    enemy_init(enemy2_H,WIDTH,HEIGHT,Enemy2);
    struct Enemy* enemy2_I;
    enemy_init(enemy2_I,WIDTH,HEIGHT,Enemy2);
    struct Enemy* enemy2_J;
    enemy_init(enemy2_J,WIDTH,HEIGHT,Enemy2);
    struct Enemy* enemy2_K;
    enemy_init(enemy2_K,WIDTH,HEIGHT,Enemy2);
    struct Enemy* enemy2_L;
    enemy_init(enemy2_L,WIDTH,HEIGHT,Enemy2);
    struct Enemy* enemy2_M;
    enemy_init(enemy2_M,WIDTH,HEIGHT,Enemy2);
    struct Enemy* enemy2_N;
    enemy_init(enemy2_N,WIDTH,HEIGHT,Enemy2);
    struct Enemy* enemy2_O;
    enemy_init(enemy2_O,WIDTH,HEIGHT,Enemy2);

    // Initialize all bosses
    struct Enemy* boss_A;
    enemy_init(boss_A,WIDTH,HEIGHT,Boss);
    struct Enemy* boss_B;
    enemy_init(boss_B,WIDTH,HEIGHT,Boss);
    struct Enemy* boss_C;
    enemy_init(boss_C,WIDTH,HEIGHT,Boss);
    struct Enemy* boss_D;
    enemy_init(boss_D,WIDTH,HEIGHT,Boss);
    struct Enemy* boss_E;
    enemy_init(boss_E,WIDTH,HEIGHT,Boss);
    */

    enemy1_A->x = 52;
    enemy1_A->y = 0;
    sprite_position(enemy1_A->sprite,enemy1_A->x,enemy1_A->y);
    
    enemy1_B->x = 112;
    enemy1_B->y = 0;
    sprite_position(enemy1_B->sprite,enemy1_B->x,enemy1_B->y);
    /*
    enemy1_C->x = 172;
    enemy1_C->y = 0;
    sprite_position(enemy1_C->sprite,enemy1_C->x,enemy1_C->y);
    */

    struct Enemy* enemy1Test;
    enemy_init(enemy1Test,WIDTH,HEIGHT,Enemy1);
    enemy1Test->x = 120;
    enemy1Test->y = 60;
    sprite_position(enemy1Test->sprite,enemy1Test->x,enemy1Test->y);


    struct Bullet* pBullet;
    bullet_init(pBullet,128,0,PlayerBullet);

    struct Bullet* eBullet;
    bullet_init(eBullet,136,0,EnemyBullet);

    struct Number* score;
    score_init(score,0,32);

    struct Number* zero;
    num_init(zero,32,32,Zero);

    struct Number* one;
    num_init(one,40,32,One);

    struct Number* two;
    num_init(two,48,32,Two);

    struct Number* three;
    num_init(three,56,32,Three);

    struct Number* four;
    num_init(four,64,32,Four);

    struct Number* five;
    num_init(five,72,32,Five);

    struct Number* six;
    num_init(six,80,32,Six);

    struct Number* seven;
    num_init(seven,88,32,Seven);

    struct Number* eight;
    num_init(eight,96,32,Eight);

    struct Number* nine;
    num_init(nine,104,32,Nine);

    /* set initial scroll to 0 */
    int xscroll = 0;
    int yscroll = 0;
    int scrollCount = 0;

    /* loop forever */
    while (1) {
        player_update(player); 

        if(button_pressed(BUTTON_RIGHT)){
            player->x += 1;
            sprite_move(player->sprite, 1, 0);
        } else if (button_pressed(BUTTON_LEFT)){
            player->x -= 1; 
            sprite_move(player->sprite, -1, 0);
        }        

        scrollBG0(&xscroll,&yscroll,&scrollCount);
        /* set on screen position */
        sprite_update_all();

        /* wait for vblank before scrolling */
        wait_vblank();
        sprite_position(player->sprite, player->x, player->y);
        /* delay some */
        delay(200);
    }
}


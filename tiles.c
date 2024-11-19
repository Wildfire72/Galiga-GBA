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
#define SCORE 32
#define EnemyBullet 42
#define PlayerBullet 40
#define Zero 44
#define One 46
#define Two 48
#define Three 50
#define Four 52
#define Five 54
#define Six 56
#define Seven 58
#define Eight 60
#define Nine 62

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

/*declaration of increaseScore*/
int increaseScore(int score,int offset);

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

/* used for numbers*/
struct Number{
    struct Sprite* sprite;
    int x,y;
};

struct Score{
    struct Sprite* sprite;
    int x,y;
    int score;
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

    /* the health of the enemy sprite */
    int health;

    /* whether the enemy sprite is alive (1 for true or 0 for false) */
    int isAlive;
};

/* used for Bullets*/
struct Bullet{
    struct Sprite* sprite;
    int x,y;
    
    int yvel;    

    int active; 
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
    koopa->x = 112;
    koopa->y = 144;
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
    koopa->animation_delay = 12;
    koopa->health = 10;
    koopa->isAlive = 0;
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
    num->active=0;
    num->yvel=0;  
    num->sprite=sprite_init(num->x, num->y, SIZE_8_8, 0, 0, 
            offset, 0);
}

void init_bullets(struct Bullet pBullets[], int size){
    for( int i = 0; i < size; i++){
        struct Bullet pbullet;
        bullet_init(&pbullet, WIDTH /2, 0, PlayerBullet); 
        pBullets[i] = pbullet;  
    }
} 

void score_init(struct Score* num,int x, int y){
    num->x=x;
    num->y=y;
    num->score=0;
    num->sprite=sprite_init(num->x, num->y, SIZE_32_8, 0, 0, 
        SCORE, 0);
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

void initializeAll_Enemy1(struct Enemy enemy1Array[], int size) {
    for (int i = 0; i < size; i++) {
        struct Enemy enemy1;
        enemy_init(&enemy1,WIDTH,HEIGHT,Enemy1);
        enemy1Array[i] = enemy1;
    }
}

void initializeAll_Enemy2(struct Enemy enemy2Array[], int size) {
    for (int i = 0; i < size; i++) {
        struct Enemy enemy2;
        enemy_init(&enemy2,WIDTH,HEIGHT,Enemy2);
        enemy2Array[i] = enemy2;
    }
}

void initializeAll_Boss(struct Enemy bossArray[], int size) {
    for (int i = 0; i < size; i++) {
        struct Enemy boss;
        enemy_init(&boss,WIDTH,HEIGHT,Boss);
        bossArray[i] = boss;
    }
}

void spawn_formation1(struct Enemy enemy1s[]) {
    int xStart = 52;
    int xOffset = 60;
    for (int i = 0; i < 3; i++) {
        enemy1s[i].x = xStart + (xOffset * i);
        enemy1s[i].y = -16;
        sprite_position(enemy1s[i].sprite,enemy1s[i].x,enemy1s[i].y);
        enemy1s[i].isAlive = 1;
    }
}

void spawn_formation2(struct Enemy enemy1s[]) {
    int xStart = 32;
    int xOffset = 32;
    for (int i = 0; i < 6; i++) {
        enemy1s[i].x = xStart + (xOffset * i);
        enemy1s[i].y = -16;
        sprite_position(enemy1s[i].sprite,enemy1s[i].x,enemy1s[i].y);
        enemy1s[i].isAlive = 1;
    }
}

void spawn_formation3(struct Enemy enemy1s[]) {
    int r1_xStart = 40;
    int r1_xOffset = 36;
    for (int i = 0; i < 5; i++) {
        enemy1s[i].x = r1_xStart + (r1_xOffset * i);
        enemy1s[i].y = -28;
        sprite_position(enemy1s[i].sprite,enemy1s[i].x,enemy1s[i].y);
        enemy1s[i].isAlive = 1; 
    }
    int r2_xStart = 58;
    int r2_xOffset = 36;
    for (int i = 5; i < 9; i++) {
        enemy1s[i].x = r2_xStart + (r2_xOffset * (i - 5));
        enemy1s[i].y = -16;
        sprite_position(enemy1s[i].sprite,enemy1s[i].x,enemy1s[i].y);
        enemy1s[i].isAlive = 1;
    } 
}

void spawn_formation4(struct Enemy enemy1s[], struct Enemy enemy2s[]) {
    int xOffset = 28;
    int r1_xStart = 14;
    for (int i = 0; i < 8; i++) {
        enemy1s[i].x = r1_xStart + (xOffset * i);
        enemy1s[i].y = -28;
        sprite_position(enemy1s[i].sprite,enemy1s[i].x,enemy1s[i].y);
        enemy1s[i].isAlive = 1;
    }
    int r2_xStart = 56;
    for (int i = 0; i < 5; i++) {
        enemy2s[i].x = r2_xStart + (xOffset * i);
        enemy2s[i].y = -16;
        sprite_position(enemy2s[i].sprite,enemy2s[i].x,enemy2s[i].y);
        enemy2s[i].isAlive = 1;
    } 
}

void spawn_formation5(struct Enemy enemy1s[], struct Enemy enemy2s[]) {
    int xOffset = 20;
    int e1_xStart = 52;
    for (int i = 0; i < 7; i++) {
        enemy1s[i].x = e1_xStart + (xOffset * i);
        if (i % 2 == 0) {
            enemy1s[i].y = -32;
        } else {
            enemy1s[i].y = -16;
        }
        sprite_position(enemy1s[i].sprite,enemy1s[i].x,enemy1s[i].y);
        enemy1s[i].isAlive = 1;
    }
    int e2_xStart = 32;
    for (int i = 0; i < 9; i++) {
        enemy2s[i].x = e2_xStart + (xOffset * i);
        if (i % 2 == 0) {
            enemy2s[i].y = -32;
        } else {
            enemy2s[i].y = -16;
        }
        sprite_position(enemy2s[i].sprite,enemy2s[i].x,enemy2s[i].y);
        enemy2s[i].isAlive = 1;
    }
}

void spawn_formation6(struct Enemy enemy1s[], struct Enemy enemy2s[]) {
    int xOffset = 24;
    int r1_xStart = 16;
    for (int i = 0; i < 9; i++) {
        enemy2s[i].x = r1_xStart + (xOffset * i);
        enemy2s[i].y = -36;
        sprite_position(enemy2s[i].sprite,enemy2s[i].x,enemy2s[i].y);
        enemy2s[i].isAlive = 1;
    }
    int r2_xStart = 28;
    for (int i = 0; i < 8; i++) {
        enemy1s[i].x = r2_xStart + (xOffset * i);
        enemy1s[i].y = -26;
        sprite_position(enemy1s[i].sprite,enemy1s[i].x,enemy1s[i].y);
        enemy1s[i].isAlive = 1;
    }
    int r3_xStart = 40;
    for (int i = 9; i < 16; i++) {
        enemy2s[i].x = r3_xStart + (xOffset * (i - 9));
        enemy2s[i].y = -16;
        sprite_position(enemy2s[i].sprite,enemy2s[i].x,enemy2s[i].y);
        enemy2s[i].isAlive = 1;
    }
}

void spawn_formation7(struct Enemy enemy1s[], struct Enemy enemy2s[], struct Enemy bosses[]) {
    int enemy1Index = 0;
    int enemy2Index = 0;
    int xOffset = 18;
    int r1_xStart = 4;
    for (int i = 0; i < 13; i++) {
        if (i == 6) {
            bosses[0].x = r1_xStart + (xOffset * i);
            bosses[0].y = -44;
            sprite_position(bosses[0].sprite,bosses[0].x,bosses[0].y);
            bosses[0].isAlive = 1;
        } else if (i == 0 || i == 1 || i == 4 || i == 5 || i == 7 || i == 8 || i == 11 || i == 12) {
            enemy1s[enemy1Index].x = r1_xStart + (xOffset * i);
            enemy1s[enemy1Index].y = -44;
            sprite_position(enemy1s[enemy1Index].sprite,enemy1s[enemy1Index].x,enemy1s[enemy1Index].y);
            enemy1s[enemy1Index].isAlive = 1;
            enemy1Index++;
        } else {
            enemy2s[enemy2Index].x = r1_xStart + (xOffset * i);
            enemy2s[enemy2Index].y = -44;
            sprite_position(enemy2s[enemy2Index].sprite,enemy2s[enemy2Index].x,enemy2s[enemy2Index].y);
            enemy2s[enemy2Index].isAlive = 1;
            enemy2Index++;
        }
    }
    int r2_xStart = 40;
    for (int i = 0; i < 9; i++) {
        if (i == 0 || i == 1 || i == 4 || i == 7 || i == 8) {
            enemy1s[enemy1Index].x = r2_xStart + (xOffset * i);
            enemy1s[enemy1Index].y = -30;
            sprite_position(enemy1s[enemy1Index].sprite,enemy1s[enemy1Index].x,enemy1s[enemy1Index].y);
            enemy1s[enemy1Index].isAlive = 1;
            enemy1Index++;
        } else {
            enemy2s[enemy2Index].x = r2_xStart + (xOffset * i);
            enemy2s[enemy2Index].y = -30;
            sprite_position(enemy2s[enemy2Index].sprite,enemy2s[enemy2Index].x,enemy2s[enemy2Index].y);
            enemy2s[enemy2Index].isAlive = 1;
            enemy2Index++;
        }
    }
    int r3_xStart = 76;
    for (int i = 0; i < 5; i++) {
        if (i != 2) {
            enemy1s[enemy1Index].x = r3_xStart + (xOffset * i);
            enemy1s[enemy1Index].y = -16;
            sprite_position(enemy1s[enemy1Index].sprite,enemy1s[enemy1Index].x,enemy1s[enemy1Index].y);
            enemy1s[enemy1Index].isAlive = 1;
            enemy1Index++;
        } else {
            enemy2s[enemy2Index].x = r3_xStart + (xOffset * i);
            enemy2s[enemy2Index].y = -16;
            sprite_position(enemy2s[enemy2Index].sprite,enemy2s[enemy2Index].x,enemy2s[enemy2Index].y);
            enemy2s[enemy2Index].isAlive = 1;
            enemy2Index++;
        }
    }
}

void spawn_EnemyFormation(int formationNum, struct Enemy enemy1s[], struct Enemy enemy2s[], struct Enemy bosses[]) {
    if (formationNum == 1) {
        spawn_formation1(enemy1s);
    } else if (formationNum == 2) {
        spawn_formation2(enemy1s);
    } else if (formationNum == 3) {
        spawn_formation3(enemy1s);
    } else if (formationNum == 4) {
        spawn_formation4(enemy1s, enemy2s);
    } else if (formationNum == 5) {
        spawn_formation5(enemy1s, enemy2s);
    } else if (formationNum == 6) {
        spawn_formation6(enemy1s, enemy2s);
    } else if (formationNum == 7) {
        spawn_formation7(enemy1s, enemy2s, bosses);
    }
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

/* kill an enemy if its health has reached zero */
void enemy_checkDeath(struct Enemy* enemy) {
    if (enemy->health <= 0) {
        enemy->isAlive = 0;
        sprite_position(enemy->sprite, WIDTH, HEIGHT);
    }
}

/* check if a bullet has collided with an enemy */
void bulletEnemy_Collision(struct Bullet* pBullet, struct Enemy enemy1s[], struct Enemy enemy2s[], struct Enemy bosses[]) {
    for (int j = 0; j < 20; j++) {
        if (enemy1s[j].isAlive) {
            if ((pBullet->x + 8 >= enemy1s[j].x && pBullet->x <= enemy1s[j].x + 12) && pBullet->y <= enemy1s[j].y + 12) {
                enemy1s[j].health -= 10;
                enemy_checkDeath(&enemy1s[j]);
                pBullet->active = 0;
                pBullet->yvel = 0;
                pBullet->x = -16;
                pBullet->y = -16;
                sprite_position(pBullet->sprite, pBullet->x, pBullet->y);
            }
        }
    }
    for (int j = 0; j < 20; j++) {
        if (enemy2s[j].isAlive) {
            if (pBullet->x + 4 >= enemy2s[j].x && pBullet->x <= enemy2s[j].x + 12 && pBullet->y <= enemy2s[j].y + 12) {
                enemy2s[j].health -= 10;
                enemy_checkDeath(&enemy2s[j]);
                sprite_position(enemy2s[j].sprite, WIDTH, HEIGHT);
                pBullet->active = 0;
                pBullet->yvel = 0;
                pBullet->x = -16;
                pBullet->y = -16;
                sprite_position(pBullet->sprite, pBullet->x, pBullet->y);    
            }
        }
    }
    for (int j = 0; j < 3; j++) {
        if (bosses[j].isAlive) {
            if (pBullet->x + 4 >= bosses[j].x && pBullet->x <= bosses[j].x + 12 && pBullet->y <= bosses[j].y + 12) {
                bosses[j].health -= 10;
                enemy_checkDeath(&bosses[j]);
                pBullet->active = 0;
                pBullet->yvel = 0;
                pBullet->x = -16;
                pBullet->y = -16;
                sprite_position(pBullet->sprite, pBullet->x, pBullet->y); 
            }
        }
    }
}

void update_bullet(struct Bullet* pbullet, struct Enemy enemy1s[], struct Enemy enemy2s[], struct Enemy bosses[]) {
   // if the bullet has been fired and hits top of the screen, reset the bullet
    if(pbullet->active == 1 && pbullet->y <=0){
        pbullet->active = 0;
        pbullet->yvel = 0;
        pbullet->x = -16;
        pbullet->y = -16;
        sprite_position(pbullet->sprite, pbullet->x, pbullet->y);
    //if the bullet has been fired and hasn't hit top, keep moving          
    }else if(pbullet->active == 1 && pbullet->y > 0){
        pbullet->y -= 1;
        sprite_move(pbullet->sprite, 0, pbullet->yvel);
        bulletEnemy_Collision(pbullet, enemy1s, enemy2s, bosses);
    }else if(pbullet->active == 0){
        pbullet->yvel = 0;
        pbullet->x = -16;
        pbullet->y = -16;
        sprite_position(pbullet->sprite, pbullet->x , pbullet->y);
    }
}

void update_bullets(struct Bullet pBullets[], struct Enemy enemy1s[], struct Enemy enemy2s[], struct Enemy bosses[]){
     for(int i = 0; i < 20; i++){
        update_bullet(&pBullets[i], enemy1s, enemy2s, bosses);
     } 
}


/* update the player sprite */
void player_update(struct Player* player) {
    sprite_position(player->sprite, player->x, player->y);
}

//void Pbullet_update(struct Bullet* bullet) {
  //  if
//}

/* update an enemy sprite */
void enemy_update(struct Enemy* enemy) {
    if (enemy->isAlive) {
        enemy->counter++;
        if (enemy->counter >= enemy->animation_delay) {
            enemy->y += 1;
            sprite_move(enemy->sprite, 0, 1);
            enemy->counter = 0;
        }
    }
}

/* check if an enemy has hit the bottom of the screen */
void enemy_screenCollision(struct Enemy* enemy) {
    if (enemy->y >= HEIGHT - 12) {
        //game_loss();
    }
}

/* check if the current formation has been beaten (1=yes, 0=no) */
int formation_check(struct Enemy enemy1s[], int enemy1Size, struct Enemy enemy2s[], int enemy2Size, struct Enemy bosses[], int bossSize) {
    for (int i = 0; i < enemy1Size; i++) {
        if (enemy1s[i].isAlive == 1) {
            return 0;
        }
    }
    for (int i = 0; i < enemy2Size; i++) {
        if (enemy2s[i].isAlive == 1) {
            return 0;
        }
    }
    for (int i = 0; i < bossSize; i++) {
        if (bosses[i].isAlive == 1) {
            return 0;
        }
    }
    return 1;
}

/* update an enemy formation */
void formation_update(int formationNum, struct Enemy enemy1s[], struct Enemy enemy2s[], struct Enemy bosses[]) {
    if (formationNum == 1) {
        for (int i = 0; i < 3; i++) {
            enemy_update(&enemy1s[i]);
        }
    } else if (formationNum == 2) {
        for (int i = 0; i < 6; i++) {
            enemy_update(&enemy1s[i]);
        }
    } else if (formationNum == 3) {
        for (int i = 0; i < 9; i++) {
            enemy_update(&enemy1s[i]);
        }
    } else if (formationNum == 4) {
        for (int i = 0; i < 8; i++) {
            enemy_update(&enemy1s[i]);
        }
        for (int i = 0; i < 5; i++) {
            enemy_update(&enemy2s[i]);
        }
    } else if (formationNum == 5) {
        for (int i = 0; i < 7; i++) {
            enemy_update(&enemy1s[i]);
        }
        for (int i = 0; i < 9; i++) {
            enemy_update(&enemy2s[i]);
        }
    } else if (formationNum == 6) {
        for (int i = 0; i < 8; i++) {
            enemy_update(&enemy1s[i]);
        }
        for (int i = 0; i < 16; i++) {
            enemy_update(&enemy2s[i]);
        }
    } else if (formationNum == 7) {
        for (int i = 0; i < 17; i++) {
            enemy_update(&enemy1s[i]);
        }
        for (int i = 0; i < 9; i++) {
            enemy_update(&enemy2s[i]);
        }
        enemy_update(&bosses[0]);
    }
}

/*updates the sprites the score is displaying*/
void updateScore(struct Score* s){
    int score=(s->score);
    int digits=1;
    int check=score%10;
    while (check!=score){
        digits++;
        int num=1;
        for (int i=0;i<digits;i++){
            num*=digits;
        }
        check=score%num;
    }
    /*Score palette starts at 100*/
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
    struct Player player;
    player_init(&player);

    struct Enemy enemy1s[20];
    initializeAll_Enemy1(enemy1s, 20);
    struct Enemy enemy2s[20];
    initializeAll_Enemy2(enemy2s, 20);
    struct Enemy bosses[3];
    initializeAll_Boss(bosses, 3);    

    spawn_EnemyFormation(7, enemy1s, enemy2s, bosses);
    
    struct Bullet playerBullets[20];
    
    init_bullets(playerBullets, 20);

    struct Bullet eBullet;
    bullet_init(&eBullet,136,64,EnemyBullet);

    struct Score score;
    score_init(&score,0,0);

    struct Number zero;
    num_init(&zero,32,32,Zero);

    struct Number one;
    num_init(&one,40,32,One);

    struct Number two;
    num_init(&two,48,32,Two);

    struct Number three;
    num_init(&three,56,32,Three);

    struct Number four;
    num_init(&four,64,32,Four);

    struct Number five;
    num_init(&five,72,32,Five);

    struct Number six;
    num_init(&six,80,32,Six);

    struct Number seven;
    num_init(&seven,88,32,Seven);

    struct Number eight;
    num_init(&eight,96,32,Eight);

    struct Number nine;
    num_init(&nine,104,32,Nine);

    /* set initial scroll to 0 */
    int xscroll = 0;
    int yscroll = 0;
    int scrollCount = 0;
    int bulletCount = 0;
    /* loop forever */
    int firingCounter = 0;
    while (1) {
        player_update(&player); 
        

        if(button_pressed(BUTTON_RIGHT) && player.x < 224 ){
            player.x += 1;
            sprite_move(player.sprite, 1, 0);
        } else if (button_pressed(BUTTON_LEFT) && player.x > 0 ){
            player.x -= 1; 
            sprite_move(player.sprite, -1, 0);
        } else if (button_pressed(BUTTON_SELECT)){
           // if(bulletCount >= 5){
            //    bulletCount = 0;
           // }
            for(int i = 0; i < 20; i++){
                if(playerBullets[i].active == 0 && firingCounter >= 30){
                    playerBullets[i].x = player.x + 4; 
                    playerBullets[i].y = player.y -2; 
                    playerBullets[i].active = 1;
                    playerBullets[i].yvel = -1;
                    sprite_position(playerBullets[i].sprite, player.x + 4, player.y -2 );
                    firingCounter = 0; 
                   // bulletCount += 1;
                    break;
                }
            }
           // bulletCount += 1; 
        }        
        updateScore(&score);
        formation_update(7, enemy1s, enemy2s, bosses);

        update_bullets(playerBullets, enemy1s, enemy2s, bosses); 
        sprite_position(player.sprite, player.x , player.y);

        scrollBG0(&xscroll,&yscroll,&scrollCount);
        /* set on screen position */
        sprite_update_all();

        /* wait for vblank before scrolling */
        wait_vblank();
        firingCounter += 1;  
        /* delay some */
        delay(200);
    }
}

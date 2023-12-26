#include "stm32f4xx_hal.h"
#include <graphics.h>
#include <ssd1306.h>
#include <ran.h>
#include <audio.h>
#include <SWO_debug.h>

#define NEAR_CLIPPING 3
#define CUBE_COUNT 50
#define ROOM_SIDES 8
#define ROOM_SIZE  10

typedef struct {
    uint8_t alive;
    float scale;
    RanVec3 pos, rot, ang_vel;
}AnimCube;

static AnimCube cube_list[CUBE_COUNT];
static float camera_fov = 1.6f;
static uint32_t cube_idx = 0;
static uint8_t avg_audio_cursor = 0;
static float avg_audio[16];
static float cube_speed = 0.1;

static const RanVec3 cube_verts[] = {
    {1, 1, -1},
    {-1, 1, -1},
    {-1, -1, -1},
    {1, -1, -1},

    {1, 1, 1},
    {-1, 1, 1},
    {-1, -1, 1},
    {1, -1, 1},
};

static RanVec3 room_verts[ROOM_SIDES * 2];

void graphics_init(){
    oled_init();
    avg_audio_cursor = 0;
    for(uint8_t i = 0; i != CUBE_COUNT; i++){
        cube_list[i].scale = 0.1;
    }

    for(uint8_t i = 0; i != ROOM_SIDES; i++){
        RanVec2 p = ran_fast_polar((i / (float)ROOM_SIDES) * 2 * RAN_PI, ROOM_SIZE);
        room_verts[i] = (RanVec3){p.x, 50, p.y};
        room_verts[i + ROOM_SIDES] = (RanVec3){p.x, 17, p.y};
    }
}

static void graphics_process_cube(AnimCube *cube){
    if(!cube->alive)return;
    
    //projected vertices
    int p_verts[8][2];
    uint8_t culled_verts = 0;
    for(uint8_t i = 0; i != sizeof(cube_verts) / sizeof(RanVec3); i++){
        RanVec3 t_vert = ran_v3_scale(cube_verts[i], cube->scale);
        t_vert = ran_v3_rot(t_vert, cube->rot);
        t_vert = ran_v3_add(t_vert, cube->pos);

        if(t_vert.y < NEAR_CLIPPING){
            t_vert.y = NEAR_CLIPPING;
            culled_verts++;
        }

        RanVec2 projected_p = ran_proj(camera_fov, t_vert);
        p_verts[i][0] = projected_p.x * 32 + 64;
        p_verts[i][1] = projected_p.y * 32 + 32;
    }
    if(culled_verts == 8)cube->alive = 0;

    cube->pos.y -= cube_speed;
    cube->rot = ran_v3_add(cube->rot, cube->ang_vel);

#define LINE(p1, p2) \
    oled_draw_line(p_verts[p1][0], p_verts[p1][1], p_verts[p2][0], p_verts[p2][1])

    LINE(0, 1);
    LINE(1, 2);
    LINE(2, 3);
    LINE(3, 0);

    LINE(4, 5);
    LINE(5, 6);
    LINE(6, 7);
    LINE(7, 4);

    LINE(0, 4);
    LINE(1, 5);
    LINE(2, 6);
    LINE(3, 7);
#undef LINE
}


static void run_spawner(){
    static uint16_t tick = 0;
    if(!(tick % 20)){

        for(uint8_t i = 0; i != 3; i++){
        AnimCube *cube = cube_list + cube_idx;
        //if(rand() & 1)goto end;
        if(cube->alive)goto end;
        cube_idx++;
        cube_idx %= CUBE_COUNT;
        cube->alive = 1;

        RanVec2 orig_pos = ran_fast_polar(tick * 0.031 + i * (2 * RAN_PI / 3.0f), 5);//ran_randf() * 2 * RAN_PI, ran_randf() * 5 + 1);
        //cube->scale = 1;//ran_fast_sin(cube_idx / (float)CUBE_COUNT * 2 * RAN_PI) * 0.5 + 0.6;
        cube->pos = (RanVec3){orig_pos.x, 40, orig_pos.y};
        cube->rot = (RanVec3){0, 0, 0};
        cube->ang_vel = (RanVec3){
            0.03f * (ran_randf() * 2.0f - 1.0f),
            0.03f * (ran_randf() * 2.0f - 1.0f),
            0.03f * (ran_randf() * 2.0f - 1.0f)
        };
        }
    }
end:
    tick++;
}

static void graphics_draw_cubes(){
    static uint32_t tick = 0;
    float value = audio_get_sample_vol();

    avg_audio[avg_audio_cursor] = value;
    avg_audio_cursor++;
    avg_audio_cursor &= (sizeof(avg_audio) / sizeof(float) - 1);

    float f_value = 0;
    for(uint8_t i = 0; i != sizeof(avg_audio) / sizeof(float); i++){
        f_value += avg_audio[i];
    }

    f_value *= (1 / (float)(sizeof(avg_audio) / sizeof(float)));
    cube_speed = 0.3f * f_value + 0.14f;
    camera_fov = (1 - value) * 0.8f + 2.1f;

    run_spawner();

    for(uint8_t i = 0; i != CUBE_COUNT; i++){
        cube_list[i].scale = value + 0.3f;
        graphics_process_cube(cube_list + i);
    }

    int8_t line_offset = 3 + 33 * value;
    oled_draw_line(128 - line_offset, 64, 128, 64 - line_offset);
    oled_draw_line(line_offset, 64, 0, 64 - line_offset);
    oled_draw_line(128 - line_offset, 0, 128, line_offset);
    oled_draw_line(line_offset, 0, 0, line_offset);

    int room_verts_ss[ROOM_SIDES * 2][2];
    float rot_ang = 0.002f*tick;
    RanVec2 R = {ran_fast_cos(rot_ang * RAN_PI), ran_fast_sin(rot_ang * RAN_PI)};
    RanVec2 S = {-R.y, R.x};
    for(uint8_t i = 0; i != ROOM_SIDES * 2; i++){
        RanVec3 p = room_verts[i];
        p = (RanVec3){
            ran_v2_dot((RanVec2){p.x, p.z}, R),
            p.y,
            ran_v2_dot((RanVec2){p.x, p.z}, S)
        };

        RanVec2 room_vert_p = ran_proj(camera_fov, p);
        room_verts_ss[i][0] = room_vert_p.x * 32 + 64;
        room_verts_ss[i][1] = room_vert_p.y * 32 + 32;
    }
#define LINE(p1, p2) \
    oled_draw_line_dotted(room_verts_ss[p1][0], room_verts_ss[p1][1], room_verts_ss[p2][0], room_verts_ss[p2][1])
    //Compile time unrolling help me
    #pragma GCC unroll 99
    for(uint8_t i = 0; i != ROOM_SIDES; i++){
        LINE(i, (i + 1) % ROOM_SIDES);
        LINE(i, i + ROOM_SIDES);
        LINE(i + ROOM_SIDES, (i + 1) % ROOM_SIDES + ROOM_SIDES);
    }
#undef LINE

    oled_show();
    tick++;
}

void graphics_update(){
    static uint32_t prev_tick;
    if(HAL_GetTick() - prev_tick > 15){
        graphics_draw_cubes();
        prev_tick = HAL_GetTick();
    }
}

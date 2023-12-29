#include "stm32f4xx_hal.h"
#include <graphics.h>
#include <ssd1306.h>
#include <ran.h>
#include <audio.h>
#include <SWO_debug.h>
#include <ruma.h>

#define NEAR_CLIPPING 3
#define CUBE_COUNT 50
#define ROOM_SIDES 8
#define ROOM_SIZE  10

typedef struct {
    uint8_t alive;
    RanVec3 pos, rot, ang_vel;
    RanVec2 vel;
}AnimCube;

static AnimCube cube_list[CUBE_COUNT];
static float camera_fov = 1.6f;
static uint32_t cube_idx = 0;
static uint8_t avg_audio_cursor = 0;
static float avg_audio[16];
static float cube_speed = 0.1;
static float cube_scale;
static uint32_t tick = 0;
static RanVec2 cube_dir;

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
static RanVec3 perc_verts[8 * 2];
static void (*graphics_func)();

static void graphics_draw_cubes();
static void graphics_draw_ruma();

void graphics_init(){
    graphics_func = graphics_draw_cubes;
    oled_init();
    avg_audio_cursor = 0;
    cube_scale = 0.1f;

    for(uint8_t i = 0; i != ROOM_SIDES; i++){
        RanVec2 p = ran_fast_polar((i / (float)ROOM_SIDES) * 2 * RAN_PI, ROOM_SIZE);
        room_verts[i] = (RanVec3){p.x, 50, p.y};
        room_verts[i + ROOM_SIDES] = (RanVec3){p.x, 17, p.y};
    }
    for(uint8_t i = 0; i != 8; i++){
        RanVec2 midpoint = (RanVec2){
            (room_verts[i].x + room_verts[(i + 1) % 8].x) / 2.0f,
            (room_verts[i].z + room_verts[(i + 1) % 8].z) / 2.0f,
        };
        perc_verts[i * 2] = (RanVec3){midpoint.x, 17, midpoint.y};
        perc_verts[i * 2 + 1] = (RanVec3){midpoint.x, 40, midpoint.y};
    }
}

static void graphics_process_cube(AnimCube *cube){
    if(!cube->alive)return;
    
    //projected vertices
    int p_verts[8][2];
    uint8_t culled_verts = 0;
    for(uint8_t i = 0; i != sizeof(cube_verts) / sizeof(RanVec3); i++){
        RanVec3 t_vert = ran_v3_scale(cube_verts[i], cube_scale);
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
    cube->pos.x -= cube->vel.x;
    cube->pos.z += cube->vel.y;
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
    if(!(tick % 20)){
        for(uint8_t i = 0; i != 3; i++){
        AnimCube *cube = cube_list + cube_idx;
        //if(rand() & 1)goto end;
        if(cube->alive)return;
        cube_idx++;
        cube_idx %= CUBE_COUNT;
        cube->alive = 1;

        RanVec2 orig_pos = ran_fast_polar(tick * 0.031 + i * (2 * RAN_PI / 3.0f), 5.0f);
        cube->pos = (RanVec3){orig_pos.x, 40, orig_pos.y};
        cube->rot = (RanVec3){0, 0, 0};
        cube->vel = ran_v2_scale(cube_dir, 0.011f);
        cube->ang_vel = (RanVec3){
            0.05f * (ran_randf() * 2.0f - 1.0f),
            0.05f * (ran_randf() * 2.0f - 1.0f),
            0.05f * (ran_randf() * 2.0f - 1.0f)
        };
        }
    }
}

//Special projection, (Make the drum pivot on its base before projecting it)
static inline RanVec2 graphics_room_proj(RanVec3 p){
    //sin and cos of 3°
    static const float s = 0.05233595624f;
    static const float c = 0.99862953475f;

    p.y -= 50;
    RanVec2 v = (RanVec2){p.x, p.y};
    p.x = ran_v2_dot(v, (RanVec2){c, s});
    p.y = ran_v2_dot(v, (RanVec2){-s, c});

    v = (RanVec2){p.x, p.z};
    p.x = ran_v2_dot(v, cube_dir);
    p.z = ran_v2_dot(v, (RanVec2){-cube_dir.y, cube_dir.x});
    p.y += 50;

    return ran_proj(camera_fov, p);
}

static void graphics_draw_cubes(){
    float perc_values[8];
    float value = audio_get_sample_vol();
    audio_get_drums(perc_values);

    avg_audio[avg_audio_cursor] = value;
    avg_audio_cursor++;
    avg_audio_cursor &= (sizeof(avg_audio) / sizeof(float) - 1);

    float f_value = 0;
    for(uint8_t i = 0; i != sizeof(avg_audio) / sizeof(float); i++){
        f_value += avg_audio[i];
    }

    for(uint8_t i = 0; i != 8; i++){
        perc_verts[i * 2 + 1].y = 17 + (45 - 17) * perc_values[i];
    }

    f_value *= (1 / (float)(sizeof(avg_audio) / sizeof(float)));
    cube_speed = 0.3f * f_value + 0.14f;
    camera_fov = (1 - value) * 0.8f + 2.1f;

    run_spawner();

    cube_scale = value + 0.3f;
    for(uint8_t i = 0; i != CUBE_COUNT; i++){
        graphics_process_cube(cube_list + i);
    }

    //Corner lines
    int8_t line_offset = 3 + 33 * value;
    oled_draw_line(128 - line_offset, 64, 128, 64 - line_offset);
    oled_draw_line(line_offset, 64, 0, 64 - line_offset);
    oled_draw_line(128 - line_offset, 0, 128, line_offset);
    oled_draw_line(line_offset, 0, 0, line_offset);

    //Room rendering
    int room_verts_ss[ROOM_SIDES * 2][2];
    int perc_verts_ss[8 * 2][2];
    float rot_ang = 0.009f*tick;
    RanVec2 R = {ran_fast_cos(rot_ang * RAN_PI), ran_fast_sin(rot_ang * RAN_PI)};
    RanVec2 S = {-R.y, R.x};
    for(uint8_t i = 0; i != ROOM_SIDES * 2; i++){
        RanVec3 p = room_verts[i];
        p = (RanVec3){
            ran_v2_dot((RanVec2){p.x, p.z}, R),
            p.y,
            ran_v2_dot((RanVec2){p.x, p.z}, S)
        };

        RanVec2 vert_p = graphics_room_proj(p);
        room_verts_ss[i][0] = vert_p.x * 32 + 64;
        room_verts_ss[i][1] = vert_p.y * 32 + 32;
    }
    for(uint8_t i = 0; i != 8 * 2; i++){
        RanVec3 p = perc_verts[i];
        p = (RanVec3){
            ran_v2_dot((RanVec2){p.x, p.z}, R),
            p.y,
            ran_v2_dot((RanVec2){p.x, p.z}, S)
        };

        RanVec2 vert_p = graphics_room_proj(p);
        perc_verts_ss[i][0] = vert_p.x * 32 + 64;
        perc_verts_ss[i][1] = vert_p.y * 32 + 32;
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

#define LINE(p1, p2) \
    oled_draw_line(perc_verts_ss[p1][0], perc_verts_ss[p1][1], perc_verts_ss[p2][0], perc_verts_ss[p2][1])
    #pragma GCC unroll 8
    for(uint8_t i = 0; i != 8; i++){
        LINE(i * 2, i * 2 + 1);
    }
#undef LINE

    oled_show();
    cube_dir = ran_fast_polar(tick * -0.01f, 1);
}

static inline void ruma_line(const RanVec3 *p_verts, const RanVec3 *n_verts, int p1, int p2, float factor){
    RanVec3 mixed_p1 = ran_v3_mix(p_verts[p1], n_verts[p1], factor);
    RanVec3 mixed_p2 = ran_v3_mix(p_verts[p2], n_verts[p2], factor);

    RanVec2 ruma_R_rot = ran_fast_polar(tick * -0.01f, 1);
    RanVec2 ruma_S_rot = {-ruma_R_rot.y, ruma_R_rot.x};
    RanVec2 temp;

    temp.x = ran_v2_dot((RanVec2){mixed_p1.x, mixed_p1.y}, ruma_R_rot);
    temp.y = ran_v2_dot((RanVec2){mixed_p1.x, mixed_p1.y}, ruma_S_rot);
    mixed_p1.x = temp.x;
    mixed_p1.y = temp.y;

    temp.x = ran_v2_dot((RanVec2){mixed_p2.x, mixed_p2.y}, ruma_R_rot);
    temp.y = ran_v2_dot((RanVec2){mixed_p2.x, mixed_p2.y}, ruma_S_rot);
    mixed_p2.x = temp.x;
    mixed_p2.y = temp.y;

    mixed_p1.y += 1.0f;
    mixed_p2.y += 1.0f;
    mixed_p1.z -= 0.4f;
    mixed_p2.z -= 0.4f;

    RanVec2 ss_p1 = ran_proj(1.9f, mixed_p1);
    RanVec2 ss_p2 = ran_proj(1.9f, mixed_p2);

    int x1 = ss_p1.x * 32.0f + 64;
    int y1 = ss_p1.y * 32.0f + 32;
    int x2 = ss_p2.x * 32.0f + 64;
    int y2 = ss_p2.y * 32.0f + 32;
    oled_draw_line(x1, y1, x2, y2);
}

static void ruma_draw_floor(){
    RanVec2 ruma_R_rot = ran_fast_polar(tick * 0.01f, 0.4f);
    RanVec2 ruma_S_rot = {-ruma_R_rot.y, ruma_R_rot.x};

    RanVec3 x_axis[2] = {
        (RanVec3){.x = ruma_R_rot.x, .y = ruma_R_rot.y },
        (RanVec3){.x = -ruma_R_rot.x, .y = -ruma_R_rot.y },
    };
    RanVec3 y_axis[2] = {
        (RanVec3){.x = ruma_S_rot.x, .y = ruma_S_rot.y },
        (RanVec3){.x = -ruma_S_rot.x, .y = -ruma_S_rot.y },
    };

    x_axis[0].y += 1.0f;
    x_axis[1].y += 1.0f;
    x_axis[0].z -= 0.4f;
    x_axis[1].z -= 0.4f;

    y_axis[0].y += 1.0f;
    y_axis[1].y += 1.0f;
    y_axis[0].z -= 0.4f;
    y_axis[1].z -= 0.4f;

    RanVec2 ss_p1 = ran_proj(1.9f, x_axis[0]);
    RanVec2 ss_p2 = ran_proj(1.9f, x_axis[1]);
    RanVec2 ss_p3 = ran_proj(1.9f, y_axis[0]);
    RanVec2 ss_p4 = ran_proj(1.9f, y_axis[1]);

    int x1 = ss_p1.x * 32.0f + 64;
    int y1 = ss_p1.y * 32.0f + 32;
    int x2 = ss_p2.x * 32.0f + 64;
    int y2 = ss_p2.y * 32.0f + 32;
    oled_draw_line_dotted(x1, y1, x2, y2);

    x1 = ss_p3.x * 32.0f + 64;
    y1 = ss_p3.y * 32.0f + 32;
    x2 = ss_p4.x * 32.0f + 64;
    y2 = ss_p4.y * 32.0f + 32;
    oled_draw_line_dotted(x1, y1, x2, y2);
}

static void graphics_draw_ruma(){
    static const int ruma_frames = (sizeof(ruma) / sizeof(RanVec3) / RUMA_TOTAL_V);

    static float acc_frame = 0.0f;
    float factor = acc_frame - (int)acc_frame;
    const RanVec3 *p_frame = ruma[(int)acc_frame];
    const RanVec3 *n_frame = ruma[((int)acc_frame + 1) % ruma_frames];

    #pragma GCC unroll 2
    for(uint8_t i = 0; i != RUMA_SPINE_V - 1; i++){
        ruma_line(p_frame, n_frame, i, i + 1, factor);
    }
    ruma_line(p_frame, n_frame, RUMA_EYES_S, RUMA_EYES_S + 1, factor);
    ruma_line(p_frame, n_frame, RUMA_EYES_S + 2, RUMA_EYES_S + 3, factor);

    #pragma GCC unroll 6
    for(uint8_t i = 0; i != RUMA_HAIR_L; i++){
        ruma_line(p_frame, n_frame, RUMA_HAIR_S + i * 2, RUMA_HAIR_S + i * 2 + 1, factor);
    }
    #pragma GCC unroll 3
    for(uint8_t i = 0; i != RUMA_ARM_V - 1; i++){
        ruma_line(p_frame, n_frame, RUMA_ARM_L_S + i, RUMA_ARM_L_S + i + 1, factor);
        ruma_line(p_frame, n_frame, RUMA_ARM_R_S + i, RUMA_ARM_R_S + i + 1, factor);
    }

    ruma_line(p_frame, n_frame, RUMA_PELVIS_V, RUMA_LEG_L_S, factor);
    ruma_line(p_frame, n_frame, RUMA_PELVIS_V, RUMA_LEG_R_S, factor);
    #pragma GCC unroll 2
    for(uint8_t i = 0; i != RUMA_LEG_V - 1; i++){
        ruma_line(p_frame, n_frame, RUMA_LEG_L_S + i, RUMA_LEG_L_S + i + 1, factor);
        ruma_line(p_frame, n_frame, RUMA_LEG_R_S + i, RUMA_LEG_R_S + i + 1, factor);
    }

    ruma_draw_floor();

    oled_show();
    acc_frame += 0.29123f;
    if(acc_frame >= (float)ruma_frames){
        acc_frame -= ruma_frames;
    }
}

void graphics_cycle_drawer(){
    static uint8_t c_drawer = 0;
    c_drawer = !c_drawer;
    tick = 0;
    switch(c_drawer){
        case 0:
            graphics_func = graphics_draw_cubes;
            break;
        case 1:
            graphics_func = graphics_draw_ruma;
            break;
    }
}

void graphics_update(){
    static uint32_t prev_tick;
    if(HAL_GetTick() - prev_tick > 15){
        graphics_func();
        tick++;
        tick %= 3141;
        prev_tick = HAL_GetTick();
    }
}

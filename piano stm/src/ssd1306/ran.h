//TOUHOU MENTIONED????!!??!!?
#ifndef RAN_H
#define RAN_H

#include <math.h>
#include <stdlib.h>
extern const float sine_table[4096];

#define RAN_PI ((float)M_PI)

typedef struct {
    float x, y;
}RanVec2;

typedef struct {
    float x, y, z;
}RanVec3;

static inline RanVec2 ran_v2_scale(RanVec2 v, float scalar){
    return (RanVec2){v.x * scalar, v.y * scalar};
}

static inline float ran_v2_dot(RanVec2 v1, RanVec2 v2){
    return v1.x * v2.x + v1.y * v2.y;
}

static inline RanVec3 ran_v3_add(RanVec3 v1, RanVec3 v2){
    return (RanVec3){v1.x + v2.x, v1.y + v2.y, v1.z + v2.z};
}

static inline RanVec3 ran_v3_sub(RanVec3 v1, RanVec3 v2){
    return (RanVec3){v1.x - v2.x, v1.y - v2.y, v1.z - v2.z};
}

static inline RanVec3 ran_v3_mul(RanVec3 v1, RanVec3 v2){
    return (RanVec3){v1.x * v2.x, v1.y * v2.y, v1.z * v2.z};
}

static inline RanVec3 ran_v3_scale(RanVec3 v, float scalar){
    return (RanVec3){v.x * scalar, v.y * scalar, v.z * scalar};
}

static inline RanVec3 ran_v3_div(RanVec3 v1, RanVec3 v2){
    return (RanVec3){v1.x / v2.x, v1.y / v2.y, v1.z / v2.z};
}

static inline float ran_fast_sin(float x){
    return sine_table[(int)(x * (4095 / 2.0f / RAN_PI)) & 0xfff];
}
static inline float ran_fast_cos(float x){
    return sine_table[((int)(x * (4095 / 2.0f / RAN_PI)) + 1024) & 0xfff];
}

//Lets try to do this without matrices (aka not much of a difference)
static inline RanVec3 ran_v3_rot(RanVec3 v, RanVec3 euler){
    float s_x = ran_fast_sin(euler.x);
    float s_y = ran_fast_sin(euler.y);
    float s_z = ran_fast_sin(euler.z);
    float c_x = ran_fast_cos(euler.x);
    float c_y = ran_fast_cos(euler.y);
    float c_z = ran_fast_cos(euler.z);

    return (RanVec3){
        .x = (v.x * c_z + v.y * s_z) * c_y + v.z * s_y,
        .y = (v.x * -s_z + v.y * c_z) * c_x +
             ((v.x * c_z + v.y * s_z) * -s_y + v.z * c_y) * s_x,
        .z = (v.x * -s_z + v.y * c_z) * -s_x +
             ((v.x * c_z + v.y * s_z) * -s_y + v.z * c_y) * c_x,
    };
}

static inline RanVec2 ran_proj(float fov, RanVec3 p){
    float depth = fov / p.y;
    return (RanVec2){p.x * depth, p.z * depth};
}

static inline float ran_randf(){ return rand() / (float)RAND_MAX; }
static inline RanVec2 ran_fast_polar(float ang, float mag){
    int index = ang * (4095 / 2.0f / RAN_PI);
    float s = sine_table[index & 0xfff];
    float c = sine_table[(index + 1024) & 0xfff];
    return ran_v2_scale((RanVec2){c, s}, mag);
}

#endif

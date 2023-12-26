#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

out vec4 finalColor;
uniform float time;
uniform float fm_factor, carrier_factor;
uniform int fm_wave, carrier_wave;

float sin_selector(in float x, int wave){
    switch(wave){
        case 0:
            return sin(x);
        case 1:
            return abs(sin(x));
        case 2:
            x = mod(x, 3.1415 * 2);
            if(x > 3.1415)return 0.0;
            return sin(x);
        case 3:
            x = mod(x, 3.1415);
            if(x > 1.5707)return 0.0;
            return sin(x);
    }
    return 0.0;
}

float func(in float x){
    float fm = sin_selector(fm_factor * 3.1415 * x, fm_wave) * (sin(time) * 0.5 + 0.7);
    return 0.8 * sin_selector(carrier_factor * 3.1415 * x + fm, carrier_wave);
}

void main(){
    float offset = time * 0.5;
    vec2 pos = (fragTexCoord - vec2(0.5)) * 2.0;
    vec2 intensity = vec2(0.05);

    //Fisheye effect taken from https://www.shadertoy.com/view/4sSSzz
    vec2 realCoordOffs;
    realCoordOffs.x = (1.0 - pos.y * pos.y) * intensity.y * (pos.x); 
    realCoordOffs.y = (1.0 - pos.x * pos.x) * intensity.x * (pos.y);

    pos = (pos - realCoordOffs);
    pos = vec2(pos.x, -pos.y);

    vec2 dot_pos = vec2(mod(time * 1.6, 2.0 * 3.1415) - 3.1415);
    dot_pos.y = func(dot_pos.x + offset);

    vec2 eq_dot = pos - dot_pos;

    float sin_v = func(pos.x + offset);

    float sine = abs(sin_v - pos.y);
    float sine_dot = dot(eq_dot, eq_dot);

    vec3 col = vec3(0.06);
    col += vec3(1.0, 1.0, 0.0) * 0.015 / sine;
    col += vec3(1.0, 1.0, 1.0) * 0.001 / sine_dot;
    col += vec3(0.7, 0.56, 0.8) * smoothstep(0.06, 0.0, abs(pos.y));
    col -= vec3(0.2) * (sin(pos.y * 15.0 * 3.1415 + time * -1.4));

    finalColor = vec4(col, 1.0);
}

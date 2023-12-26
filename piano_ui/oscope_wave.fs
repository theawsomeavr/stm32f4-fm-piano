#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

out vec4 finalColor;
uniform float time;
uniform int form;

float sin_selector(in float x, int wave){
    switch(wave){
        case 0:
            return sin(x);
        case 1:
            return abs(sin(x)) * 2.0 - 1.0;
        case 2:
            x = mod(x, 3.1415 * 2);
            if(x > 3.1415)return -1.0;
            return sin(x) * 2.0 - 1.0;
        case 3:
            x = mod(x, 3.1415);
            if(x > 1.5707)return -1.0;
            return sin(x) * 2.0 - 1.0;
    }
    return 0.0;
}

float func(in float x){
    return 0.8 * sin_selector(3.1415 * x, form);
}

void main(){
    float offset = time;
    vec2 pos = (fragTexCoord - vec2(0.5)) * 2.0;
    pos = vec2(pos.x, -pos.y);

    float sin_v = func(pos.x + offset);
    float sine = 0.1 / abs(sin_v - pos.y);

    vec3 col = vec3(0.02);
    col += vec3(0.6, 0.0, 0.8) * sine;

    finalColor = vec4(col, min(sine, 0.7));
}

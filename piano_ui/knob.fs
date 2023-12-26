#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

out vec4 finalColor;
uniform float time, hover;
uniform float value;

vec3 hsv2rgb(vec3 c){
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

vec2 line_coord(in vec2 pos, in float ang){ 
    float x = dot(pos, vec2(cos(ang), -sin(ang)));
    float y = dot(pos, vec2(sin(ang), cos(ang)));

    if(x < 0.0)return vec2(1.0);
    return vec2(x, y);
}

void main(){
    const float ang_range = (7.0/4.0 * 3.1415);
    vec2 pos = fragTexCoord - vec2(0.5);
    vec2 polar = vec2(length(pos), atan(pos.y, pos.x));
    if(polar.y < 0.0)polar.y += 3.1415 * 2.0;

    float alpha = smoothstep(0.0, -0.05, dot(pos, pos) - (0.25));
    float ang = ang_range * (1.0 - value) - 3.1415 / 2.0 + (6.2831 - ang_range) / 2.0;

    vec2 coords = line_coord(pos, ang);
    float line = smoothstep(0.04, 0.0, abs(coords.y));
    vec3 col = vec3(0.3, 0.25, 0.3);

    float fact = smoothstep(0.08, 0.03, abs(polar.x - 0.35));
    float c_ang = mod(polar.y - 3.1415 / 2.0 - (6.2831 - ang_range) / 2.0, 3.1415 * 2.0);

    float bright = min(c_ang / (ang_range * value) * 2.0, 1.0);
    float _h = min(c_ang / ang_range, 1.0);
    bright = max(bright, 0.6);

    vec3 hue = hsv2rgb(vec3(_h + sin(time * 0.8), 0.4, bright));
    hue = mix(hue, vec3(0.4), smoothstep(0, 0.1, c_ang - (ang_range * value)));

    col = mix(col, hue, fact);
    col = mix(col, vec3(coords.x * 2.0 + 0.2 + hover * 0.4), line);

    finalColor = vec4(col, alpha);
}

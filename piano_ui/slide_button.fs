#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

out vec4 finalColor;
uniform int state;

float body_fact(in vec2 pos){
    if(abs(pos.x) < 0.2){
        return 1.0;
    }
    float x = (abs(pos.x) - 0.2) * (1.0 / 0.3);
    float y = pos.y * (1.0 / 0.5);
    return smoothstep(0.1, -0.1, x*x + y*y - 1.0);
}

float circ_fact(in vec2 pos){
    float x = (pos.x + 0.2 - 0.4 * float(state)) / 0.52;
    float y = pos.y;
    return smoothstep(0.03, -0.03, x*x + y*y - 0.1);
}

void main(){
    vec2 pos = fragTexCoord - vec2(0.5);
    vec3 final_col = mix(fragColor.rgb, vec3(0.8), circ_fact(pos));

    finalColor = vec4(final_col, body_fact(pos));
}

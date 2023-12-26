#define MAZ_REP_CTX "GoycoUI report"
#include <mz_rep.h>
#include <goyco_ui.h>
#include <goyco_MIDI.h>
#include <rlgl.h>
#include <format>

//TODO: maybe create a shader struct like in my engine
static MZMesg r(MAZ_REP_CTX);
static Shader slide_switch_SHD;
static Shader oscope_SHD;
static Shader oscope_wave_SHD;
static Shader knob_SHD;
static Shader bar_SHD;
static int slide_switch_state;
static int oscope_time;
static int oscope_fm_w;
static int oscope_car_w;
static int oscope_fm_fact;
static int oscope_car_fact;
static int oscope_wave_time;
static int oscope_wave_form;
static int knob_time;
static int knob_value;
static int knob_hover;
static int bar_time;
static int bar_value;
static int bar_hover;
static uint32_t tick = 0;

void hsplit_rect(Rect basis, Rect &r1, Rect &r2, float factor){
    Vector2 size = Vector2Multiply(basis.size, {1, factor});
    r1.pos = basis.pos;
    r2.pos = Vector2Add(basis.pos, {0, size.y});
    r1.size = size;
    r2.size = {size.x, basis.size.y - size.y};
}

void vsplit_rect(Rect basis, Rect &r1, Rect &r2, float factor){
    Vector2 size = Vector2Multiply(basis.size, {factor, 1});
    r1.pos = basis.pos;
    r2.pos = Vector2Add(basis.pos, {size.x, 0});
    r1.size = size;
    r2.size = {basis.size.x - size.x, size.y};
}

void GoycoUIModule::register_module(GoycoUIModule *mod){
    mod->_parent = this;
    mod->init();
    _sub_modules.push_back(std::unique_ptr<GoycoUIModule>(mod));
}

void GoycoUIModule::render_sub_module(){
    for(auto &module : _sub_modules){
        module->render();
    }
}

void GoycoUIModule::m_input_base(Vector2 pos){
    if(pos.x < _pos.x || pos.y < _pos.y ||
       pos.x > _pos.x + _size.x || pos.y > _pos.y + _size.y){

        if(_hovering){
            _hovering = 0;
            _hover_counter = fmin(HOVER_TICK, _hover_counter);
        }

        _hover_fact = fmax(_hover_counter / (float)HOVER_TICK, 0);
        _hover_counter--;

        m_input(pos);
        for(auto &module : _sub_modules)module->m_input_base(pos);
        return;
    }

    if(!_hovering){
        _hovering = 1;
        _hover_counter = fmax(0, _hover_counter);
    }

    _hover_fact = fmin(_hover_counter / (float)HOVER_TICK, 1);
    _hover_counter++;
    m_input(pos);
    for(auto &module : _sub_modules)module->m_input_base(pos);
}

void GoycoUIModule::m_click_base(uint32_t button, bool state){
    bool prev = _mouse_b[button];
    if(state && _hovering)_mouse_b[button] = 1;
    if(!state)_mouse_b[button] = 0;
    if(prev != _mouse_b[button])m_click(button, state);
    for(auto &module : _sub_modules)module->m_click_base(button, state);
}

void GoycoUIModule::transform(GoycoUIModule &master, Vector2 center, Vector2 size, Axis only_n){
    if(only_n == Only_y)_size = {size.y * master._size.y, size.y * master._size.y};
    else if(only_n == Only_x)_size = {size.x * master._size.x, size.x * master._size.x};
    else if(only_n == Both_xy)_size = Vector2Multiply(size, master._size);

    _pos = master._pos;
    _pos = Vector2Add(_pos,
        Vector2Subtract(
            Vector2Multiply(master._size, center),
            Vector2Scale(_size, 0.5)
        )
    );
}

void GoycoUIModule::set(Vector2 pos, Vector2 size){
    _pos = pos;
    _size = size;
}

static void sysex(uint8_t id, uint8_t control, const uint8_t *data, uint32_t size){
    std::vector<uint8_t> msg = {0xf0, 'T', 'H'};
    msg.push_back(id);
    msg.push_back(control);
    int bit_count = size * 8;
    int bit_cursor = 0;

    auto next_bit = [&]() -> bool {
        int bit_idx = bit_cursor & 7;
        bool bit = (data[bit_cursor / 8] >> bit_idx) & 0x1;
        bit_cursor += 1;
        return bit;
    };

    auto get_byte_7 = [&](){
        uint8_t byte_7 = 0;
        for(int i = 0; i != 7; i++){
            byte_7 |= next_bit() << i;
            if(bit_cursor == bit_count)break;
        }
        return byte_7;
    };

    while(bit_cursor < bit_count)msg.push_back(get_byte_7());
    msg.push_back(0xf7);
    goyco_MIDI_send(MIDICmd::msg(msg));
}

//The real deal
void Knob::render(){
    float time = tick / 60.0f;
    float value = _getter(_id);

    BeginShaderMode(knob_SHD);
    SetShaderValue(knob_SHD, knob_time, &time, SHADER_UNIFORM_FLOAT);
    SetShaderValue(knob_SHD, knob_value, &value, SHADER_UNIFORM_FLOAT);
    SetShaderValue(knob_SHD, knob_hover, &_hover_fact, SHADER_UNIFORM_FLOAT);
    DrawRectangle(_pos.x, _pos.y, _size.x, _size.y, WHITE);
    EndShaderMode();
}

void Knob::set_knob_speed(float speed){
    _speed = speed;
    if(_activated){
        _prev_y = _cur_y;
        _prev_value = _getter(_id);
    }
}

void Knob::m_input(Vector2 pos){
    if(_cur_y == int(pos.y))return;
    _cur_y = pos.y;
    if(_activated){
        float new_value = _prev_value + (_prev_y - _cur_y) * 0.01 * _speed;
        new_value = fmin(new_value, 1.0f);
        new_value = fmax(new_value, 0.0f);
        _setter(new_value, _id);
    }
}

void Knob::m_click(uint32_t button, bool state){
    if(button)return;
    if(state){
        _prev_y = _cur_y;
        _prev_value = _getter(_id);
        _activated = 1;
    }
    else _activated = 0;
}

void Button::render(){
    Color c = _color;
    if(!_state)c = ColorBrightness(c, -0.5);

    if(_type == Slide){
        SetShapesTexture((Texture2D){ 1, 1, 1, 1, 7 }, (Rectangle){ 0.0f, 0.0f, 1.0f, 1.0f });
        BeginShaderMode(slide_switch_SHD);
        SetShaderValue(slide_switch_SHD, slide_switch_state, &_state, SHADER_UNIFORM_INT);
        DrawRectangle(_pos.x, _pos.y, _size.x, _size.y, ColorBrightness(c, _hover_fact * 0.25));
        EndShaderMode();
    }
    else {
        DrawRectangle(_pos.x, _pos.y, _size.x, _size.y, ColorBrightness(c, _hover_fact * 0.25));
    }
}

void Button::m_click(uint32_t button, bool state){
    if(button)return;
    if(state){
        _state = !_state;
        _callback(_arg, _state);
    }
    else if(_type == Push)_state = 0;
}


void LED::update(float intensity){
    _value = intensity;
}

void LED::render(){
    Color c = ColorBrightness(_color, _value - 0.96f);
    DrawRectangle(_pos.x, _pos.y, _size.x, _size.y, c);
}

void Text::update(std::string text){
    _text = text;
}

void Text::render(){
    //Text still kinda sucks
    DrawText(_text.c_str(), _pos.x, _pos.y, _size.x, BLACK);
}

void OScope::m_click(uint32_t button, bool state){
    int dir = 0;
    if(!state)return;
    if(button == 0)dir = 1;
    if(button == 1)dir = -1;
    _callback(dir);
}

void OScope::render(){
    float time = tick / 60.0f;
    int idx;
    VoiceSettings *settings = _ops(idx);
    int form = settings[idx].waveform;

    SetShapesTexture((Texture2D){ 1, 1, 1, 1, 7 }, (Rectangle){ 0.0f, 0.0f, 1.0f, 1.0f });
    BeginShaderMode(oscope_SHD);
    SetShaderValue(oscope_SHD, oscope_time, &time, SHADER_UNIFORM_FLOAT);
    SetShaderValue(oscope_SHD, oscope_car_w, &settings[0].waveform, SHADER_UNIFORM_INT);
    SetShaderValue(oscope_SHD, oscope_car_fact, &settings[0].multiplier, SHADER_UNIFORM_FLOAT);
    SetShaderValue(oscope_SHD, oscope_fm_w, &settings[1].waveform, SHADER_UNIFORM_INT);
    SetShaderValue(oscope_SHD, oscope_fm_fact, &settings[1].multiplier, SHADER_UNIFORM_FLOAT);
    DrawRectangle(_pos.x, _pos.y, _size.x, _size.y, WHITE);

    BeginShaderMode(oscope_wave_SHD);
    SetShaderValue(oscope_wave_SHD, oscope_wave_time, &time, SHADER_UNIFORM_FLOAT);
    SetShaderValue(oscope_wave_SHD, oscope_wave_form, &form, SHADER_UNIFORM_INT);
    Vector2 wave_size = Vector2Scale({_size.x * 1.2f, _size.x}, 0.2f + (0.05f * sin(_hover_fact * 2.5)));
    DrawRectangle(_pos.x, _pos.y + _size.y - wave_size.y, wave_size.x, wave_size.y, WHITE);

    EndShaderMode();
}

void VoicePanel::init(){
    Rect lower_rect, upper_rect;
    Rect left_upper, right_upper;
    Rect buttons_rect, activity_rect;

    hsplit_rect({{0, 0}, {1, 1}}, upper_rect, lower_rect);
    vsplit_rect(upper_rect, left_upper, right_upper);
    vsplit_rect(left_upper, buttons_rect, activity_rect, 0.75);

#define MEM(name) std::bind(&VoicePanel::name, this)
#define FUNC1(name) std::bind(&VoicePanel::name, this, std::placeholders::_1)
#define FUNC2(name) std::bind(&VoicePanel::name, this, std::placeholders::_1, std::placeholders::_2)

    for(int i = 0; i != 5; i++){
        Knob *knob = new Knob(FUNC1(get_knob), FUNC2(set_knob), i);
        Vector2 size = {1 / 6.0, 1 / 6.0};
        Vector2 center = Vector2Add(
                Vector2Multiply(lower_rect.size, {i / 5.0f + 1/10.0f, 0.5f}),
                lower_rect.pos);

        knob->transform(*this, center, size, Only_x);
        register_module(knob);
        _knobs.push_back(knob);
    }

    //FM Carrier switch
    Button *button = Button::slide(BLUE, FUNC2(change_op), 0);
    //          Rectagle view    center        size      Basis
    button->POS(activity_rect, {0.5, 0.35}, {0.9, 0.23}, Both_xy);
    register_module(button);

    _act = new LED(LIME);
    _act->POS(activity_rect, {0.5, 0.7}, {0.5, 0.5}, Only_x);
    register_module(_act);

    //Mute and Solo buttons
    button = Button::latch(SKYBLUE, FUNC2(mute_solo), 0);
    button->POS(buttons_rect, {0.25, 0.25}, {0.3, 0.3}, Only_x);
    register_module(button);

    button = Button::latch(GOLD, FUNC2(mute_solo), 1);
    button->POS(buttons_rect, {0.75, 0.25}, {0.3, 0.3}, Only_x);
    register_module(button);

    //Mutiplier buttons
    button = Button::push(PURPLE, FUNC2(change_factor), -1);
    button->POS(buttons_rect, {0.2, 0.75}, {0.15, 0.15}, Only_x);
    register_module(button);

    button = Button::push(PURPLE, FUNC2(change_factor), 1);
    button->POS(buttons_rect, {0.8, 0.75}, {0.15, 0.15}, Only_x);
    register_module(button);

    //Multiplier display
    _multi = new Text();
    _multi->POS(buttons_rect, {0.5, 0.75}, {0.2, 0.0}, Only_x);
    register_module(_multi);

    //Oscilloscope view
    OScope *scope = new OScope(FUNC1(get_voices), FUNC1(change_wave));
    scope->POS(right_upper, {0.5, 0.5}, {0.95, 0.95}, Both_xy);
    register_module(scope);

    _multi->update(std::format("{:.1f}", get_voice().multiplier));
}

void VoicePanel::set_knob_speed(float speed){
    for(auto &knob : _knobs)knob->set_knob_speed(speed);
}

VoiceSettings &VoicePanel::get_voice(){
    if(_selected)return _fm;
    return _carrier;
}

VoiceSettings *VoicePanel::get_voices(int &cur){
    cur = _selected;
    return &_carrier;
}

void VoicePanel::change_wave(int inc){
    int &wave = get_voice().waveform;
    wave += inc;
    wave &= 0x03;
    send_MIDI(6);
}

void VoicePanel::change_factor(int inc, bool unused){
    float &m = get_voice().multiplier;
    m += inc;
    if(m < 1.0f)m = 0.5f;
    else if(fmod(m, 1.0f) > 0.4)m = floor(m);
    _multi->update(std::format("{:.1f}", get_voice().multiplier));
    send_MIDI(5);
}

void VoicePanel::change_op(int unused, bool op){
    _selected = op;
    _multi->update(std::format("{:.1f}", get_voice().multiplier));
}

void VoicePanel::send_MIDI(uint8_t setting){
    uint8_t *ptr = (uint8_t *)(&get_voice().attack_max + setting);
    sysex((_channel << 1) | _selected, setting, ptr, 4);
}

void VoicePanel::update_MIDI(){
    int prev = _selected;
    for(int i = 0; i != 7; i++){
        _selected = 0;
        send_MIDI(i);
        _selected = 1;
        send_MIDI(i);
    }
    _selected = prev;
    _multi->update(std::format("{:.1f}", get_voice().multiplier));
}

void yank_queue();

void VoicePanel::set_value(int op, int idx, float value){
    VoiceSettings *v = &_carrier;
    switch(idx){
        case 0 ... 5:
            (&v[op].attack_max)[idx] = value;
            break;
        case 6:
            v[op].waveform = value;
            break;
    }
}

float VoicePanel::get_value(int op, int idx){
    VoiceSettings *v = &_carrier;
    switch(idx){
        case 0 ... 5: return (&v[op].attack_max)[idx];
        case 6: return v[op].waveform;
    }
    return 0;
}

float VoicePanel::get_knob(int idx){
    float *v = &get_voice().attack_max;
    return v[idx];
}

void VoicePanel::set_knob(float value, int idx){
    float *v = &get_voice().attack_max;
    v[idx] = value;
    send_MIDI(idx);
}

void VoicePanel::mute_solo(int button, bool state){
    goyco_MIDI_mute_solo(_channel, button, state);
}

void VoicePanel::put(VoicePanel *p){
    p->_carrier = _carrier;
    p->_fm = _fm;
    p->update_MIDI();
}

void VoicePanel::render(){
    if(!_is_yanking)DrawRectangle(_pos.x, _pos.y, _size.x, _size.y, LIGHTGRAY);
    else {
        DrawRectangle(_pos.x, _pos.y, _size.x, _size.y, ColorBrightness(RAYWHITE, cosf(tick * 0.06f) * 0.3 - 0.2));
    }
    DrawLine(_pos.x + _size.x / 2, _pos.y, _pos.x + _size.x / 2, _pos.y + _size.y / 2, BLACK);
    DrawLine(_pos.x, _pos.y + _size.y / 2, _pos.x + _size.x, _pos.y + _size.y / 2, BLACK);
    int notes = goyco_MIDI_get_notes(_channel);
    _act->update(fmin(notes / 2.0f, 1.2f));
    render_sub_module();
}

void PercussionPanel::init(){
    Rect lower_rect, upper_rect;
    Rect buttons_rect, activity_rect;

    hsplit_rect({{0, 0}, {1, 1}}, upper_rect, lower_rect);
    vsplit_rect(upper_rect, buttons_rect, activity_rect);
    //vsplit_rect(left_upper, buttons_rect, activity_rect, 0.75);

#undef MEM
#undef FUNC1
#undef FUNC2
#define MEM(name) std::bind(&PercussionPanel::name, this)
#define FUNC1(name) std::bind(&PercussionPanel::name, this, std::placeholders::_1)
#define FUNC2(name) std::bind(&PercussionPanel::name, this, std::placeholders::_1, std::placeholders::_2)

    for(int i = 0; i != 1; i++){
        Knob *knob = new Knob(FUNC1(get_knob), FUNC2(set_knob), i);
        Vector2 size = {1 / 6.0, 1 / 6.0};
        Vector2 center = Vector2Add(
                Vector2Multiply(lower_rect.size, {0.5, 0.5f}),
                lower_rect.pos);

        knob->transform(*this, center, size, Only_x);
        register_module(knob);
        _knobs.push_back(knob);
    }

    _act = new LED(LIME);
    _act->POS(activity_rect, {0.5, 0.5}, {0.2, 0.2}, Only_x);
    register_module(_act);

    //Mute and Solo buttons
    Button *button = Button::latch(SKYBLUE, FUNC2(mute_solo), 0);
    button->POS(buttons_rect, {0.25, 0.5}, {0.3, 0.3}, Only_x);
    register_module(button);

    button = Button::latch(GOLD, FUNC2(mute_solo), 1);
    button->POS(buttons_rect, {0.75, 0.5}, {0.3, 0.3}, Only_x);
    register_module(button);
}

void PercussionPanel::render(){
    DrawRectangle(_pos.x, _pos.y, _size.x, _size.y, LIGHTGRAY);
    DrawLine(_pos.x + _size.x / 2, _pos.y, _pos.x + _size.x / 2, _pos.y + _size.y / 2, BLACK);
    DrawLine(_pos.x, _pos.y + _size.y / 2, _pos.x + _size.x, _pos.y + _size.y / 2, BLACK);
    int notes = goyco_MIDI_get_notes(9);
    _act->update(fmin(notes / 2.0f, 1.2f));
    render_sub_module();
}

void PercussionPanel::set_knob_speed(float speed){
    for(auto &knob : _knobs)knob->set_knob_speed(speed);
}

void PercussionPanel::update_MIDI(){
    send_MIDI(0);
}

void PercussionPanel::set_value(int idx, float value){
    if(idx == 0)_volume = value;
}

float PercussionPanel::get_value(int idx){
    if(idx == 0)return _volume;
    return 0;
}

void PercussionPanel::send_MIDI(uint8_t setting){
    uint8_t *ptr = (uint8_t *)(&_volume);
    sysex((9 << 1), setting, ptr, 4);
}

void PercussionPanel::set_knob(float value, int idx){
    if(idx == 0)_volume = value;
    send_MIDI(idx);
}

float PercussionPanel::get_knob(int idx){
    if(idx == 0)return _volume;
    return 0;
}

void PercussionPanel::mute_solo(int button, bool state){
    goyco_MIDI_mute_solo(9, button, state);
}

void ProgressBar::render(){
    float time = tick / 60.0f;
    float value = goyco_MIDI_get_pos();

    BeginShaderMode(bar_SHD);
    SetShaderValue(bar_SHD, bar_time, &time, SHADER_UNIFORM_FLOAT);
    SetShaderValue(bar_SHD, bar_value, &value, SHADER_UNIFORM_FLOAT);
    SetShaderValue(bar_SHD, bar_hover, &_hover_fact, SHADER_UNIFORM_FLOAT);
    DrawRectangle(_pos.x, _pos.y, _size.x, _size.y, WHITE);
    EndShaderMode();
}

void ProgressBar::m_input(Vector2 pos){
    _x_pos = (pos.x - _pos.x) / _size.x;
    _x_pos = fmin(_x_pos, 1.0);
    _x_pos = fmax(_x_pos, 0.0);
}

void ProgressBar::m_click(uint32_t button, bool state){
    if(button)return;
    if(!state)return;
    goyco_MIDI_seek(_x_pos);
}

void goycoui_init(){
    slide_switch_SHD = LoadShader(0, "slide_button.fs");
    oscope_SHD = LoadShader(0, "oscope.fs");
    oscope_wave_SHD = LoadShader(0, "oscope_wave.fs");
    knob_SHD = LoadShader(0, "knob.fs");
    bar_SHD = LoadShader(0, "bar.fs");

    slide_switch_state = GetShaderLocation(slide_switch_SHD, "state");
    oscope_time = GetShaderLocation(oscope_SHD, "time");
    oscope_fm_w = GetShaderLocation(oscope_SHD, "fm_wave");
    oscope_car_w = GetShaderLocation(oscope_SHD, "carrier_wave");
    oscope_fm_fact = GetShaderLocation(oscope_SHD, "fm_factor");
    oscope_car_fact = GetShaderLocation(oscope_SHD, "carrier_factor");
    oscope_wave_time = GetShaderLocation(oscope_wave_SHD, "time");
    oscope_wave_form = GetShaderLocation(oscope_wave_SHD, "form");
    knob_time = GetShaderLocation(knob_SHD, "time");
    knob_value = GetShaderLocation(knob_SHD, "value");
    knob_hover = GetShaderLocation(knob_SHD, "hover");
    bar_time = GetShaderLocation(bar_SHD, "time");
    bar_value = GetShaderLocation(bar_SHD, "value");
    bar_hover = GetShaderLocation(bar_SHD, "hover");
}

void goycoui_update(){
    tick++;
}

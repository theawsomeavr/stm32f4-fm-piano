#ifndef GOYCO_UI_H
#define GOYCO_UI_H

#include <cstdint>
#include <raylib.h>
#include <raymath.h>
#include <string>
#include <vector>
#include <memory>
#include <functional>

#define HOVER_TICK 15
#define POS(rect, pos_x, pos_y, size_x, size_y, axis) transform(*this, \
            Vector2Add(rect.pos, Vector2Multiply(rect.size, pos_x, pos_y)), \
            Vector2Multiply(rect.size, size_x, size_y), axis)

enum Axis {
    Only_x,
    Only_y,
    Both_xy,
};

struct VoiceSettings {
    float attack_max;
    float attack;
    float decay;
    float sustain;
    float release;
    float multiplier;
    int waveform;

    VoiceSettings(){
        attack_max = 1;
        attack = 0;
        decay = 0;
        sustain = 1;
        release = 0;
        multiplier = 1;
        waveform = 0;
    }
};

struct Rect {
    Vector2 pos, size;
};

class GoycoUIModule {
public:
    virtual void init(){}
    virtual void m_input(Vector2 pos){}
    virtual void m_click(uint32_t button, bool state){}
    virtual void k_input(){}
    virtual void render(){ render_sub_module(); }
    void m_input_base(Vector2 pos);
    void m_click_base(uint32_t button, bool state);
    void register_module(GoycoUIModule *mod);
    void render_sub_module();
    void transform(GoycoUIModule &master, Vector2 center, Vector2 size, Axis only_n = Only_y);
    void set(Vector2 pos, Vector2 size);
    bool is_hovering(){ return _hovering; }

protected:
    Vector2 _pos;
    Vector2 _size;
    float _hover_fact;
    bool _hovering = false;
    int _hover_counter = 0;
    bool _mouse_b[3];

    const void *_parent = nullptr;
    std::vector<std::unique_ptr<GoycoUIModule>> _sub_modules;
};

class Knob : public GoycoUIModule {
public:
    Knob(std::function<float(int)> getter, std::function<void(float, int)> setter, int id){
        _id = id;
        _getter = getter;
        _setter = setter;
    }

    void render() override;
    void m_click(uint32_t button, bool state) override;
    void m_input(Vector2 pos) override;
    void set_knob_speed(float speed);

protected:
    int _id;
    int _prev_y;
    int _cur_y;
    bool _activated = 0;
    float _prev_value;
    float _speed = 1;
    std::function<float(int)> _getter;
    std::function<void(float, int)> _setter;
};

class Button : public GoycoUIModule {
    enum Type {
        Latch,
        Slide,
        Push,
    };
public:
    Button(Color c, std::function<void(int, bool)> callback, int arg){
        _color = c;
        _callback = callback;
        _arg = arg;
    }

    static Button *latch(Color c, std::function<void(int, bool)> callback, int arg){
        Button *item = new Button(c, callback, arg);
        item->_type = Latch;
        return item;
    }

    static Button *slide(Color c, std::function<void(int, bool)> callback, int arg){
        Button *item = new Button(c, callback, arg);
        item->_type = Slide;
        return item;
    }

    static Button *push(Color c, std::function<void(int, bool)> callback, int arg){
        Button *item = new Button(c, callback, arg);
        item->_type = Push;
        return item;
    }

    void render() override;
    void m_click(uint32_t button, bool state) override;

protected:
    std::function<void(int, bool)> _callback;
    int _arg;
    Color _color;
    Type _type;
    int _state = 0;
};

class LED : public GoycoUIModule {
public:
    LED(Color c){
        _color = c;
    }
    
    void render() override;
    void update(float intensity);
protected:
    Color _color;
    float _value = 0;
};

class OScope : public GoycoUIModule {
public:
    OScope(std::function<VoiceSettings *(int &)> getter, std::function<void(int)> setter){
        _callback = setter;
        _ops = getter;
    }

    void render() override;
    void m_click(uint32_t button, bool state) override;
    
protected:
    std::function<VoiceSettings *(int &)> _ops;
    std::function<void(int)> _callback;
};

class Text : public GoycoUIModule {
public:
    void render() override;
    void update(std::string text);

protected:
    std::string _text;
};

class ProgressBar : public GoycoUIModule {
public:
    void render() override;
    void m_input(Vector2 pos) override;
    void m_click(uint32_t button, bool state) override;
protected:
    float _x_pos;
};

class VoicePanel : public GoycoUIModule {
public:
    VoicePanel(int channel){
        _channel = channel;
    }

    void init() override;
    void render() override;
    void update_MIDI();
    void set_value(int op, int idx, float value);
    float get_value(int op, int idx);
    void set_knob_speed(float speed);
    void yank_queue(bool state){ _is_yanking = state; }
    void put(VoicePanel *p);
protected:
    void send_MIDI(uint8_t setting);
    void change_factor(int inc, bool unused);
    void change_wave(int inc);
    void change_op(int unused, bool op);
    void set_knob(float value, int idx);
    void mute_solo(int button, bool state);
    float get_knob(int idx);
    VoiceSettings &get_voice();
    VoiceSettings *get_voices(int &cur);
protected:
    std::vector<Knob *> _knobs;
    bool _is_yanking = 0;
    int _selected = 0;
    int _channel;
    VoiceSettings _carrier, _fm;
    Text *_multi;
    LED *_act;
};

class PercussionPanel : public GoycoUIModule {
public:
    void init() override;
    void render() override;
    void update_MIDI();
    void set_value(int idx, float value);
    float get_value(int idx);
    void set_knob_speed(float speed);
protected:
    void send_MIDI(uint8_t setting);
    void set_knob(float value, int idx);
    float get_knob(int idx);
    void mute_solo(int button, bool state);
protected:
    std::vector<Knob *> _knobs;
    LED *_act;
    float _volume;
};

void goycoui_init();
void goycoui_update();
void hsplit_rect(Rect basis, Rect &r1, Rect &r2, float factor = 0.5f);
void vsplit_rect(Rect basis, Rect &r1, Rect &r2, float factor = 0.5f);

#endif

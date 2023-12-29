#define MAZ_REP_CTX "Main Report"
#include <mz_rep.h>
#include <cstdint>
#include <mutex>
#include <goyco_ui.h>
#include <goyco_MIDI.h>
#include <filesystem>
#include <save.h>

#define SONG_CONFIG_PATH "songs"

namespace fs = std::filesystem;
static MZMesg r(MAZ_REP_CTX);
static std::mutex connection_mutex;
static bool new_connection = 0;
static std::string perc_keys[] = {
    "volume",
};
static std::string voice_keys[] = {
    "attack_max",
    "attack",
    "decay",
    "sustain",
    "release",
    "multiplier",
    "waveform"
};
const char *usage = R"(-E)";

class SynthRack : public GoycoUIModule {
public:
    void init() override {
#define COUNT 16
#define COUNT_SQRT 4
        Rect panels, bar;
        hsplit_rect({.pos = {0, 0}, .size = {1, 1}}, panels, bar, 0.94);

        for(uint8_t i = 0; i != COUNT; i++){
            if(i == 9){
                _perc_panel = new PercussionPanel();
                _perc_panel->POS(panels,
                           {(i % COUNT_SQRT) / float(COUNT_SQRT) + (0.5f / float(COUNT_SQRT)),
                            (i / COUNT_SQRT) / float(COUNT_SQRT) + (0.5f / float(COUNT_SQRT))},
                           {0.24, 0.24}, Both_xy);
                register_module(_perc_panel);
                continue;
            }
            VoicePanel *panel = new VoicePanel(i);

            panel->POS(panels,
                       {(i % COUNT_SQRT) / float(COUNT_SQRT) + (0.5f / float(COUNT_SQRT)),
                        (i / COUNT_SQRT) / float(COUNT_SQRT) + (0.5f / float(COUNT_SQRT))},
                       {0.24, 0.24}, Both_xy);
            _panels.push_back(panel);
            register_module(panel);
        }

        ProgressBar *prog = new ProgressBar();
        prog->POS(bar, {0.5, 0.5}, {0.9, 0.7}, Both_xy);
        register_module(prog);
    }

    void update_MIDI(){
        for(auto &p : _panels){
            p->update_MIDI();
        }
        _perc_panel->update_MIDI();
    }

    //vim btw
    void yank(){
        if(_yank_idx != -1)_panels[_yank_idx]->yank_queue(0);
        for(_yank_idx = 0; _yank_idx != _panels.size(); _yank_idx++){
            if(_panels[_yank_idx]->is_hovering()){
                _panels[_yank_idx]->yank_queue(1);
                return;
            }
        }
        _yank_idx = -1;
    }
    
    void put(){
        if(_yank_idx == -1)return;
        for(int i = 0; i != _panels.size(); i++){
            if(_panels[i]->is_hovering()){
                _panels[_yank_idx]->yank_queue(0);
                _panels[_yank_idx]->put(_panels[i]);
                return;
            }
        }
    }

    void set_value(int channel, std::string &key, std::string &value){
        if(channel == 9){
            float v;
            if(sscanf(value.c_str(), "%f", &v) != 1)return;
            for(int i = 0; i != 1; i++){
                if(perc_keys[i] == key){
                    _perc_panel->set_value(i, v);
                    break;
                }
            }
            return;
        }
        if(channel > 9)channel--;
        float car_v, fm_v;
        if(sscanf(value.c_str(), "%f %f", &car_v, &fm_v) != 2)return;
        for(int i = 0; i != 7; i++){
            if(voice_keys[i] == key){
                _panels[channel]->set_value(0, i, car_v);
                _panels[channel]->set_value(1, i, fm_v);
                break;
            }
        }
    }

    std::string get_value(int channel){
        std::string keys;
        if(channel == 9){
            for(int i = 0; i != 1; i++){
                keys += "    ";
                keys += perc_keys[i];
                keys += std::string(12 - perc_keys[i].size(), ' ');
                keys += std::to_string(_perc_panel->get_value(i));
                keys += "\n";
            }
            return keys;
        }
        if(channel > 9)channel--;
        for(int i = 0; i != 7; i++){
            keys += "    ";
            keys += voice_keys[i];
            keys += std::string(12 - voice_keys[i].size(), ' ');
            keys += std::to_string(_panels[channel]->get_value(0, i));
            keys += " ";
            keys += std::to_string(_panels[channel]->get_value(1, i));
            keys += "\n";
        }
        return keys;
    }

    void set_knob_speed(float speed){
        for(auto &panel : _panels)panel->set_knob_speed(speed);
    }
protected:
    std::vector<VoicePanel *> _panels;
    PercussionPanel *_perc_panel;
    int _yank_idx = -1;
};

int main(const int argc, const char **argv) {
    std::unique_lock<std::mutex> lk(connection_mutex);
    lk.unlock();
    SynthRack rack;
    bool mouse_buttons[3];
    bool prev_mouse_buttons[3] = {0, 0, 0};
    rack.set({0, 0}, {1600, 900});
    rack.init();

    auto deserial_cb = [&](std::string &name, std::string &key, std::string &value){
        if(name.find("Channel ") == std::string::npos)return;
        int idx;
        try {
            idx = std::stoi(std::string(name.c_str() + 8));
        } catch(...){ return; }
        idx -= 1;
        if(idx < 0 || idx >= 16)return;
        rack.set_value(idx, key, value);
    };

    auto serial_cb = [&](int channel) -> std::string { return rack.get_value(channel); };
    auto song_config = [](std::string song_path) -> std::string {
        fs::path song(song_path);
        return SONG_CONFIG_PATH +
            std::string(1, fs::path::preferred_separator) +
            song.replace_extension("keik").filename().string();
    };

    deserialize("settings.keik", deserial_cb);
    InitWindow(1600, 900, "Goyco UI");
    goycoui_init();

    goyco_MIDI_init([&](){
        lk.lock();
        new_connection = 1;
        lk.unlock();
    });
    SetTargetFPS(60);

    while(!WindowShouldClose()){
        if(IsFileDropped()){
            FilePathList files = LoadDroppedFiles();
            if(files.count == 1){
                goyco_MIDI_send(MIDICmd::request_file(files.paths[0]));
                if(fs::exists(SONG_CONFIG_PATH)){
                    if(deserialize(song_config(files.paths[0]), deserial_cb)){
                        rack.update_MIDI();
                    } else {
                        deserialize("settings.keik", deserial_cb);
                    }
                }
            }
            UnloadDroppedFiles(files);    // Unload filepaths from memory
        }
        Vector2 m_pos = GetMousePosition();
        mouse_buttons[0] = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
        mouse_buttons[1] = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
        mouse_buttons[2] = IsMouseButtonDown(MOUSE_BUTTON_MIDDLE);

        rack.m_input_base(m_pos);
        for(int i = 0; i != 3; i++){
            if(prev_mouse_buttons[i] != mouse_buttons[i]){
                prev_mouse_buttons[i] = mouse_buttons[i];
                rack.m_click_base(i, mouse_buttons[i]);

                if(!i && !mouse_buttons[0]){
                    serialize("settings.keik", serial_cb);
                }
            }
        }

        //Player buttons
        if(IsKeyPressed(KEY_LEFT))goyco_MIDI_rseek(-5000);
        if(IsKeyPressed(KEY_RIGHT))goyco_MIDI_rseek(5000);
        if(IsKeyPressed(KEY_SPACE))goyco_MIDI_pause();

        if(IsKeyPressed(KEY_R))goyco_MIDI_send(MIDICmd::msg({0xf0, 'R', 'M', 0xf7}));
        if(IsKeyPressed(KEY_Y))rack.yank();
        if(IsKeyPressed(KEY_P)){
            rack.put();
            serialize("settings.keik", serial_cb);
        }
        if(IsKeyPressed(KEY_LEFT_SHIFT))rack.set_knob_speed(0.1);
        if(IsKeyReleased(KEY_LEFT_SHIFT))rack.set_knob_speed(1);
        //Save song confing
        if(IsKeyPressed(KEY_S)){
            std::string song_path;
            if(goyco_MIDI_fpath(song_path)){
                if(!fs::exists(SONG_CONFIG_PATH)){
                    fs::create_directory(SONG_CONFIG_PATH);
                }
                serialize(song_config(song_path), serial_cb);
            }
        }

        BeginDrawing();
            ClearBackground({58, 59, 49, 255});
            rack.render();
        EndDrawing();
        goycoui_update();
        lk.lock();
        if(new_connection){
            rack.update_MIDI();
            new_connection = 0;
        }
        lk.unlock();
    }

    goyco_MIDI_stop();
    CloseWindow();
    return 0;
}

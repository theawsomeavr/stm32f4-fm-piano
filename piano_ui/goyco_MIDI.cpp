#define MAZ_REP_CTX "Goyco MIDI report"
#include <mz_rep.h>
#include <goyco_MIDI.h>
#include <midi_parsee.h>
#include <RtMidi.h>
#include <thread>
#include <chrono>
#include <deque>
#include <cmath>
#include <cstring>
#include <condition_variable>
#include "waiter.h"

//chrono, not this time
using namespace std::chrono;

#define MIDI_NAME "Skrafty piano"

enum MIDIState {
    Scanning,
    Running,
};

enum MIDIPlayerState {
    Idle,
    Playing,
    LoadSong,
    Seek,
    Quit
};

struct MIDIMuteSolo {
    bool mute, solo;
    MIDIMuteSolo(){
        mute = 0;
        solo = 0;
    }
};

static MZMesg r(MAZ_REP_CTX);
static RtMidiOut MIDI_dev;
static MIDIState device_state = Scanning;
static std::mutex MIDI_queue_mutex, MIDI_notes_mutex;
static std::deque<MIDICmd> MIDI_queue;
static std::thread MIDI_thread;
static std::condition_variable MIDI_event;
static bool MIDI_done = 0;
static float player_req_pos, player_cur_pos;
static MIDIPlayerState player_state = Idle;
static std::thread player_thread;
static std::string player_song;
static Waiter player_lock;
static int MIDI_notes[16];
static MIDIMuteSolo MIDI_channel_setting[16];
static std::function<void()> connection_callback;
static MIDIParsee MIDI_file;

MIDICmd MIDICmd::request_file(std::string name){
    MIDICmd msg;
    msg.type = MIDICmd::File;
    msg.data.insert(msg.data.end(), name.begin(), name.end());
    return msg;
}

MIDICmd MIDICmd::msg(std::vector<uint8_t> _msg){
    MIDICmd msg;
    msg.type = MIDICmd::MIDI;
    msg.data.insert(msg.data.end(), _msg.begin(), _msg.end());
    return msg;
}

static void goyco_MIDI_mute(){
    memset(MIDI_notes, 0, sizeof(MIDI_notes));
    for(int i = 0; i != 16; i++){
        std::vector<uint8_t> data = {0xb0, 120, 0};
        data[0] |= i;
        goyco_MIDI_send(MIDICmd::msg(data));
    }
}

static int find_device(){
    int dev_count = MIDI_dev.getPortCount();
    for(int i = 0; i != dev_count; i++){
        std::string name = MIDI_dev.getPortName(i);
        if(name.find(MIDI_NAME) != std::string::npos){
            return i;
        }
    }
    return -1;
}

static bool goyco_MIDI_scanner(){
    int dev_idx = find_device();
    if(dev_idx != -1){
        try{
            MIDI_dev.openPort(dev_idx);
            device_state = Running;
            player_lock.lock();
            if(MIDI_file.is_open())player_state = Playing;
            player_lock.unlock();
            connection_callback();
            return 1;
        } catch(RtMidiError &e){
            MZErr_(e.what()).p();
            std::unique_lock<std::mutex> lk(MIDI_queue_mutex);
            if(MIDI_event.wait_for(lk, milliseconds(1000), [](){ return MIDI_done; })){
                return 0;
            }
        }
    }

    std::unique_lock<std::mutex> lk(MIDI_queue_mutex);
    if(MIDI_event.wait_for(lk, milliseconds(100), [](){ return MIDI_done; })){
        return 0;
    }

    return 1;
}

static void goyco_MIDI_handle_cmd(MIDICmd &cmd){
    std::unique_lock<std::mutex> lk(MIDI_notes_mutex);
    uint8_t channel = cmd.data[0] & 0x0f;
    uint8_t command = cmd.data[0] & 0xf0;
    switch(command){
        case 0x90:
            if(!cmd.data[2]){
                MIDI_notes[channel]--;
                if(MIDI_notes[channel] < 0)MIDI_notes[channel] = 0;
            }
            else {
                MIDI_notes[channel]++;
            }
            break;
        case 0x80:
            MIDI_notes[channel]--;
            if(MIDI_notes[channel] < 0)MIDI_notes[channel] = 0;
            break;
        case 0xB0:
            if(cmd.data[1] == 120 || cmd.data[1] == 123){
                MIDI_notes[channel] = 0;
            }
            break;
    }
    lk.unlock();

    MIDIMuteSolo &b = MIDI_channel_setting[channel];
    bool is_solo = 0;
    if(command == 0xB0)goto send;
    if(command == 0xF0)goto send;
    if(b.mute)return;
    for(int i = 0; i != 16; i++){
        if(MIDI_channel_setting[i].solo){
            is_solo = 1;
            break;
        }
    }
    if(is_solo && !b.solo)return;

send:
    try {
        MIDI_dev.sendMessage(&cmd.data);
    } catch(RtMidiError &e){
        MZErr_(e.what());
    }
}

static void goyco_MIDI_process_cmd(MIDICmd &cmd){
    switch(cmd.type){
        case MIDICmd::MIDI:
            goyco_MIDI_handle_cmd(cmd);
            break;
        case MIDICmd::File: {
            std::string name((const char *)cmd.data.data(), cmd.data.size());
            player_lock.lock();
            player_song = name;
            player_state = LoadSong;
            player_lock.unlock();
            player_lock.cancel();
            break;
        }
    }
}

static bool goyco_MIDI_process_queue(){
    std::unique_lock<std::mutex> lk(MIDI_queue_mutex);
    while(1){
        int dev_idx = find_device();
        if(dev_idx == -1){
            memset(MIDI_notes, 0, sizeof(MIDI_notes));
            MIDI_queue.clear();
            device_state = Scanning;
            MIDI_dev.closePort();
            player_lock.lock();
            player_state = Idle;
            player_lock.unlock();
            return 1;
        }

        bool res = MIDI_event.wait_for(lk, milliseconds(100), [](){ return !MIDI_queue.empty() | MIDI_done; });
        if(res == 1)break;
    }

    while(MIDI_queue.size()){
        MIDICmd &msg = MIDI_queue.front();
        goyco_MIDI_process_cmd(msg);
        MIDI_queue.pop_front();
    }
    if(MIDI_done)return 0;
    return 1;
}

static void goyco_MIDI_loop(){
    while(1){
        switch(device_state){
            case Scanning:
                if(!goyco_MIDI_scanner())return;
                break;
            case Running:
                if(!goyco_MIDI_process_queue())return;
                break;
        }
    }
}

static int process_player_state(uint32_t millis){
    static MIDIPlayerState state = Idle;
    MIDIPlayerState prev_state = state;
    float pos;
    player_lock.wait_for(millis);
    player_lock.lock();
    state = player_state;
    pos = player_req_pos;
    player_lock.unlock();

    switch(state){
        case Idle:
            if(prev_state == Playing)goyco_MIDI_mute();
            return 2;
        case LoadSong:
            goyco_MIDI_mute();
            try {
                MIDI_file.close();
                MIDI_file.open(player_song);
            } catch(MZStream &e){
                e.p();
                player_lock.lock();
                player_state = Idle;
                player_cur_pos = 0;
                player_lock.unlock();
                return 2;
            }
            player_lock.lock();
            player_state = Playing;
            player_cur_pos = 0;
            player_lock.unlock();
            return 2;
        case Playing: break;
        case Seek: {
            if(!MIDI_file.is_open())break;
            MIDI_file.rewind();
            int len = MIDI_file.song_len() * pos;
            if(len){
                MIDIEvent ev;
                while(MIDI_file.get_event(ev)){
                    if(ev.is_delay()){
                        len -= ev.ticks;
                        if(len <= 0)break;
                    }
                }
            }
            goyco_MIDI_mute();
            player_lock.lock();
            player_state = Playing;
            player_lock.unlock();
            return 1;
        }
        case Quit: return 0;
    }
    return 1;
}

static void goyco_MIDI_player(){
    while(1){
scan:
        switch(process_player_state(100)){
            case 0: return;
            case 1: break;
            case 2: goto scan;
        }

        MIDIEvent ev;
        while(MIDI_file.get_event(ev)){
            if(ev.is_delay()){
                switch(process_player_state(ev.ticks)){
                    case 0: return;
                    case 1: break;
                    case 2: goto scan;
                }
                player_lock.lock();
                player_cur_pos = MIDI_file.song_pos_f();
                player_lock.unlock();
                continue;
            }

            if((ev.cmd & 0xF0) == 0xC0 ||
               (ev.cmd & 0xF0) == 0xD0)goyco_MIDI_send(MIDICmd::msg({ev.cmd, ev.data1}));
            else goyco_MIDI_send(MIDICmd::msg({ev.cmd, ev.data1, ev.data2}));
        }
    }
}

void goyco_MIDI_send(MIDICmd msg){
    std::unique_lock<std::mutex> lk(MIDI_queue_mutex);
    if(device_state == Scanning && msg.MIDI){
        MZWarn("No device is connected");
        return;
    }
    MIDI_queue.push_back(msg);
    MIDI_event.notify_all();
}

void goyco_MIDI_init(std::function<void()> callback){
    connection_callback = callback;
    player_lock.init();
    memset(MIDI_notes, 0, sizeof(MIDI_notes));
    MIDI_thread = std::thread(goyco_MIDI_loop);
    player_thread = std::thread(goyco_MIDI_player);
}

void goyco_MIDI_stop(){
    goyco_MIDI_mute();
    std::unique_lock<std::mutex> lk (MIDI_queue_mutex);
    MIDI_done = 1;
    lk.unlock();
    MIDI_event.notify_all();

    player_lock.lock();
    player_state = Quit;
    player_lock.unlock();
    player_lock.cancel();

    if(MIDI_thread.joinable())MIDI_thread.join();
    if(player_thread.joinable())player_thread.join();
    player_lock.end();
}

void goyco_MIDI_seek(float pos){
    player_lock.lock();
    if(!MIDI_file.is_open()){
        player_lock.unlock();
        return;
    }
    player_req_pos = pos;
    player_state = Seek;
    player_lock.unlock();
    player_lock.cancel();
}

float goyco_MIDI_get_pos(){
    float pos;
    player_lock.lock();
    pos = player_cur_pos;
    player_lock.unlock();
    return pos;
}

int goyco_MIDI_get_notes(uint8_t channel){
    std::unique_lock<std::mutex> lk(MIDI_notes_mutex);
    return MIDI_notes[channel];
}

void goyco_MIDI_rseek(int millis){
    player_lock.lock();
    if(!MIDI_file.is_open()){
        player_lock.unlock();
        return;
    }
    player_req_pos = player_cur_pos + (1 / (float)MIDI_file.song_len()) * millis;
    player_req_pos = fmin(player_req_pos, 1.0f);
    player_req_pos = fmax(player_req_pos, 0.0f);
    player_state = Seek;
    player_lock.unlock();
    player_lock.cancel();
}

void goyco_MIDI_pause(){
    player_lock.lock();
    if(!MIDI_file.is_open()){
        player_lock.unlock();
        return;
    }
    if(player_state == Playing)player_state = Idle;
    else if(player_state == Idle)player_state = Playing;
    player_lock.unlock();
    player_lock.cancel();
}

bool goyco_MIDI_fpath(std::string &name){
    player_lock.lock();
    if(!MIDI_file.is_open()){
        player_lock.unlock();
        return 0;
    }
    name = player_song;
    player_lock.unlock();
    return 1;
}

void goyco_MIDI_mute_solo(uint8_t channel, int button, bool state){
    std::unique_lock<std::mutex> lk(MIDI_notes_mutex);
    MIDIMuteSolo &b = MIDI_channel_setting[channel];
    bool is_solo = 0;
    switch(button){
        case 0: {
            b.mute = state;
            std::vector<uint8_t> data = {0xb0, 120, 0};
            data[0] |= channel;
            MIDI_queue.push_back(MIDICmd::msg(data));
            MIDI_event.notify_all();
            break;
        }
        case 1:
            b.solo = state;
            for(int i = 0; i != 16; i++){
                if(MIDI_channel_setting[i].solo){
                    is_solo = 1;
                    break;
                }
            }
            if(!is_solo)return;
            for(int i = 0; i != 16; i++){
                if(MIDI_channel_setting[i].solo)continue;
                std::vector<uint8_t> data = {0xb0, 120, 0};
                data[0] |= i;
                MIDI_queue.push_back(MIDICmd::msg(data));
                MIDI_event.notify_all();
            }
            break;
    }
}

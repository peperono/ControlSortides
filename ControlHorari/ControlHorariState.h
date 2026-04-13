#pragma once
#include <atomic>
#include <mutex>
#include <string>

struct ControlHorariState {
    std::mutex        mtx;
    std::string       horariJson;
    std::atomic<bool> horariLoadPending{false};
};

extern ControlHorariState ch_state;

#pragma once
#include "Rellotge/RellotgeState.h"
#include <atomic>
#include <mutex>
#include <string>
#include <vector>
#include <cstdio>

struct LogEntry {
    std::string time;    // "HH:MM:SS" simulat
    std::string src;     // "ControlHorari" | "ControlRemot" | "HttpServer"
    std::string sig;     // nom del senyal o endpoint
    std::string detail;  // dades lliures
};

struct LogState {
    std::mutex             mtx;
    std::vector<LogEntry>  pending;
    std::atomic<bool>      push_pending{false};
};

extern LogState log_state;

// Thread-safe. Llegeix el temps simulat de rellotge_state.
inline void log_append(const char* src, const char* sig, std::string detail) {
    char tbuf[12];
    {
        std::lock_guard<std::mutex> lk(rellotge_state.mtx);
        std::snprintf(tbuf, sizeof(tbuf), "%02d:%02d:%02d",
                      rellotge_state.hour, rellotge_state.minute,
                      rellotge_state.second);
    }
    LogEntry e{ tbuf, src, sig, std::move(detail) };
    {
        std::lock_guard<std::mutex> lk(log_state.mtx);
        log_state.pending.push_back(std::move(e));
    }
    log_state.push_pending.store(true);
}

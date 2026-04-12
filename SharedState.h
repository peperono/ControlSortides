#pragma once
#include <atomic>
#include <mutex>
#include <unordered_map>

// ── SharedState ───────────────────────────────────────────────────────────────
// Dades compartides entre el thread QV (ControlRemot escriu) i el thread
// Mongoose (HttpServer llegeix). Tot accés ha d'estar protegit per mtx,
// excepte push_pending que és atòmic.

struct OutputInfo {
    bool physical  = false;
    bool commanded = false;
    bool result    = false;
    bool remote    = false; // true = REMOTE, false = AUTO
};

struct SharedState {
    std::mutex                          mtx;
    std::unordered_map<int, bool>       outputs;     // estat resultant (legacy)
    std::unordered_map<int, OutputInfo> outputsFull;  // estat complet
    std::atomic<bool>                   push_pending{false};
};

extern SharedState se;

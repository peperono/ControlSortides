#pragma once
#include "../qpcpp/include/qpcpp.hpp"
#include "../signals.h"
#include <unordered_map>
#include <cstddef>

// ── ControlRemot ──────────────────────────────────────────────────────────────
// Active Object que gestiona el mode i l'estat resultant de cada sortida.
//
// Entrades:
//   - OUTPUT_STATE_SIG  → estat físic de les sortides (subscrit)
//   - handleJson(buf, len)  → comandes JSON del thread Mongoose (thread-safe)
//
// Sortida:
//   - OUTPUT_RESULT_SIG (publicat) → estat resultant de les sortides
//
// Comportament per sortida:
//   Mode automàtic : estat resultant = estat físic
//   Mode remot     : estat resultant = última comanda (activate/deactivate)
//   Auto + comanda : aplica puntualment i continua en automàtic
//
// Format JSON esperat (handleJson):
//   [{"id":<int>,"action":"<activate|deactivate|set_remote|set_auto|return_auto|delete>"},...]
class ControlRemot : public QP::QActive {
public:
    explicit ControlRemot() noexcept;

    // Cridat des del thread Mongoose. Thread-safe: Q_NEW + POST a this.
    void handleJson(const char* buf, std::size_t len);

private:
    struct OutputEntry {
        enum class Mode : uint8_t { AUTO, REMOTE } mode = Mode::AUTO;
        bool state  = false; // últim estat físic conegut
        bool commanded = false; // últim valor ordenat
        bool result    = false; // valor resultant publicat
    };

    std::unordered_map<int, OutputEntry> m_outputs;
    OutputResultEvt                      m_resultEvt; // static event (poolNum_=0)

    void publishResult();

    Q_STATE_DECL(initial);
    Q_STATE_DECL(operating);
};

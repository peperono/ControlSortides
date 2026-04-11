#include "ControlRemot.h"
#include <cstring>
#include <cctype>
#include <cstdlib>

// ── Constructor ───────────────────────────────────────────────────────────────

ControlRemot::ControlRemot(QP::QActive* sortides) noexcept
    : QP::QActive{Q_STATE_CAST(&ControlRemot::initial)},
      m_sortides{sortides}
{}

// ── State: initial ────────────────────────────────────────────────────────────

Q_STATE_DEF(ControlRemot, initial) {
    Q_UNUSED_PAR(e);
    // Cap subscripció: els events entrants arriben via handleJson() des de
    // HttpServer, no pel mecanisme pub/sub de QP.
    return tran(&ControlRemot::operating);
}

// ── State: operating ──────────────────────────────────────────────────────────

Q_STATE_DEF(ControlRemot, operating) {
    QP::QState status;
    switch (e->sig) {
        case Q_ENTRY_SIG: { status = Q_HANDLED(); break; }
        default:          { status = super(&ControlRemot::top); break; }
    }
    return status;
}

// ── handleJson ────────────────────────────────────────────────────────────────
// Analitza un array JSON i posta els events de control a ControlSortides.
//
// Format: [{"id":10,"action":"activate"},{"id":11,"action":"set_remote"},...]
//
// Thread-safe: Q_NEW i QActive::POST són segurs des de qualsevol thread.

void ControlRemot::handleJson(const char* buf, std::size_t len) {
    const char* p   = buf;
    const char* end = buf + len;

    // Itera sobre cada objecte { } de l'array
    while (p < end) {
        while (p < end && *p != '{') ++p;
        if (p >= end) break;
        ++p; // salta {

        int  id     = -1;
        char action[32] = {};

        // Llegeix els dos camps (id i action) en qualsevol ordre
        for (int field = 0; field < 2; ++field) {
            // Nom de la clau
            while (p < end && *p != '"') ++p;
            if (p >= end) break;
            ++p; // salta "
            char key[16] = {};
            int  ki = 0;
            while (p < end && *p != '"' && ki < 15) key[ki++] = *p++;
            if (p < end) ++p; // salta "

            // Separador :
            while (p < end && *p != ':') ++p;
            if (p < end) ++p; // salta :
            while (p < end && *p == ' ') ++p;

            if (std::strcmp(key, "id") == 0) {
                // Valor numèric
                while (p < end && !std::isdigit((unsigned char)*p) && *p != '-') ++p;
                id = std::atoi(p);
                while (p < end && (std::isdigit((unsigned char)*p) || *p == '-')) ++p;

            } else if (std::strcmp(key, "action") == 0) {
                // Valor string
                while (p < end && *p != '"') ++p;
                if (p < end) ++p; // salta "
                int ai = 0;
                while (p < end && *p != '"' && ai < 31) action[ai++] = *p++;
            }
        }

        if (id < 0) continue;

        // Genera l'event de QP corresponent a l'acció
        if (std::strcmp(action, "activate") == 0 ||
            std::strcmp(action, "deactivate") == 0)
        {
            // [1] Comanda activar / desactivar
            auto* ev      = Q_NEW(OutputCmdEvt, CTRL_OUTPUT_CMD_SIG);
            ev->output_id = id;
            ev->activate  = (std::strcmp(action, "activate") == 0);
            m_sortides->POST(ev, this);
        }
        else if (std::strcmp(action, "set_remote") == 0 ||
                 std::strcmp(action, "set_auto") == 0)
        {
            // [2] Selecció de mode
            auto* ev      = Q_NEW(OutputModeEvt, CTRL_OUTPUT_MODE_SIG);
            ev->output_id = id;
            ev->remote    = (std::strcmp(action, "set_remote") == 0);
            m_sortides->POST(ev, this);
        }
        else if (std::strcmp(action, "return_auto") == 0)
        {
            // [3] Retorn a automàtic
            auto* ev      = Q_NEW(OutputReturnAutoEvt, CTRL_OUTPUT_RETURN_AUTO_SIG);
            ev->output_id = id;
            m_sortides->POST(ev, this);
        }
    }
}

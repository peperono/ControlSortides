#include "ControlHorari.h"
#include "../mongoose/mongoose.h"
#include "../SharedState.h"
#include <cstdio>
#include <ctime>
#include <mutex>

// ── Claus dels dies al JSON (0=dilluns..6=diumenge) ──────────────────────────

static const char* DAY_KEYS[7] = {
    "$.dilluns", "$.dimarts", "$.dimecres", "$.dijous",
    "$.divendres", "$.dissabte", "$.diumenge"
};

// ── Constructor ───────────────────────────────────────────────────────────────

ControlHorari::ControlHorari() noexcept
    : QP::QActive{Q_STATE_CAST(&ControlHorari::initial)},
      m_tick{this, HORARI_TICK_SIG, 0U}
{}

// ── State: initial ────────────────────────────────────────────────────────────

Q_STATE_DEF(ControlHorari, initial) {
    Q_UNUSED_PAR(e);

    // Inicialitza el rellotge simulat des de l'hora real del sistema
    std::time_t now = std::time(nullptr);
    std::tm lt;
    localtime_r(&now, &lt);
    m_wday   = (lt.tm_wday + 6) % 7;
    m_hour   = lt.tm_hour;
    m_minute = lt.tm_min;

    // 5 ticks = 50 ms @ 100 Hz → cada tick avança 1 minut simulat (1 dia ≈ 72 s)
    m_tick.armX(5U, 5U);
    return tran(&ControlHorari::operating);
}

// ── State: operating ──────────────────────────────────────────────────────────

Q_STATE_DEF(ControlHorari, operating) {
    QP::QState status;

    switch (e->sig) {

        case Q_ENTRY_SIG: {
            status = Q_HANDLED();
            break;
        }

        case HORARI_TICK_SIG: {
            // Recarrega el calendari si HttpServer n'ha rebut un de nou
            if (se.horariLoadPending.exchange(false)) {
                std::string copy;
                {
                    std::lock_guard<std::mutex> lk(se.mtx);
                    copy = se.horariJson;
                }
                loadJson(copy.c_str(), copy.size());
            }

            // Avança el rellotge simulat un minut
            if (++m_minute >= 60) {
                m_minute = 0;
                if (++m_hour >= 24) {
                    m_hour = 0;
                    m_wday = (m_wday + 1) % 7;
                }
            }
            int hh = m_hour;
            int mm = m_minute;
            int wday = m_wday;

            // Publica el rellotge simulat a SharedState per al WebSocket
            {
                std::lock_guard<std::mutex> lk(se.mtx);
                se.horariHour   = hh;
                se.horariMinute = mm;
                se.horariWday   = wday;
            }
            se.push_pending.store(true);

            // Primer compta quantes maniobres coincideixen aquest minut
            int matches = 0;
            for (auto const& m : m_schedule[wday]) {
                if (m.hour == hh && m.minute == mm) ++matches;
            }
            if (matches > 0) {
                auto* ev = Q_NEW(IoStateHttpEvt, IO_STATE_HTTP_SIG);
                ev->n_outputs = 0;
                for (auto const& m : m_schedule[wday]) {
                    if (m.hour == hh && m.minute == mm
                        && ev->n_outputs < IoStateHttpEvt::MAX_OUTPUTS) {
                        ev->outputs[ev->n_outputs++] = { m.id, m.on };
                        std::printf("[ControlHorari] %02d:%02d output %d -> %s\n",
                                    hh, mm, m.id, m.on ? "ON" : "OFF");
                    }
                }
                PUBLISH(ev, this);
            }
            status = Q_HANDLED();
            break;
        }

        default: {
            status = super(&ControlHorari::top);
            break;
        }
    }
    return status;
}

// ── loadJson ──────────────────────────────────────────────────────────────────
// Format esperat:
//   { "dilluns":[{"id":10,"act":"on","time":"07:30"}, ...], "dimarts":[...], ... }

void ControlHorari::loadJson(const char* buf, std::size_t len) {
    for (auto& v : m_schedule) v.clear();

    struct mg_str json = mg_str_n(buf, len);

    for (int d = 0; d < 7; ++d) {
        int alen = 0;
        int aoff = mg_json_get(json, DAY_KEYS[d], &alen);
        if (aoff < 0) continue;
        struct mg_str arr = { json.buf + aoff, (std::size_t)alen };

        for (int i = 0; ; ++i) {
            char path[32];
            std::snprintf(path, sizeof(path), "$[%d]", i);
            int elen = 0;
            int eoff = mg_json_get(arr, path, &elen);
            if (eoff < 0) break;
            struct mg_str elem = { arr.buf + eoff, (std::size_t)elen };

            long id = mg_json_get_long(elem, "$.id", -1);
            if (id < 0) continue;

            int act_len = 0;
            int act_off = mg_json_get(elem, "$.act", &act_len);
            if (act_off < 0) continue;
            struct mg_str act = { elem.buf + act_off, (std::size_t)act_len };
            bool on = (mg_strcmp(act, mg_str("\"on\"")) == 0);

            // time és una string "HH:MM" → inclou les cometes, longitud 7
            int tlen = 0;
            int toff = mg_json_get(elem, "$.time", &tlen);
            if (toff < 0 || tlen < 7) continue;
            const char* t = elem.buf + toff + 1; // salta la cometa inicial
            int hour   = (t[0] - '0') * 10 + (t[1] - '0');
            int minute = (t[3] - '0') * 10 + (t[4] - '0');

            m_schedule[d].push_back(
                { (int)id, on, (uint8_t)hour, (uint8_t)minute });
        }
    }

    int total = 0;
    for (auto const& v : m_schedule) total += (int)v.size();
    std::printf("[ControlHorari] calendari carregat: %d maniobres\n", total);
}

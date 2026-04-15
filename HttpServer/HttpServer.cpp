#include "HttpServer.h"
#include "../ControlRemot/ControlRemot.h"
#include "../ControlRemot/ControlRemotState.h"
#include "../ControlHorari/ControlHorariState.h"
#include "../Rellotge/RellotgeState.h"
#include "../LogState.h"
#include "../signals.h"
#include "../mongoose/mongoose.h"
#include <atomic>
#include <thread>
#include <string>
#include <cstdio>
#include <mutex>
#include "index_html.h"
#include "../ControlHorari/json_horari.h"

LogState log_state;

// ── Server thread ─────────────────────────────────────────────────────────────

static std::atomic<bool>  s_running{false};
static std::thread        s_thread;
static QP::QActive*       s_controlRemot = nullptr;

// ── Helpers JSON ──────────────────────────────────────────────────────────────

static std::string json_str(const std::string& v) {
    std::string out;
    out.reserve(v.size() + 2);
    out += '"';
    for (char c : v) {
        if      (c == '"')  out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else                out += c;
    }
    out += '"';
    return out;
}

static std::string build_ws_message(
        const std::unordered_map<int, OutputInfo>& m,
        int hour, int minute, int second, int wday,
        const std::vector<LogEntry>& logs) {
    static const char* DAYS[7] = {
        "dilluns","dimarts","dimecres","dijous","divendres","dissabte","diumenge"};
    char tbuf[12];
    std::snprintf(tbuf, sizeof(tbuf), "%02d:%02d:%02d", hour, minute, second);

    std::string s = "{\"time\":\"";
    s += tbuf;
    s += "\",\"day\":\"";
    s += (wday >= 0 && wday < 7) ? DAYS[wday] : "";
    s += "\",\"outputs\":{";
    bool first = true;
    for (auto const& [k, v] : m) {
        if (!first) s += ",";
        s += "\"" + std::to_string(k) + "\":{"
             "\"state\":"     + (v.state     ? "true" : "false") + ","
             "\"commanded\":" + (v.commanded ? "true" : "false") + ","
             "\"result\":"    + (v.result    ? "true" : "false") + ","
             "\"mode\":\""    + (v.remote    ? "REMOTE" : "AUTO") + "\""
             "}";
        first = false;
    }
    s += "},\"log\":[";
    bool first_log = true;
    for (auto const& e : logs) {
        if (!first_log) s += ",";
        s += "{\"t\":" + json_str(e.time)
           + ",\"src\":"  + json_str(e.src)
           + ",\"sig\":"  + json_str(e.sig)
           + ",\"d\":"    + json_str(e.detail)
           + "}";
        first_log = false;
    }
    s += "]}";
    return s;
}

// ── WebSocket push ────────────────────────────────────────────────────────────

static void push_if_pending(struct mg_mgr* mgr) {
    bool pending = cr_state.push_pending.exchange(false)
                 | rellotge_state.push_pending.exchange(false)
                 | log_state.push_pending.exchange(false);
    if (!pending) return;

    std::unordered_map<int, OutputInfo> outputs;
    int hh, mm, ss, wd;
    std::vector<LogEntry> logs;
    {
        std::lock_guard<std::mutex> lk(cr_state.mtx);
        outputs = cr_state.outputsResult;
    }
    {
        std::lock_guard<std::mutex> lk(rellotge_state.mtx);
        hh = rellotge_state.hour;
        mm = rellotge_state.minute;
        ss = rellotge_state.second;
        wd = rellotge_state.wday;
    }
    {
        std::lock_guard<std::mutex> lk(log_state.mtx);
        logs = std::move(log_state.pending);
    }

    std::string msg = build_ws_message(outputs, hh, mm, ss, wd, logs);
    for (struct mg_connection* c = mgr->conns; c != nullptr; c = c->next) {
        if (c->is_websocket) {
            mg_ws_send(c, msg.c_str(), msg.size(), WEBSOCKET_OP_TEXT);
        }
    }
}

// ── Mongoose event handler ────────────────────────────────────────────────────

static void http_fn(struct mg_connection* c, int ev, void* ev_data) {
    if (ev == MG_EV_HTTP_MSG) {
        auto* hm = static_cast<struct mg_http_message*>(ev_data);

        // ── GET / ────────────────────────────────────────────────────────────
        if (mg_match(hm->uri, mg_str("/"), NULL)
            && mg_match(hm->method, mg_str("GET"), NULL))
        {
            mg_http_reply(c, 200, "Content-Type: text/html\r\n",
                          "%.*s", (int)sizeof(INDEX_HTML) - 1, INDEX_HTML);
        }

        // ── POST /control ─────────────────────────────────────────────────────
        else if (mg_match(hm->uri, mg_str("/control"), NULL)
                 && mg_match(hm->method, mg_str("POST"), NULL))
        {
            if (s_controlRemot && hm->body.len > 0) {
                static_cast<ControlRemot*>(s_controlRemot)
                    ->handleJson(hm->body.buf, hm->body.len);
                log_append("HttpServer", "POST /control",
                           std::string(hm->body.buf, hm->body.len));
            }
            mg_http_reply(c, 200, "Content-Type: application/json\r\n", "{}");
        }

        // ── GET /horari ───────────────────────────────────────────────────────
        else if (mg_match(hm->uri, mg_str("/horari"), NULL)
                 && mg_match(hm->method, mg_str("GET"), NULL))
        {
            std::string body;
            {
                std::lock_guard<std::mutex> lk(ch_state.mtx);
                body = ch_state.programacioHoraria;
            }
            mg_http_reply(c, 200, "Content-Type: application/json\r\n",
                          "%.*s", (int)body.size(), body.c_str());
        }

        // ── POST /horari ──────────────────────────────────────────────────────
        else if (mg_match(hm->uri, mg_str("/horari"), NULL)
                 && mg_match(hm->method, mg_str("POST"), NULL))
        {
            if (hm->body.len > 0) {
                {
                    std::lock_guard<std::mutex> lk(ch_state.mtx);
                    ch_state.programacioHoraria.assign(hm->body.buf, hm->body.len);
                }
                ch_state.load_pending.store(true);
                log_append("HttpServer", "POST /horari",
                           std::to_string(hm->body.len) + " bytes");
            }
            mg_http_reply(c, 200, "Content-Type: application/json\r\n", "{}");
        }

        // ── GET /ws (WebSocket upgrade) ──────────────────────────────────
        else if (mg_match(hm->uri, mg_str("/ws"), NULL)) {
            mg_ws_upgrade(c, hm, NULL);
        }

        else {
            mg_http_reply(c, 404, "", "Not found\n");
        }

    } else if (ev == MG_EV_WS_MSG) {
        // Missatges WebSocket entrants ignorats per ara
        (void)ev_data;

    } else if (ev == MG_EV_WS_OPEN) {
        std::unordered_map<int, OutputInfo> outputs;
        int hh, mm, ss, wd;
        {
            std::lock_guard<std::mutex> lk(cr_state.mtx);
            outputs = cr_state.outputsResult;
        }
        {
            std::lock_guard<std::mutex> lk(rellotge_state.mtx);
            hh = rellotge_state.hour;
            mm = rellotge_state.minute;
            ss = rellotge_state.second;
            wd = rellotge_state.wday;
        }
        std::string msg = build_ws_message(outputs, hh, mm, ss, wd, {});
        mg_ws_send(c, msg.c_str(), msg.size(), WEBSOCKET_OP_TEXT);
    }
}

// ── Server loop ───────────────────────────────────────────────────────────────

static void server_loop(uint16_t port) {
    char addr[32];
    std::snprintf(addr, sizeof(addr), "http://0.0.0.0:%u", port);

    struct mg_mgr mgr;
    mg_mgr_init(&mgr);
    mg_http_listen(&mgr, addr, http_fn, NULL);
    std::printf("[HttpServer] escoltant a %s\n", addr);

    while (s_running.load()) {
        mg_mgr_poll(&mgr, 100);
        push_if_pending(&mgr);
    }

    mg_mgr_free(&mgr);
}

// ── API pública ───────────────────────────────────────────────────────────────

void HttpServer::start(uint16_t port, QP::QActive* controlRemot) {
    s_controlRemot = controlRemot;
    s_running      = true;
    s_thread       = std::thread(server_loop, port);
}

void HttpServer::stop() {
    s_running = false;
    if (s_thread.joinable()) s_thread.join();
}

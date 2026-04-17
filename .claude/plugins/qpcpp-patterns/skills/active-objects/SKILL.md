# Skill: active-objects

Guia per crear i usar Active Objects (AO) amb el framework QP/C++ en aquest projecte.
Usa aquesta skill quan l'usuari vulgui afegir un AO nou, entendre com funcionen els existents,
o fer canvis a la comunicació entre AOs.

## Context del projecte

Aquest projecte utilitza **QP/C++ amb kernel QV** (cooperatiu, single-thread event loop).
Els AOs actuals són: `Rellotge`, `ControlHorari`, `ControlRemot`.
L'`HttpServer` corre en un thread separat (Mongoose) i es comunica amb els AOs via `POST`.

---

## 1. Estructura de fitxers d'un AO

Cada AO viu en la seva pròpia carpeta i té aquests fitxers:

```
NomAO/
├── NomAO.h          # Declaració de la classe
├── NomAO.cpp        # Implementació dels estats
└── NomAOState.h     # Estat compartit thread-safe (si cal accés extern)
```

---

## 2. Declaració de la classe (NomAO.h)

```cpp
#pragma once
#include <qpcpp.hpp>
#include "signals.h"

class NomAO : public QP::QActive {
public:
    explicit NomAO() noexcept;

    // Mètodes públics cridats des de threads externs (thread-safe):
    void handleJson(const char* buf, std::size_t len);

private:
    // Membres interns de l'AO:
    QP::QTimeEvt m_timer;        // Si necessita timeout periòdic
    SomeDatum    m_data;          // Dades internes de l'AO

    Q_STATE_DECL(initial);       // Sempre necessari
    Q_STATE_DECL(operating);     // Estat principal
    // Q_STATE_DECL(altreEstat); // Afegir si cal
};
```

---

## 3. Implementació (NomAO.cpp)

### Constructor

```cpp
NomAO::NomAO() noexcept
    : QP::QActive{Q_STATE_CAST(&NomAO::initial)},
      m_timer{this, NOM_AO_TIMEOUT_SIG, 0U}  // Només si té QTimeEvt
{}
```

### Estat initial — subscripcions i arrancada del timer

```cpp
Q_STATE_DEF(NomAO, initial) {
    Q_UNUSED_PAR(e);
    subscribe(ALGUN_SIG);          // Un cop per senyal que vulgui rebre
    m_timer.armX(100U, 100U);      // delay inicial, periode (en ticks)
    return tran(&NomAO::operating);
}
```

### Estat operating — gestió d'events

```cpp
Q_STATE_DEF(NomAO, operating) {
    QP::QState status;

    switch (e->sig) {

        case Q_ENTRY_SIG: {
            // Codi d'entrada a l'estat (opcional)
            status = Q_HANDLED();
            break;
        }

        case Q_EXIT_SIG: {
            // Codi de sortida de l'estat (opcional)
            status = Q_HANDLED();
            break;
        }

        case ALGUN_SIG: {
            auto const* ev = Q_EVT_CAST(AlgunEvt);
            // ... processar ev->camp ...

            // Publicar resultat si cal:
            auto* out = Q_NEW(AltreEvt, ALTRE_SIG);
            out->camp = valor;
            PUBLISH(out, this);

            status = Q_HANDLED();
            break;
        }

        case NOM_AO_TIMEOUT_SIG: {
            // Acció periòdica
            status = Q_HANDLED();
            break;
        }

        default: {
            status = super(&NomAO::top);  // Sempre al default
            break;
        }
    }
    return status;
}
```

---

## 4. Definició de senyals i events (signals.h)

Tots els senyals es declaren en `signals.h`. Afegir-los **al final** de l'enum, mantenint
`MAX_SIG` com a últim element:

```cpp
// signals.h
enum Signals : QP::QSignal {
    // ... senyals existents ...
    NOM_AO_TIMEOUT_SIG,     // Timer intern de NomAO
    NOM_AO_RESULT_SIG,      // Resultat publicat per NomAO
    MAX_SIG
};

// Definició dels events (structs):
struct NomAOResultEvt : QP::QEvt {
    int   camp1;
    float camp2;
    // MAX_OUTPUTS si és un array: static constexpr int MAX_X = 8;
};
```

---

## 5. Registre al main.cpp

```cpp
// 1. Variables globals (abans de main):
static NomAO            s_nomAO;
static QP::QEvtPtr      s_nomAOQSto[32];     // Mida de la cua d'events

// 2. Si l'AO genera events dinàmics, assegurar que hi ha pool suficient:
//    QP::QF::poolInit(s_poolSto, sizeof(s_poolSto), sizeof(s_poolSto[0]));

// 3. Dins main(), després de QF::init() i psInit():
s_nomAO.start(
    4U,                          // Prioritat (única per AO, 1=mínima)
    s_nomAOQSto, Q_DIM(s_nomAOQSto),
    nullptr, 0U                  // Stack (nullptr per a QV cooperatiu)
);
```

**Regles de prioritats actuals:**
- `ControlHorari` → 1
- `ControlRemot`  → 2
- `Rellotge`      → 3
- Nou AO → prioritat següent disponible (ex: 4)

---

## 6. Comunicació entre AOs

### Publish-Subscribe (un-a-molts)

```cpp
// Publicar (des de l'AO emissor):
auto* ev = Q_NEW(AlgunEvt, ALGUN_SIG);
ev->camp = valor;
PUBLISH(ev, this);

// Subscriure (a l'estat initial del receptor):
subscribe(ALGUN_SIG);
```

### POST directe (un-a-un)

```cpp
// Des de qualsevol lloc (fins i tot des d'un altre thread):
auto* ev = Q_NEW(AlgunEvt, ALGUN_SIG);
ev->camp = valor;
s_nomAO.POST(ev, sender);   // O bé: POST(ev, this) si és intern
```

### Desde un thread extern (HttpServer)

```cpp
// Thread-safe: Q_NEW i POST/PUBLISH funcionen des de qualsevol thread
void NomAO::handleJson(const char* buf, std::size_t len) {
    // Parsejar buf...
    auto* ev = Q_NEW(AlgunEvt, ALGUN_SIG);
    ev->camp = valor;
    POST(ev, this);
}
```

---

## 7. Estat compartit extern (NomAOState.h)

Per exposar dades a threads externs (HttpServer / WebSocket) de forma thread-safe:

```cpp
// NomAOState.h
#pragma once
#include <mutex>
#include <atomic>
#include <unordered_map>

struct NomAOState {
    std::mutex mtx;
    std::unordered_map<int, SomeDatum> data;
    std::atomic<bool> push_pending{false};
};
extern NomAOState nomao_state;
```

```cpp
// NomAO.cpp — actualitzar des de l'AO:
{
    std::lock_guard<std::mutex> lk(nomao_state.mtx);
    nomao_state.data[id] = datum;
}
nomao_state.push_pending.store(true);
```

---

## 8. QTimeEvt — timers periòdics

```cpp
// Declaració al .h:
QP::QTimeEvt m_timer;

// Inicialització al constructor:
m_timer{this, NOM_AO_TIMEOUT_SIG, 0U}

// Armar al initial (delay inicial, periode — en ticks):
m_timer.armX(50U, 50U);   // 50 ticks @ 100Hz = 500ms

// Armar una sola vegada:
m_timer.armX(100U, 0U);   // 0 = no repeteix

// Desarmar explícitament (p.ex. al Q_EXIT_SIG):
m_timer.disarm();
```

**Referència del tick:** 100 ticks/s configurats a `QF::onStartup()` → 1 tick = 10ms.

---

## 9. Flux de cascada d'events del projecte

```
Rellotge ──[RELLOTGE_TICK_SIG]──> ControlHorari ──[OUTPUT_STATE_SIG]──> ControlRemot
HttpServer ──POST──> ControlRemot
ControlRemot ──[OUTPUT_RESULT_SIG]──> (subscriptors) + SharedState ──WS push──> clients
```

---

## 10. Checklist per afegir un AO nou

- [ ] Crear carpeta `NomAO/` amb `NomAO.h`, `NomAO.cpp` (i `NomAOState.h` si cal)
- [ ] Heretar de `QP::QActive`, declarar `Q_STATE_DECL(initial)` i estats
- [ ] Constructor: `QP::QActive{Q_STATE_CAST(&NomAO::initial)}`
- [ ] Afegir senyals nous a `signals.h` (abans de `MAX_SIG`)
- [ ] Afegir structs d'events a `signals.h`
- [ ] Afegir instància estàtica i cua a `main.cpp`
- [ ] Cridar `.start()` amb prioritat única a `main.cpp`
- [ ] Subscriure als senyals necessaris a l'estat `initial`
- [ ] Afegir als `CMakeLists.txt` si cal

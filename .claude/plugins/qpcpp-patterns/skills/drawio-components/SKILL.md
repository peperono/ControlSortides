# Skill: drawio-components

Guia per crear diagrames de components DrawIO seguint l'estil del projecte ControlSortides
(fitxer de referència: `docs/ControlRemot.drawio`).

Usa aquesta skill quan l'usuari vulgui crear o modificar un diagrama `.drawio` per documentar
l'arquitectura de components, els fluxos d'events QP o la comunicació entre threads.

---

## Paleta de colors i significat

| Color    | Hex fill   | Hex stroke | Representa                          |
|----------|------------|------------|-------------------------------------|
| **Blau** | `#dae8fc`  | `#6c8ebf`  | Active Object (fil QP / QV kernel)  |
| **Verd** | `#d5e8d4`  | `#82b366`  | Event QP (input o output)           |
| **Groc** | `#fff2cc`  | `#d6b656`  | Memòria compartida entre fils       |
| **Rosa** | `#ffcccc`  | `#36393d`  | Thread extern (Mongoose, etc.)      |
| **Blanc**| `#ffffff`  | `#666666`  | Actor extern (Browser, client)      |

La llegenda es posa sempre a l'interior del frame:
```
Blau: Fil QP.
Verd: Events QP.
Groc: Memòria compartida entre fils.
Rosa: Fil Mongoose.
```

---

## Estils XML dels elements

### Frame del diagrama (contenidor global)
```xml
style="shape=umlFrame;whiteSpace=wrap;html=1;pointerEvents=0;recursiveResize=0;
       container=1;collapsible=0;width=170;height=40;"
```
- Títol: `NomProjecte<div>Mes Any</div>`
- Mida típica: `x=10 y=120 width=1630 height=920`

### Active Object (rectangle blau gran)
```xml
style="rounded=0;whiteSpace=wrap;html=1;
       fillColor=#dae8fc;strokeColor=#6c8ebf;
       fontSize=11;fontStyle=1;verticalAlign=top;spacingTop=8"
```
- Text: `NomAO (QP::QActive)`
- Mida típica: `width=350-400 height=100-250` (prou gran per contenir els events fills)
- Conté a dins els rectangles verds i grocs

### Event QP (rectangle verd petit, dins l'AO)
```xml
style="rounded=0;whiteSpace=wrap;html=1;
       fillColor=#d5e8d4;strokeColor=#82b366;fontSize=8"
```
- Mida: `width=132-149 height=35`
- Text d'**input** (esquerra de l'AO): `NomEvt\n(NOM_SIG, subscribe)` o `NomEvt\n(NOM_SIG)`
- Text d'**output** (dreta de l'AO): `NomEvt\n(NOM_SIG, PUBLISH)`
- Timer intern: `NOM_INTERNAL_SIG\n(QTimeEvt, cada Xms)`

### Memòria compartida (rectangle groc petit, dins l'AO)
```xml
style="rounded=0;whiteSpace=wrap;html=1;
       fillColor=#fff2cc;strokeColor=#d6b656;fontSize=8"
```
- Mida: `width=132-145 height=35`
- Text: `nom_state.camp1/camp2/push_pending`
- Es posa al costat dret de l'AO, agrupat amb els events de sortida

### Thread extern (rectangle rosa)
```xml
style="rounded=0;whiteSpace=wrap;html=1;
       fillColor=#ffcccc;strokeColor=#36393d;fontSize=9;fontStyle=1"
```
- Text: `NomThread\n(thread NomLib)`
- Mida: `width=95-110 height=42-70`

### Actor extern — Browser / client (rectangle blanc arrodonit)
```xml
style="rounded=1;whiteSpace=wrap;html=1;
       fillColor=#ffffff;strokeColor=#666666;fontColor=#333333;fontSize=11"
```
- Text: `<b>Browser</b><div><b>Protocol</b></div>` (ex: API HTTP, WebSocket)
- Mida: `width=90-110 height=60-70`

### Nota / comentari
```xml
style="shape=note2;boundedLbl=1;whiteSpace=wrap;html=1;
       size=25;verticalAlign=top;align=center"
```

### Llegenda (text lliure, dins el frame)
```xml
style="text;strokeColor=none;fillColor=none;align=left;verticalAlign=middle;
       spacingLeft=4;spacingRight=4;overflow=hidden;
       points=[[0,0.5],[1,0.5]];portConstraint=eastwest;rotatable=0;
       whiteSpace=wrap;html=1"
```

---

## Connectors (edges)

### Connector estàndard (flux entre elements)
```xml
style="edgeStyle=orthogonalEdgeStyle;rounded=0;html=1;
       exitX=1;exitY=0.5;entryX=0;entryY=0.5"
```
- Labels habituals: `publish`, `push`, `write`, `read`

### Connector curved (HTTP/WS cap a l'exterior)
```xml
style="edgeStyle=none;curved=1;rounded=0;orthogonalLoop=1;
       jettySize=auto;html=1;fontSize=8;startSize=8;endSize=8"
```

---

## Layout i distribució espacial

```
┌─────────────────────────────────────────────────────────────────────────────┐
│ Frame: NomProjecte                                                          │
│                                                                             │
│  ┌─────────────────────────────┐                                            │
│  │ AO Productor (blau)         │                                            │
│  │  [event intern verd] [SS groc] [event output verd]──────────────────┐   │
│  └─────────────────────────────┘                                        │   │
│                     │ publish                                           │   │
│                     ▼                                                   │   │
│  ┌─────────────────────────────┐                                        │   │
│  │ AO Consumidor (blau)        │                                        │   │
│  │  [event input verd]◄────────┘  [SS groc]──► Thread extern (rosa)    │   │
│  │  [event input verd]◄────────────────────── Thread extern (rosa)     │   │
│  │  [event output verd]────────────────────►  Thread extern (rosa)     │   │
│  └─────────────────────────────┘                                        │   │
│                                                                          │   │
│  [Actor Browser]◄──────────────────────────── Thread extern (rosa)      │   │
│  [Actor Browser]───────────────────────────► Thread extern (rosa)       │   │
│                                                                          │   │
│  [Llegenda]                                                              │   │
└──────────────────────────────────────────────────────────────────────────┘
```

**Regles de posicionament:**
- Els events d'**entrada** (subscribe) es posen a la **banda esquerra** de l'AO
- Els events de **sortida** (PUBLISH) i la **memòria compartida** a la **banda dreta**
- El flux general va d'**esquerra a dreta** i de **dalt a baix**
- Els threads externs (HttpServer) es posen a la **cantonada inferior esquerra**
- Els actors (Browser HTTP) a l'**esquerra**, els actors (Browser WS) a la **dreta**

---

## Estructura XML del fitxer .drawio

```xml
<mxfile host="Electron" version="28.2.8">
  <diagram name="NomDiagrama" id="nomdiagrama">
    <mxGraphModel dx="534" dy="348" grid="1" gridSize="10" guides="1"
                  tooltips="1" connect="1" arrows="1" fold="1" page="1"
                  pageScale="1" pageWidth="1654" pageHeight="1169"
                  math="0" shadow="0">
      <root>
        <mxCell id="0" />
        <mxCell id="1" parent="0" />

        <!-- Frame global -->
        <mxCell id="frame" value="NomProjecte<div>Mes Any</div>"
          style="shape=umlFrame;whiteSpace=wrap;html=1;pointerEvents=0;
                 recursiveResize=0;container=1;collapsible=0;width=170;height=40;"
          parent="1" vertex="1">
          <mxGeometry x="10" y="120" width="1630" height="920" as="geometry" />
        </mxCell>

        <!-- AO (contenidor visual, NO container DrawIO) -->
        <mxCell id="ao1" value="NomAO (QP::QActive)"
          style="rounded=0;whiteSpace=wrap;html=1;fillColor=#dae8fc;
                 strokeColor=#6c8ebf;fontSize=11;fontStyle=1;
                 verticalAlign=top;spacingTop=8"
          parent="1" vertex="1">
          <mxGeometry x="400" y="280" width="380" height="110" as="geometry" />
        </mxCell>

        <!-- Event d'entrada (verd, dins l'AO) -->
        <mxCell id="ao1_in" value="NomEvt&#xa;(NOM_SIG, subscribe)"
          style="rounded=0;whiteSpace=wrap;html=1;fillColor=#d5e8d4;
                 strokeColor=#82b366;fontSize=8"
          parent="1" vertex="1">
          <mxGeometry x="406" y="320" width="139" height="35" as="geometry" />
        </mxCell>

        <!-- Event de sortida (verd, dins l'AO) -->
        <mxCell id="ao1_out" value="NomOutEvt&#xa;(NOM_OUT_SIG, PUBLISH)"
          style="rounded=0;whiteSpace=wrap;html=1;fillColor=#d5e8d4;
                 strokeColor=#82b366;fontSize=8"
          parent="1" vertex="1">
          <mxGeometry x="623" y="320" width="139" height="35" as="geometry" />
        </mxCell>

        <!-- Memòria compartida (groc, dins l'AO) -->
        <mxCell id="ao1_ss" value="nom_state.camp1/camp2/push_pending"
          style="rounded=0;whiteSpace=wrap;html=1;fillColor=#fff2cc;
                 strokeColor=#d6b656;fontSize=8"
          parent="1" vertex="1">
          <mxGeometry x="623" y="355" width="139" height="35" as="geometry" />
        </mxCell>

        <!-- Thread extern (rosa) -->
        <mxCell id="thread1" value="HttpServer&#xa;(thread Mongoose)"
          style="rounded=0;whiteSpace=wrap;html=1;fillColor=#ffcccc;
                 strokeColor=#36393d;fontSize=9;fontStyle=1"
          parent="1" vertex="1">
          <mxGeometry x="265" y="772" width="104" height="70" as="geometry" />
        </mxCell>

        <!-- Actor extern (blanc arrodonit) -->
        <mxCell id="browser1" value="&lt;b&gt;Browser&lt;/b&gt;&lt;div&gt;&lt;b&gt;API HTTP&lt;/b&gt;&lt;/div&gt;"
          style="rounded=1;whiteSpace=wrap;html=1;fillColor=#ffffff;
                 strokeColor=#666666;fontColor=#333333;fontSize=11"
          parent="1" vertex="1">
          <mxGeometry x="57" y="772" width="90" height="69" as="geometry" />
        </mxCell>

        <!-- Connector publish -->
        <mxCell id="e1" value="publish"
          style="edgeStyle=orthogonalEdgeStyle;rounded=0;html=1;
                 exitX=1;exitY=0.5;entryX=0;entryY=0.5"
          parent="1" source="ao1_out" target="ao2_in" edge="1">
          <mxGeometry relative="1" as="geometry">
            <Array as="points">
              <mxPoint x="791" y="337" />
              <mxPoint x="791" y="540" />
            </Array>
          </mxGeometry>
        </mxCell>

        <!-- Llegenda (dins el frame, coordenades relatives al frame) -->
        <mxCell id="legend"
          value="&lt;div&gt;Blau: Fil QP.&lt;/div&gt;&lt;div&gt;Verd: Events QP.&lt;/div&gt;&lt;div&gt;Groc: Memòria compartida entre fils.&lt;/div&gt;&lt;div&gt;Rosa: Fil Mongoose.&lt;/div&gt;"
          style="text;strokeColor=none;fillColor=none;align=left;verticalAlign=middle;
                 spacingLeft=4;spacingRight=4;overflow=hidden;
                 points=[[0,0.5],[1,0.5]];portConstraint=eastwest;rotatable=0;
                 whiteSpace=wrap;html=1"
          vertex="1" parent="frame">
          <mxGeometry x="1350" y="800" width="260" height="110" as="geometry" />
        </mxCell>

      </root>
    </mxGraphModel>
  </diagram>
</mxfile>
```

---

## Convencions de noms d'IDs

| Element                   | ID recomanat           |
|---------------------------|------------------------|
| Frame                     | `frame`                |
| AO NomAO                  | `ao_nomao` (ex: `cr`, `ch`, `rl`) |
| Event input de NomAO      | `nomao_in_nomsig`      |
| Event output de NomAO     | `nomao_out_nomsig`     |
| Memòria compartida        | `nomao_ss`             |
| Thread HttpServer HTTP    | `http`                 |
| Thread HttpServer WS      | `ws`                   |
| Actor Browser HTTP        | `browser_http`         |
| Actor Browser WS          | `browser_ws`           |
| Edge N                    | `e1`, `e2`, …          |

---

## Checklist per crear un diagrama nou

- [ ] Crear fitxer `docs/NomDiagrama.drawio`
- [ ] Afegir frame `umlFrame` amb nom i data del projecte
- [ ] Un rectangle blau per cada AO, prou gran per contenir els fills
- [ ] Rectangles verds d'input a la banda esquerra de cada AO
- [ ] Rectangles verds d'output i grocs a la banda dreta de cada AO
- [ ] Rectangles rosa per als threads externs (fora dels AOs)
- [ ] Rectangles blancs arrodonits per als actors externs
- [ ] Connectors orthogonals amb labels (`publish`, `push`, `write`)
- [ ] Llegenda de colors a l'interior del frame (cantonada inferior dreta)
- [ ] Nota `shape=note2` si cal explicar algun flux complex

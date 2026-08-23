# Eficiencia de triggers de jets boosted (CMS Open Data, JetHT Run2016H)

Esta carpeta contiene el analisis de eficiencia de triggers HLT de jets AK8,
usado como paso previo a la construccion del identificador de jets boosted
(carpeta `jet-tagger/` del repositorio).

## Dataset

- **Dataset**: `/JetHT/Run2016H-UL2016_MiniAODv2_NanoAODv9-v1/NANOAOD`
- **Registro CMS Open Data**: [record 30558](https://opendata.cern.ch/record/30558)
- **Formato**: NanoAOD (no requiere CMSSW para analizarse)
- **Subconjunto usado**: 24 de los 72 archivos totales del dataset (1 de cada 3,
  espaciados uniformemente para representar todo el periodo de toma de datos)
- **Certificacion de calidad de datos**: Golden JSON
  `Cert_271036-284044_13TeV_Legacy2016_Collisions16_JSON.txt`

## Motivacion

El objetivo final de la tesis es construir un identificador (tagger) de jets
boosted. Antes de usar datos seleccionados por un trigger de jets como muestra
de entrenamiento o validacion, es necesario demostrar que el trigger no
introduce sesgos en la region cinematica (`p_T`, masa del jet) donde vive la
senal fisica de interes. Esta carpeta documenta ese estudio.

## Metodologia: por que no basta con "todos los eventos"

El dataset JetHT **no es una muestra neutral**: por construccion, solo
contiene eventos que dispararon *alguno* de muchos triggers de jets distintos
(no solo el trigger que se quiere medir). Calcular la eficiencia como

```
eficiencia(p_T) = N(trigger disparo | p_T) / N(TODOS los eventos | p_T)
```

produce resultados no fisicos para triggers con prescale (fraccion de
disparo que decrece con `p_T` en vez de saturar en un plateau), porque el
denominador se contamina con eventos seleccionados por OTROS triggers de
umbral mas alto que no tienen relacion con el trigger que se esta midiendo.

### El metodo correcto: trigger de referencia

La eficiencia de un trigger `T` se mide condicionando el denominador a una
muestra de eventos seleccionada por un **trigger de referencia** `R`,
independiente y ya saturado (en su propio plateau) en la region de `p_T`
bajo estudio:

$$
\varepsilon_T(p_T) \;=\; \frac{N(T \,\cap\, R \mid p_T)}{N(R \mid p_T)}
$$

Donde:

- $N(R \mid p_T)$ = numero de eventos con el FatJet lider en el bin de
  $p_T$ dado, que ademas dispararon el trigger de referencia `R`.
- $N(T \cap R \mid p_T)$ = de esos mismos eventos, cuantos **tambien**
  dispararon el trigger bajo estudio `T`.

Esto aisla la medicion del sesgo de que JetHT es la union de muchos triggers
distintos: al fijar `R` como condicion, el denominador ya no depende de que
trigger disparo el evento en primer lugar mas alla de `R`.

Cada punto de la curva es un cociente binomial (una proporcion), y su
incertidumbre estadistica se calcula con el intervalo de Clopper-Pearson
(metodo por defecto de `TEfficiency` en ROOT), apropiado para cantidades
acotadas entre 0 y 1, a diferencia del error gaussiano simple que puede dar
intervalos fuera de ese rango en los bordes de la curva.

## El codigo: `macros/TriggerEffi2.C`

### Firma de la funcion

```cpp
void TriggerEffi2(const char *triggersToStudy = "HLT_AK8PFJet360_TrimMass30",
                   const char *referenceTrigger = "HLT_AK8PFJet140",
                   const char *treeName = "Events",
                   const char *jsonFile = "/code/Cert_271036-284044_13TeV_Legacy2016_Collisions16_JSON.txt")
```

- `triggersToStudy`: uno o mas nombres de trigger HLT, separados por coma.
  Permite analizar varios triggers en una sola corrida.
- `referenceTrigger`: el trigger `R` usado como condicion del denominador.
- Ambos son parametros de linea de comandos; no requieren editar el archivo
  para analizar un trigger distinto.

### Paso a paso

1. **Carga del JSON**: `LoadJSON()` lee el Golden JSON y construye un
   conjunto (`std::set`) de todos los pares `(run, lumi)` certificados como
   validos. Solo eventos cuyo `(run, luminosityBlock)` este en ese conjunto
   se usan en el analisis.

2. **Parseo de triggers**: `SplitTriggerList()` separa la cadena
   `triggersToStudy` por comas en una lista de nombres de trigger.

3. **Construccion del TChain**: se agregan los 24 archivos `.root` del
   subconjunto representativo a una unica cadena de eventos (`TChain`), y se
   verifica que los 24 se hayan cargado correctamente.

4. **Lectura de ramas**: se conectan las variables del arbol `Events`
   (`run`, `luminosityBlock`, `nFatJet`, `FatJet_pt`, el booleano del
   trigger de referencia, y un booleano por cada trigger a estudiar) a
   variables en memoria mediante `SetBranchAddress`.

   *Nota tecnica*: los booleanos de los triggers a estudiar se guardan en
   un `std::deque<Bool_t>` en vez de `std::vector<Bool_t>`, porque
   `std::vector<bool>` tiene una especializacion interna que empaqueta los
   valores bit a bit y no permite tomar la direccion de memoria de un
   elemento individual (`SetBranchAddress` necesita esa direccion).
   `std::deque<bool>` no tiene esa especializacion.

5. **Bucle sobre eventos**: para cada evento,
   - se descarta si su `(run, lumi)` no esta en el JSON;
   - se descarta si no hay al menos un FatJet reconstruido;
   - se descarta si el trigger de referencia no disparo (`refPass == false`);
   - si sobrevive los filtros anteriores, se llena el histograma del
     denominador (`h_all`) con el $p_T$ del FatJet lider, y se llena el
     histograma numerador de cada trigger estudiado que tambien haya
     disparado.

6. **Calculo de eficiencia**: por cada trigger, se construye un objeto
   `TEfficiency` a partir del par de histogramas (numerador, denominador),
   que calcula automaticamente $\varepsilon_T(p_T)$ y su incertidumbre
   Clopper-Pearson por bin.

7. **Salida grafica**: se genera un canvas combinado (con tantos paneles
   como triggers se hayan pedido, en una grilla de hasta 2 columnas) y un
   PNG individual por cada trigger, con nombres que incluyen tanto el
   trigger estudiado como la referencia usada.

## Diferencias respecto al script original (`TriggerEfficiency_All.C`)

El primer script que se escribio para este analisis (`TriggerEfficiency_All.C`)
media la eficiencia de 4 triggers fijos sin condicionar el denominador a
ningun trigger de referencia. La tabla resume las diferencias exactas:

| Aspecto | `TriggerEfficiency_All.C` (original) | `TriggerEffi2.C` (actual) |
|---|---|---|
| Denominador | Todos los eventos que pasan JSON y tienen >=1 FatJet (sin condicion de trigger) | Igual, pero ademas exige que el trigger de referencia haya disparado |
| Triggers a estudiar | 4 triggers fijos, hardcodeados en un `std::vector<std::string>` dentro del codigo | Cualquier cantidad de triggers, pasados como argumento del macro (cadena separada por comas) |
| Trigger de referencia | No existe el concepto | Parametro configurable del macro |
| Contenedor de booleanos | `std::vector<Bool_t>` (causaba error de compilacion "address of temporary" con `SetBranchAddress`) | `std::deque<Bool_t>` (sin ese problema) |
| Layout del canvas combinado | Fijo en 2x2 (asume siempre 4 triggers) | Calculado dinamicamente segun cuantos triggers se pidan |
| Nombres de archivo de salida | Fijos, uno por trigger hardcodeado | Generados dinamicamente, incluyendo el nombre del trigger de referencia (para no sobrescribir resultados de corridas con distinta referencia) |
| Titulos de los graficos | Cadena fija por trigger, escrita a mano | Generados dinamicamente a partir de los parametros de entrada |

## Hallazgos principales

1. Los triggers `HLT_AK8PFJet140`, `200`, `260` y `320` muestran una
   fraccion de disparo baja (1-15%) y aproximadamente constante en todos
   los runs de Run2016H (verificado en el diagnostico por run), consistente
   con un **prescale fijo** aplicado a lo largo de todo el periodo.
2. Los triggers `HLT_AK8PFJet450` y `HLT_AK8PFJet500` muestran un turn-on
   limpio, alcanzando un plateau cercano a 1.0 entre 550-650 GeV, sin
   evidencia de prescale.
3. `HLT_AK8PFJet360_TrimMass30`, condicionado a un trigger de referencia
   de umbral bajo, tambien alcanza un plateau cercano a 1.0 alrededor de
   800-900 GeV — comportamiento consistente con un trigger no prescaleado.
4. La eficiencia de `HLT_AK8PFJet360_TrimMass30` en funcion de
   `FatJet_msoftdrop` (exigiendo `p_T > 600` GeV, dentro del plateau en
   $p_T$) satura cerca de 1.0 alrededor de $m_{SD} \approx 100$ GeV,
   confirmando que la region de masa de un W/Z boosted (65-105 GeV) esta
   completamente cubierta por el plateau del trigger.

## Archivos en esta carpeta

- `macros/TriggerEffi2.C`: script principal descrito arriba.
- `file-lists/`: listas de los archivos `.root` usados, para reproducibilidad.
- `results/plots/`: graficos PNG generados por el macro.

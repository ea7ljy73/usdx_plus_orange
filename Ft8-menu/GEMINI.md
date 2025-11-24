# 💎 Guía de Buenas Prácticas para Proyectos uSDX / ATmega328P (SDR/QRP)

Esta guía se enfoca en las optimizaciones de software y las consideraciones de hardware esenciales para proyectos de transceptor SDR QRP como el **uSDX**, que depende en gran medida del procesamiento digital de señales (DSP) en el ATmega328P.

---

## 💻 1. Buenas Prácticas de Codificación (Software Crítico)

El código del uSDX es un ejemplo de ingeniería al límite. Cualquier ineficiencia puede provocar *glitches* de audio, ruido o una supresión deficiente de banda lateral.

### 🚀 Optimización de Rendimiento y Memoria (SRAM)

* **Frecuencia de 20 MHz:** Es una práctica común y **esencial** en uSDX usar el ATmega328P a **20 MHz** (en lugar de los 16 MHz de Arduino Uno) para obtener ciclos de CPU adicionales para el DSP. Asegúrate de configurar los fusibles del *bootloader* o la placa correctamente para esta frecuencia.
* **Control Directo de Registros (AVR):** En las secciones de código que manejan el muestreo ADC, el control PWM del PA, y las interrupciones, **utiliza manipulación directa de registros AVR** (`PORTB`, `DDRD`, `TCCR1A`, etc.) en lugar de las funciones de la librería Arduino (`digitalWrite()`). Esto ahorra cientos de ciclos de reloj.
* **Evita la Clase `String`:** **Nunca** uses la clase `String` (con mayúscula) para concatenación de cadenas, ya que la fragmentación de la SRAM es catastrófica en un entorno de DSP tan ajustado. Usa `char*` y búferes estáticos.
* **Almacenamiento en `PROGMEM`:** Todo lo que sea estático (textos de menú, tablas de búsqueda, etc.) debe almacenarse en la memoria Flash (usando `PROGMEM`) para **liberar la SRAM** para las variables de DSP y los búferes de audio.

    ```cpp
    // Ejemplo de PROGMEM para textos de menú
    const char menu_opcion_1[] PROGMEM = "1.1 Volumen";
    // ...
    ```

### 🧹 Estructura del Código

* **Lógica en `loop()` Mínima:** El ciclo principal (`loop()`) debe ser **extremadamente rápido**. La mayor parte del procesamiento de audio y de la señal debe realizarse **dentro de las rutinas de interrupción (ISR)** (ej: Timer/ADC ISR) que se ejecutan a una tasa precisa de muestreo (como 62.5 kHz).
* **Separación Lógica:** Aísla las funciones de la interfaz de usuario (Encoder, LCD) de la lógica crítica de DSP y RF. Permite que las funciones de DSP se ejecuten sin interrupciones o bloqueos causados por el manejo de la pantalla o la entrada.

---

## ⚡ 2. Buenas Prácticas de Hardware (RF y DSP)

El rendimiento del uSDX está intrínsecamente ligado a la calidad del hardware y el diseño de la PCB.

### 🔇 Reducción de Ruido Digital

* **Aislamiento de la Línea AF:** Los amplificadores de audio de AF (frecuencia de audio) son propensos a acoplar ruido de la línea de alimentación. Asegúrate de un **desacoplamiento adecuado** (capacitores de bajo ESR) en la línea de alimentación del amplificador de AF (LM386 o similar) para evitar la realimentación que causa un comportamiento similar al *feedback* con alta ganancia.
* **Decoupling Capacitors (Condensadores de Desacoplo):** Coloca condensadores de desacoplo de **100 nF** (cerámica, cerca del pin) en **cada pin de alimentación/tierra** (`VCC` y `GND`) del ATmega328P, así como en los pines del chip Si5351 y del 74ACT00. Esto minimiza el ruido digital en la línea de alimentación que podría contaminar las etapas de RF.
* **Plano de Masa:** Un **sólido plano de masa** (Ground plane) en la PCB es vital para minimizar el ruido de RF y digital. Esto ayuda a la supresión de la banda lateral y reduce la Distorsión por Intermodulación (IMD).

### 📐 Componentes y Diseño

* **Uso de Zócalos (Sockets):** Siempre usa un **zócalo DIP** para el ATmega328P. La programación del *firmware* del uSDX a menudo requiere el uso de ISP (In-System Programming) y, en variantes que no tienen programador a bordo, es necesario retirar y reprogramar el chip.
* **Emparejamiento de Componentes:** En las etapas críticas, como el mezclador I/Q (para el desfasador de 90 grados), se recomienda emparejar los componentes (ej. condensadores C9/C10/C11/C28 y resistencias R6/R7) para que sus valores sean lo más cercanos posible. Esto es clave para lograr la supresión de banda lateral necesaria en el SDR.
* **Refrigeración del PA:** Aunque el amplificador Clase E es eficiente, para potencias cercanas a 5W QRP, se recomienda la instalación de un **pequeño disipador de calor (radiador)** en el transistor de salida (FET) para garantizar la estabilidad y la longevidad.

El ATmega328P se utiliza en el uSDX para realizar funciones avanzadas de DSP y su rendimiento es fundamental para la calidad de la señal SSB; este video muestra el proceso de carga y programación de este microcontrolador: [The ATmega328P | Burn and Program | 2021](https://www.youtube.com/watch?v=UAqdFtcaNCs).
http://googleusercontent.com/youtube_content/3
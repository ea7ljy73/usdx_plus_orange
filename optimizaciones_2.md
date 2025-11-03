### **Plan de Optimización v2**

1.  **Optimizar `arctan3()` para Cálculo de Fase Rápido:**
    *   **Análisis:** La función `arctan3`, usada en `ssb()`, depende de una división de punto flotante que consume ciclos de CPU valiosos.
    *   **Propuesta:** Crear una tabla de consulta (LUT) para la función interna `_atan2`. La tabla contendrá valores precalculados, permitiendo reemplazar la división por una búsqueda rápida, lo que acelerará notablemente la ruta de procesamiento de la señal de SSB.

2.  **Optimizar `process_minsky()` para Generación de CW:**
    *   **Análisis:** La generación de tonos de CW en `process_minsky()` utiliza aritmética que puede ser optimizada.
    *   **Propuesta:** Convertir los cálculos a **aritmética de punto fijo**. Esto es considerablemente más rápido que la emulación de punto flotante y mejorará la eficiencia y precisión en modo CW.

3.  **Eliminación Progresiva de `delay()`:**
    *   **Análisis:** Persisten llamadas a `delay()` en varias partes del código (fuera del bucle de botones), bloqueando la ejecución y afectando la fluidez de la interfaz.
    *   **Propuesta:** Realizar una auditoría completa del código para identificar todas las llamadas restantes a `delay()`. Reemplazar cada una por temporizadores no bloqueantes basados en `millis()`, similar a lo que se hizo con los botones.

4.  **Reducción de Huella de Memoria (Código y RAM):**
    *   **Análisis:** El uso de funciones de librería como `sprintf()` y el almacenamiento de cadenas de texto en RAM aumentan innecesariamente el consumo de memoria.
    *   **Propuesta:**
        *   Reemplazar las llamadas a `sprintf()` por funciones manuales de concatenación de cadenas, que son más ligeras y eficientes en microcontroladores.
        *   Mover todas las cadenas de texto constantes (`const char*`) que aún residan en RAM a la memoria de programa (Flash) utilizando la macro `F()` o `PROGMEM`.

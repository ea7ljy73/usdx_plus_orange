Plan de Optimización y Personalización para usdx_plus_orange.ino

1.  **Optimizar la función `filt_var()` (Filtros DSP):**
    *   Reemplazar las divisiones por potencias de 2 con operaciones de desplazamiento de bits (ej. ` / 16` se convierte en `>> 4`).
    *   Reemplazar multiplicaciones por combinaciones de desplazamientos y sumas (ej. `* 30` se convierte en `(x << 5) - (x << 1)`).

2.  **Optimizar las funciones `smeter()` y `readSWR()` (Medidores):**
    *   **`smeter()`:** Reemplazar el cálculo de `log10` para la conversión a dBm por una tabla de consulta (LUT) o una aproximación lineal por tramos con enteros.
    *   **`readSWR()`:** Refactorizar los cálculos para usar aritmética de enteros, minimizando el uso de punto flotante.

3.  **Optimizar la función `ssb()` (Generación de SSB):**
    *   Revisar las multiplicaciones del filtro FIR de la transformada de Hilbert y reemplazarlas por operaciones de desplazamiento y suma más eficientes.

4.  **Mejorar la capacidad de respuesta de la `loop()` principal:**
    *   Reimplementar la lógica de manejo de botones utilizando una máquina de estados no bloqueante basada en `millis()` para eliminar los `delay()` y las esperas activas.

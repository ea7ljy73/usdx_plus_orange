# Configuración de Fuses para uSDX Plus Orange (27MHz)

## Programación ISP

El firmware se programa mediante **ISP (In-System Programming)** usando un programador USBasp o Arduino como programador.

### Conexiones ISP (Arduino Nano como programador)

```
Programador → ATmega328P en uSDX
----------------------------------------
VCC (5V)    → VCC
GND         → GND
D13 (SCK)   → SCK
D12 (MISO)  → MISO
D11 (MOSI)  → MOSI
D10 (RESET) → RESET
```

### Fuses para 27MHz (sin bootloader)

Para un cristal de 27MHz externo, sin bootloader:

| Fuse | Valor | Descripción |
|------|-------|-------------|
| Low | `0xFF` | Full swing crystal oscillator, startup time 258CK + 64ms |
| High | `0xD6` | No bootloader, SPI enabled, EEPROM preserved |
| Extended | `0xFD` | Brown-out disabled (o `0x05` para BOD a 2.7V) |

### Configuración desde línea de comandos (avrdude)

```bash
# Leer fuses actuales
avrdude -c usbasp -p m328p -U hfuse:r:-:h -U lfuse:r:-:h -U efuse:r:-:h

# Programar fuses para 27MHz
avrdude -c usbasp -p m328p -U lfuse:w:0xFF:m -U hfuse:w:0xD6:m -U efuse:w:0xFD:m

# Programar firmware
avrdude -c usbasp -p m328p -U flash:w:usdx_plus_orange.ino.hex:i
```

### Configuración desde Arduino IDE

1. Seleccionar placa: **Arduino Nano**
2. Seleccionar procesador: **ATmega328P**
3. Seleccionar programador: **USBasp** (o tu programador)
4. Herramientas → **Grabar bootloader** (configura los fuses)
5. Herramientas → **Subir usando programador** (Ctrl+Shift+U)

### Verificación de fuses en Arduino IDE

Para verificar los fuses en Arduino IDE, el proceso estándar de "Grabar bootloader" debería configurarlos correctamente según la frecuencia seleccionada.

### Notas importantes

1. **Sin bootloader**: No se necesita bootloader ya que el código usa ISP
2. **27MHz crystal**: El cristal de 27MHz requiere fuses específicos
3. **Fuse extendido**: Para 27MHz se recomienda Brown-out Disabled o BOD a 2.7V
4. **División de clock**: No usar CKDIV8 (el ATmega debe correr a velocidad completa)

### Solución de problemas

Si el microcontrolador no responde después de programar:
1. Verificar conexiones ISP
2. Verificar voltaje (5V)
3. Verificar que el cristal esté funcionando
4. Intentar programar con velocidad baja (`-B 5` en avrdude)

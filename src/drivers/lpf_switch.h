/**
 * @file lpf_switch.h
 * @brief Low-Pass Filter Switch para uSDX
 *
 * Controla la selección de filtros paso bajo según la banda de operación.
 * Soporta diferentes configuraciones de hardware.
 */

#ifndef DRV_LPF_SWITCH_H
#define DRV_LPF_SWITCH_H

#include <Arduino.h>

// ============================================================================
// CONFIGURACIÓN DE HARDWARE
// ============================================================================

#ifdef LPF_SWITCHING_DL2MAN_USDX_REV3

// Configuración para DL2MAN USDX REV3
// Utiliza 3 bits para seleccionar el filtro:
// LPF_BIT0 -> Q1 (2MHz)
// LPF_BIT1 -> Q2 (5MHz)  
// LPF_BIT2 -> Q3 (9.5MHz)

#define LPF_BIT0  A3  // PC3
#define LPF_BIT1  A2  // PC2
#define LPF_BIT2  A1  // PC1

#elif defined(LPF_SWITCHING_SIMPLE)

// Configuración simple con relés
// Definir pines de los relés según el hardware
#define LPF_RELAY_1  5   // PD5
#define LPF_RELAY_2  4   // PD4
#define LPF_RELAY_3  3   // PD3

#else

// Configuración estándar (sin LPF switching)
// Los filtros se seleccionan manualmente o no hay switching

#endif

// ============================================================================
// FRECUENCIAS DE CORTE DE LPF (MHz)
// ============================================================================

// Tabla de frecuencias de corte por banda (MHz)
// Orden: 160m, 80m, 60m, 40m, 30m, 20m, 17m, 15m, 12m, 10m, 6m
#define LPF_FREQUENCIES { 2, 4, 6, 8, 12, 17, 20, 26, 30, 35, 54 }

// Número de filtros disponibles
#define NUM_LPF 11

// ============================================================================
// DECLARACIONES DE FUNCIONES
// ============================================================================

namespace lpf {

/**
 * @brief Inicializa los pines del LPF switch
 */
void init();

/**
 * @brief Selecciona el LPF según la frecuencia
 * @param freq_khz Frecuencia en kHz
 */
void set_by_frequency(uint32_t freq_khz);

/**
 * @brief Selecciona el LPF por índice
 * @param index Índice del filtro (0-NUM_LPF-1)
 */
void set_by_index(uint8_t index);

/**
 * @brief Desactiva todos los filtros
 */
void off();

} // namespace lpf

#endif // DRV_LPF_SWITCH_H

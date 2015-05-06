/**
 * This header files contains function prototypes and macros for CSlogging.c
 *
 * @file CSlogging.h
 * @author Kev
 * @ingroup CSlogging
 */
/**
 * @ingroup CSlogging
 * @{
 */

#ifndef CSLOGGING_H
#define	CSLOGGING_H

#include <stdint.h>
#include "types.h"

/* Constant Definitions */
#define NUM_SENSORS                  44
#define SENSOR_BYTES                 14
#define LOGGING_TEMP_LOG_SIZE       100     /// Max number of temperature measurements
#define LOGGING_VOLTAGE_LOG_SIZE    100     /// Nax number of voltage measurements
#define TEMP_LOG_FILE "temp.log"            /// Where to store the temperature entries
#define VOLTAGE_LOG_FILE "volt.log"            /// Where to store the voltage entries


/* Type Definitions */
//CHANGE THIS BACK TO FLOAT!
typedef uint16 TempEntry;        /// A temperature measurement value
typedef float VoltageEntry;     /// A voltage measurement value

/* Function Prototypes */

/**
 * Save off the current tempterature reading for sensor 128 to a circular buffer
 */
void handleTelemetryRecording();

/**
 * Get the number of days in a given epoch timestamp, and format a telemetry
 * filename with the days. (Looks like "DDDDDDDD.TXT", e.g. "00000042.TXT"
 */
void epoch_to_telemetry_filename(uint32_t epoch, char filename[12]);

/**
 * Start flushing telemetry to SD card
 */
void startFlushToSD();


//writing to the monitoring file
void writeToMonitor(INT64 time);

#endif	/* CSLOGGING_G */

/**
 * @}
 */
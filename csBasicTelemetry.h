/* 
 * File:   csBasicTelemetry.h
 * Author: Donald
 *
 * Created on September 22, 2014, 12:52 PM
 */

#ifndef CSBASICTELEMETRY_H
#define	CSBASICTELEMETRY_H

//define statement to also declare clearBasicTelem as basicTelemInit
#define clearBasicTelemetry     initBasicTelemetry

uint16 checkInitBasicTelemetry();
void storeBattDelta(uint16 battery);
uint16 initBasicTelemetry();
uint16 clearBasicTelemetry();
void storeBasicTelemetry(telemetry_block_t values);
void storeAnomalyBasicTelemetry(uint16 anomalyInfo, uint32 time);
uint16_t getBasicTelemetry(char * telem);

#endif	/* CSBASICTELEMETRY_H */


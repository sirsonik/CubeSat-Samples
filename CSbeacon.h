/* 
 * File:   CSbeacon.h
 * Author: Donald Eckels
 *
 * Created on February 1, 2015, 6:28 PM
 */

#ifndef CSBEACON_H
#define	CSBEACON_H

typedef enum{
    SOFTWARE_STATE  = 0,
    NV_MEM_CHECK,   //1
    PGM_IMG_STATUS, //2
    VT_MEM_CHECK,   //3
    FAULTS,         //4
    PROC_FAULTS,    //5
    SC_BATT_V,      //6
    SC_BATT_A,      //7
    SC_BATT_T,      //8
    PL_BATT_V,      //9
    PL_BATT_A,      //10
    PL_BATT_T,      //11
    V3d3_V,         //12
    V3d3_A,         //13
    V3d3_SW1_V,     //14
    V3d3_SW1_A,     //15
    V3d3_SW2_V,     //16
    V3d3_SW2_A,     //17
    V3d3_SW3_V,     //18
    V3d3_SW3_A,     //19
    V3d3_SW4_V,     //20
    V3d3_SW4_A,     //21
    V5_V,           //22
    V5_A,           //23
    V5_SW5_V,       //24
    V5_SW5_A,       //25
    V12_V,          //26
    V12_A,          //27
    V12_SW6_V,      //28
    V12_SW6_A,      //29
    RADIO_T,        //30
} beacon_msg_index_t;

bool beaconEnable(bool enabled);
void beaconMsgInit();
void beaconMsgUpdateTelemetry();
void beaconMsgUpdateSingle(beacon_msg_index_t index, char val);
char intToBeaconChar(uint16_t val);
uint8_t getTempChar(uint8_t index);


#endif	/* CSBEACON_H */


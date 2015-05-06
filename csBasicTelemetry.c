/*
 * File:   csBasicTelemetry.c
 * Author: Donald Eckels
 *
 * Created on September 22, 2014, 12:52 PM
 */

#include "CSstateAnomaly.h"
#include "Globals.h"
#include "types.h"
#include "CSi2c.h"
#include "debug.h"
#include "CSlinearBuf.h"
#include "CSdefine.h"


/**
 * checkBasicTelemInit
 * INPUT: none
 * OUTPUT: uint16 - TRUE if all values are 0, FALSE if otherwise.
 * INFO: Checks the basic telemetry structure to verify it has been
 * cleared, or at least all values are set to 0.
 * Or logic is used in order to prevent potential overflow back to 0.
 */
uint16 checkInitBasicTelemetry(){
    uint8 i;
    uint16 chk = 0;
    //check the sensor values
    for(i=0;i<NUM_SENSORS;i++){
        chk |= Global->csBasicTelemetry.csSingleTelemetry[i].hiVal;
        chk |= Global->csBasicTelemetry.csSingleTelemetry[i].lowVal;
        //check the corresponding high and low times are zeroed
        chk |= Global->csBasicTelemetry.csSingleTelemetry[i].hiTime;
        chk |= Global->csBasicTelemetry.csSingleTelemetry[i].lowTime;
        //check the average and n (number of values in the average)
        chk |= Global->csBasicTelemetry.csSingleTelemetry[i].avg;
        chk |= Global->csBasicTelemetry.csSingleTelemetry[i].n;
    }
    //check the anomaly registers
    for(i=0;i<5;i++){
        chk |= Global->csBasicTelemetry.anomalyModeTime[i];
        chk |= Global->csBasicTelemetry.anomalyModeBasicInfo[i];
    }
    //check the next anomaly slot variable
    chk |= Global->csBasicTelemetry.anomalyslot;

    //check the battery average
    for(i=0;i<3;i++){
        chk |= Global->csBasicTelemetry.battRecentTemp[i];
    }
    chk |= Global->csBasicTelemetry.battDeltaTemp;
    chk |= Global->csBasicTelemetry.battSlot;


    return chk;
}

/**
 * storeBattDelta
 * INPUT: uint16 - value of the spacecraft battery to have the value delta calculated & stored
 * OUTPUT: NONE
 * INFO: every 10 seconds the value of the battery is passed into this function. The current battery
 * value is subtracted from the battery value from 30 seconds ago to get the delta. Both this battery
 * temperature and the delta are stored away. The delta is in place so we can keep track of how fast
 * or not the battery temp is changing.
 */
void storeBattDelta(uint16 battery){
    uint8 slot;
    uint16 delta;
    slot = Global->csBasicTelemetry.battSlot; //get current slot
    delta = (battery - Global->csBasicTelemetry.battRecentTemp[slot]); //calculate the delta
    G_SET (csBasicTelemetry.battRecentTemp[slot], &battery); //store new val
    G_SET (csBasicTelemetry.battDeltaTemp, &delta); //store new delta
    slot = (slot+1)%3; //advance pointer to next oldest value
    G_SET (csBasicTelemetry.battSlot, &slot); //store the updated index
}

/**
 * initBasicTelemetry
 * INPUT: none
 * OUTPUT: TRUE if everyhting is set to 0, FALSE otherwise.
 * INFO: Initialize the basic telemetry structure and set everything to 0;
 * Since the basic telemetry portion of the global struct could end up on garbage memory
 * or if the ground station requests to clear the basic telemetry structure.
 */
uint16 initBasicTelemetry(){
    uint16 ret,battTempBackup,slot;
    //back up the battery temp
    slot = Global->csBasicTelemetry.battSlot;
    slot = (slot+2)%3; // back it up by one, prevent going out of bounds negative
    battTempBackup = Global->csBasicTelemetry.battRecentTemp[slot];
    //clear out the entire structure
    G_SET(csBasicTelemetry, NULL);

    //check everything and return it
    ret = checkInitBasicTelemetry();
    
    //restore the last battery value
    storeBattDelta(battTempBackup);
    storeBattDelta(battTempBackup);
    storeBattDelta(battTempBackup);
    return ret;
}



/**
 * Update the basic telemetry structure with all of the telemetry values that were
 * @param values - array of the most recent value of each sensor, if it is to be stored away
 * @param sensors - number of sensors to be updated - only temp sensors aren't done every time
 * @param time - csunSatEpoch time value
 * INFO: One block of telemetry is passed into this function. It is stored aside as the last telemetry.
 *     Each sensor is pulled out successively, the current value is added into the running average.
 *          - The average is multiplied with the number of readings in order to get an approximate sum
 *            of the values used in the calculation, but not take up more storage on the satellite.
 *            This proves to have less error per calculation and takes less steps than any other method.
 *            The buf variable is 32 bit in order to give the average more room to expand. Because the
 *            telem values are all 12 bit, this gives them 20 bits for expansion. Even if a running average is
 *            at 0x0FFF it can function properly at over one million readings
 *     The value for the sensor is compared to the maximum and minimum. If it is at or beats the current value
 *     the new value is saved and the time associated with it is updated. This is done because obviously we want
 *     to know when the most recent time i was seen at this value, not at the first time.
 * This single sensor set of values is stored away, the next one is pulled out, and this is repeated until all
 * sensors are updated. Every ten seconds the battery temperature delta is calculated and stored away
 */
void storeBasicTelemetry(telemetry_block_t values){
    uint8_t i;
    uint32_t buf;
    uint32_t temp;
    csSingleBasicTelemetry tempTelem;

    //go through each possible sensor value to be stored
    for(i=0;i<NUM_SENSORS;i++){
        //pull out a copy of the one sensor to be edited time time around
        memcpy(&tempTelem, &(Global->csBasicTelemetry.csSingleTelemetry[i]), sizeof(csSingleBasicTelemetry));
        //move average to buffer for averaging math to prevent overflow
        buf = tempTelem.avg;
        temp = values.readings[i];

        //increase to running sum
        buf = (buf * tempTelem.n);

        //add current value
        buf += temp;
        //increment n to reflect there is another value in the average
        tempTelem.n += 1;
        //decrease back to average
        buf = (buf / tempTelem.n);
        //return to structure
       tempTelem.avg = buf;
        //overwrite the high value if the current reading is the highest
        if((values.readings[i] >= tempTelem.hiVal) || (tempTelem.n == 1)){
            //max value
            tempTelem.hiVal = values.readings[i];
            //time values
            tempTelem.hiTime = values.epoch;
        }
        //overwrite the low value if the current reading is the lowest
        if((values.readings[i] <= tempTelem.lowVal) || (tempTelem.n == 1)){
            //min value
            tempTelem.lowVal = values.readings[i];
            //time values
            tempTelem.lowTime = values.epoch;
        }
       //put the updated sensor values back
       G_SET(csBasicTelemetry.csSingleTelemetry[i], &tempTelem);
    }

    //update the payload battery temp average when necessary
    if((values.epoch%10)==0){
        storeBattDelta(values.readings[27]);
        dprintf("Delta: %d\r\n", Global->csBasicTelemetry.battDeltaTemp);
    }
}
/*
 * storeAnomalyBasicTelemetry
 * INPUT: uint16 anomalyInfo - numeric value corresonding to the anomaly and potentially some basic information regarding it
 *        uint32 time - time that this anomaly occurred
 * OUTPUT: none
 * INFO: As part of the basic telemetry, the five most recent anomalies are logged here. This provides a quick overview
 * of any problems the satellite may have experienced without taking the time to send down larger sections of data, like
 * the journal. The number was selected to be a uint16 so that not only could a number representing the anomaly be sotred,
 * but potentially some very basic information.
 */
void storeAnomalyBasicTelemetry(uint16 anomalyInfo, uint32 time){
    uint8 slot;
    //SettleGlobal();
    //make the slot value easier to read
    slot = Global->csBasicTelemetry.anomalyslot;
    //store the anomlaly mode info
    G_SET(csBasicTelemetry.anomalyModeBasicInfo[slot], &anomalyInfo);
    //store the anomaly mode time
    G_SET(csBasicTelemetry.anomalyModeTime[slot], &time);

    //prep the anomaly storage for the next slot
    slot = ((slot+1)%5);
    G_SET(csBasicTelemetry.anomalyslot, &slot);
}


/*
 * getBasicTelemetry
 * INPUT: char* telem - pointer to the array that will house the data to be sent to the ground
 * OUTPUT: NONE (technically char * telem)
 * INFO: all of the basic telemetry is packed up and sent to the ground. Nested loops are used in order to keep the code
 * slightly more condensed and in case the size of the time values were adjusted more in the future.
 */
uint16_t getBasicTelemetry(char * telem){
    uint8 i,j;
    //first loop to input each sensor's information
    /*Because each sensor has 17 bytes associated with it, the first value of each sensor
     , the sensor number, needs to be offset to every 17th byte (which is the reason for i*17 */
    for(i=0;i<NUM_SENSORS;i++){
        //telem values are 2 bytes
        telem[((i*SENSOR_BYTES) + 0)] = (Global->csBasicTelemetry.csSingleTelemetry[i].hiVal >> 8); //MSByte goes in the lowest index (lower address), holds for all non-byte sized values
        telem[((i*SENSOR_BYTES) + 1)] = Global->csBasicTelemetry.csSingleTelemetry[i].hiVal;
        //each time value is 1 byte
        for(j=0;j<4;j++){
            telem[((i*SENSOR_BYTES) + 2 + j)] = (Global->csBasicTelemetry.csSingleTelemetry[i].hiTime >> (24-(8*j)));
        }
        //telem values are 2 bytes
        telem[((i*SENSOR_BYTES) + 6)] = (Global->csBasicTelemetry.csSingleTelemetry[i].lowVal >> 8);
        telem[((i*SENSOR_BYTES) + 7)] = Global->csBasicTelemetry.csSingleTelemetry[i].lowVal;
        for(j=0;j<4;j++){
            //each time value is 1 byte
            telem[((i*SENSOR_BYTES) + 8 + j)] =(Global->csBasicTelemetry.csSingleTelemetry[i].lowTime >> (24-(8*j)));
        }
        //telem values are 2 bytes
        telem[((i*SENSOR_BYTES) + 12)] = (Global->csBasicTelemetry.csSingleTelemetry[i].avg >> 8);
        telem[((i*SENSOR_BYTES) + 13)] = Global->csBasicTelemetry.csSingleTelemetry[i].avg;
    }
    //pass the payload battery delta temp
    telem[NUM_SENSORS*SENSOR_BYTES] = (Global->csBasicTelemetry.battDeltaTemp >> 8);
    telem[(NUM_SENSORS*SENSOR_BYTES)+1] = Global->csBasicTelemetry.battDeltaTemp;
    //get the satellite state and put it in one byte
    telem[(NUM_SENSORS*SENSOR_BYTES)+2] = Global->csState.mainState;
    //second loop to include the anomaly mode information. will be all zeroes if all are empty
    for(i=0;i<5;i++){
        //basic info is 2 bytes
        telem[((NUM_SENSORS*SENSOR_BYTES)+ 3 + (i*6))] = (Global->csBasicTelemetry.anomalyModeBasicInfo[i] >> 8);
        telem[((NUM_SENSORS*SENSOR_BYTES)+ 3 + (i*6) + 1)] = Global->csBasicTelemetry.anomalyModeBasicInfo[i];
        for(j=0;j<4;j++){
            //each time value is 1 byte
            telem[((NUM_SENSORS*SENSOR_BYTES)+ 3 + (i*6) + 2 + j)] = (Global->csBasicTelemetry.anomalyModeTime[i] >> (24-(8*j)));
        }
    }
    uint16_t retVal = ((NUM_SENSORS*SENSOR_BYTES)+3+(5*6));
    return retVal;
}
/**
 * This file contains functions for managing logging of data.
 *
 * @file CSlogging.c
 * @author Kev
 *
 * @ingroup CSlogging
 * @defgroup CSlogging Data Caching
 * @{
 * Implements caching of measured paramters such as temperature and power supply
 * voltage.
 */

/* System & Local Includes */
#include <string.h>
#include "metal/timers.h"
#include "CSlogging.h"
#include "CSdefine.h"
#include "CStemp.h"
#include "CSi2c.h"
#include "CSlinearBuf.h"
#include "CStimers.h"
#include "CSuptime.h"  // for cachingTimer
#include "csBasicTelemetry.h"
#include "debug.h"
#include "CSopenSourceFAT.h"
#include "CSstateStatusMonitoring.h"
#include "Globals.h"

//#include "CStimeElapse.h" // fortesting remove before flight

/**
 * handleTelemetryRecording
 * INPUT: none
 * RETURN: none
 * INFO: Function is called once every second and handles all of our telemetry recording needs.
 *       Each ADC is polled and the telemetry is placed into the buffer. The ADC count values occupy
 *       a uint16, but due to i2c protocol, they are broken into 2 bytes. These are then reconstructed and the
 *       address (pin) is stripped.
 *       The ADCs are called in order and stored in order so that this function can process faster.
 *       This set of telemetry is then added to the buffer that colects telem to be flushed to the SD card.
 *
 *       Afterwards, every 8th call of this function initiates the flush to SD function.
 *
 *       The basic telemetry and last telemetry are updated.
 *
 *       SettleGlobal is also called. this is done since we only want this to happen once per second
 *       It was previously happening WAY more often (unnecessary due to the probability of bit errors)
 *       and a second timer that occurs once per second could interfere with this timer.
 *
 *       Because we now have new data, the state status monitoring state machine is ready to be put into
 *       the pending processing state. This way the state status monitoring state machine will only check
 *       and execute the pending command sequence once per second and can continue in any of the other states.
 */
void handleTelemetryRecording(){
    dprintf("TlmLog\r\n");

    //first time is to retrieve the current RTC time, second is to parse time into packets for placing into
    //the circular buffer

    LinearBuf buf;
    INT64 time;
    uint8 timeRecord[8];
    uint8 i;
#if PUMPKIN_DEV_BOARD
    uint8 j, arrIndex, using;
    uint8 fromADC[16];
    BYTE ADCaddrs[7];
    uint16 buffer;
#endif
    telemetry_block_t tlmBuff;

    memset(&tlmBuff, 0, sizeof(telemetry_block_t));

        #if PUMPKIN_DEV_BOARD
        time = getRTC();  //gets the RTC via the I2C buffer
        #elif EXPLORER_16
        time= 0x00323B17041F0C0E; //12/31/2014 23:59:50
        #endif
    //separate the RTC values for storage
    for(i=0; i<8; i++){
        timeRecord[i] = 0xff & ( time >> ( (7-i) * 8) );
    }
    tlmBuff.epoch = csunSatEpoch(time);
    #if PUMPKIN_DEV_BOARD
    ADCaddrs[0] = ADC_1;
    ADCaddrs[1] = ADC_2;
    ADCaddrs[2] = ADC_3;
    ADCaddrs[3] = ADC_4;
    ADCaddrs[4] = ADC_5;
    ADCaddrs[5] = ADC_6;
    ADCaddrs[6] = ADC_7;
    arrIndex = 0; //start at the beginning
    //loop through each ADC
    for(i=0;i<7;i++){
        switch(i){ //switch determines how many sensors we're looking at on each ADC
            case 5:
                using = 4;
                break;
            case 1:
            case 2:
            case 6:
                using = 6;
                break;
            case 0:
            case 3:
                using = 7;
                break;
            case 4:
                using = 8;
                break;
        }
        configADC(ADCaddrs[i]);
        if(readADC_AllChannels(ADCaddrs[i], fromADC) != 0){ //store away values only if the ADC reported properly
            for(j=0; (j<using) &&(arrIndex <NUM_SENSORS);j++){
                //extract each 2 bytes for a sensor and drop the addressing bits
                buffer = (0x0FFF & ((fromADC[(2*j)]<<8) + (fromADC[(2*j)+1]))); //get the two bytes for the first sensor and get rid of the address
                //move the telemetry to the array
                tlmBuff.readings[arrIndex] = buffer;
                
                arrIndex++;
            }
        }
        else{//if it's not responding properly we need to skip its values
            arrIndex += using;
        }
     }


        memcpy(&buf, &(Global->csTelemetry.buf),sizeof(buf));
        linearBuf_put(&buf, &tlmBuff);
        G_SET(csTelemetry.buf, &buf);

        if(linearBuf_count(&Global->csTelemetry.buf) >= 8){
            startFlushToSD();
        }


        //add to basic telemetry averages
        storeBasicTelemetry(tlmBuff);
        SettleGlobal();
        G_SET(csLastTelemetry, values.readings); //record the most recent telem values - for use by

        if(Global->csState.statMonState != PENDING_PROCESS){
            StatMonState newState = PENDING_PROCESS;
            changeStatMonState(newState); //update so next time we go through state status monitoring we will check pending commands
        }
}

void epoch_to_telemetry_filename(uint32_t epoch, char filename[12]) {
    uint32_t days = epoch / (60ul*60ul*24ul);

    filename[0]  = '0' + ((days / 10000000) % 10);
    filename[1]  = '0' + ((days / 1000000 ) % 10);
    filename[2]  = '0' + ((days / 100000  ) % 10);
    filename[3]  = '0' + ((days / 10000   ) % 10);
    filename[4]  = '0' + ((days / 1000    ) % 10);
    filename[5]  = '0' + ((days / 100     ) % 10);
    filename[6]  = '0' + ((days / 10      ) % 10);
    filename[7]  = '0' + ((days / 1       ) % 10);
    filename[8]  = '.';
    filename[9]  = 'T';
    filename[10] = 'E';
    filename[11] = 'L';
}

/*
 *
 * Function:
 *      startFlushToSD()
 * Summary:
 *      Flushes telemetry buffer to daily .TEL file on SD card
 * Conditions:
 *      Called from OnMetal_FlushTelem_Tick() located in CSmain.c
 * Input:
 *      None
 * Return Values:
 *      None
 * Side Effects:
 *      Daily .TEL file is updated on the SD card
 * Description:
 *      Function opens the daily .TEL file on the SD card and flushs the
 *      telemetry buffer to it. Then, the buffer is cleared.
 * Remarks:
 *      Written by Natalia Alonso
 *
 *
 */
void startFlushToSD(){
    uint16_t old_tail = Global->csTelemetry.buf.tail;
    G_SET(csTelemetry.buf.tail,NULL);

    FSFILE* testFile = NULL;
    uint32_t epoch = 0ul;

    char filename[] = "00000000.TEL";
    LinearBuf bufTemp;
    memcpy(&bufTemp, &Global->csTelemetry.buf, sizeof(bufTemp));

    for (uint16_t i = 0; i < linearBuf_count(&Global->csTelemetry.buf); ++i) {
        telemetry_block_t block;
        linearBuf_get(&bufTemp, &block);

        if (block.epoch != epoch) {
            if (testFile != NULL) {
                FSfclose(testFile);
            }
            epoch_to_telemetry_filename(block.epoch, filename);
            testFile = FSfopen(filename, "a");
        }

        if (testFile != NULL) {
            FSfwrite(&block, sizeof(telemetry_block_t), 1, testFile);
        }
    }

    if (testFile != NULL) {
        FSfclose(testFile);
    }

    dprintf("FLUSH!! Wrote %u items\r\n", linearBuf_count(&Global->csTelemetry.buf));

    G_SET(csTelemetry.buf.tail, &old_tail);
    G_SET(csTelemetry.buf, NULL); //empty the buffer
}


void writeToMonitor(INT64 time){
    char tab = '-';
    FSFILE* file = FSfopen("TLMTIME.TST", "a");
    FSfwrite(&time, sizeof(INT64), 1, file);
    FSfwrite(&tab, sizeof(char), 1, file);
    FSfclose(file);

    dprintf("Tlm to test\r\n");

    G_SET(csTelemetry.buf, NULL); //empty the buffer
}
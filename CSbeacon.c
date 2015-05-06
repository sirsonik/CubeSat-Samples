/*
 * File:   CSbeacon.h
 * Author: Donald Eckels
 *
 * Created on February 1, 2015, 6:28 PM
 */

#include "types.h"
#include "Globals.h"
#include "metal/beacon.h"
#include "CSbeacon.h"

/*
 * beaconEnable
 * INPUT: bool enabled - set wether or not the beacon is to be enabled or not through software means
 * OUTPUT: TRUE if the software beacon status matches the new requested state,
 *         FALSE if it does not
 * INFO: The ground, or potentially even the satellite itself, needs a way of being able to enable or
 *       disable the beacon. The status of it is sent to this function, which will set it to enabled or disabled.
 *       The function returns if the status was set properly or not, so that the satellite could potentially raise
 *       anomalies if the beacon doesn't get set properly.
 */
bool beaconEnable(bool enabled){
    G_SET(csBeacon.beacon_enabled,&enabled);
    if(Global->csBeacon.beacon_enabled == enabled){
        return true;
    }
    else{
        //TODO: do we want to resolve if beacon does not properly turn on or off?
        return false;
    }
}

/*
 * beaconMsgInit
 * INPUT: none
 * OUTPUT: none
 * INFO: The beacon needs to be set to a default string and all A's was selected since
 *       it is the minimum or OK value for each.
 */
void beaconMsgInit(){
 char defaultString[31];
 uint8_t i;
 for(i = 0; i<31;i++){
     defaultString[i] = 'A';
 }
 G_SET(csBeacon.beaconMsg,defaultString);
}

/*
 * beaconMsgUpdateTelemetry
 * INPUT: none
 * OUTPUT: none
 * INFO: A majority of the beacon values are based on telemetry. The csLastTelemetry set of readings
 *       is used to update it. Since the beacon values are not in the same order as the collected telemetry
 *       and they are also not consecutive, the beacon characters are set in order. Functions are used to
 *       convert the count into the appropriate character as per the beacon requirements.
 */
void beaconMsgUpdateTelemetry(){
    char tempChar;
    dprintf("Updating beacon telemetry values:\r\n");
    tempChar = intToBeaconChar(((Global->csLastTelemetry.reading[19]) >> 7)); //12 bits, don't care about least significant 2
    G_SET(csBeacon.beaconMsg[SC_BATT_V], &tempChar);

    tempChar = intToBeaconChar(((Global->csLastTelemetry.reading[20]) >> 7)); //12 bits, don't care about least significant 2
    G_SET(csBeacon.beaconMsg[SC_BATT_A], &tempChar);

    tempChar = getTempChar(21); //temperature values need a bit more work, therefore it has its own seperate function
    G_SET(csBeacon.beaconMsg[SC_BATT_T], &tempChar);


    tempChar = intToBeaconChar(((Global->csLastTelemetry.reading[26]) >> 7)); //12 bits, don't care about least significant 2
    G_SET(csBeacon.beaconMsg[PL_BATT_V], &tempChar);

    tempChar = intToBeaconChar(((Global->csLastTelemetry.reading[27]) >> 7)); //12 bits, don't care about least significant 2
    G_SET(csBeacon.beaconMsg[PL_BATT_A], &tempChar);

    tempChar = getTempChar(28);
    G_SET(csBeacon.beaconMsg[PL_BATT_T], &tempChar);


    tempChar = intToBeaconChar(((Global->csLastTelemetry.reading[16]) >> 7)); //12 bits, don't care about least significant 2
    G_SET(csBeacon.beaconMsg[V3d3_V], &tempChar);

    tempChar = intToBeaconChar(((Global->csLastTelemetry.reading[15]) >> 7)); //12 bits, don't care about least significant 2
    G_SET(csBeacon.beaconMsg[V3d3_A], &tempChar);


    tempChar = intToBeaconChar(((Global->csLastTelemetry.reading[43]) >> 7)); //12 bits, don't care about least significant 2
    G_SET(csBeacon.beaconMsg[V3d3_SW1_V], &tempChar);

    tempChar = intToBeaconChar(((Global->csLastTelemetry.reading[42]) >> 7)); //12 bits, don't care about least significant 2
    G_SET(csBeacon.beaconMsg[V3d3_SW1_A], &tempChar);


    tempChar = intToBeaconChar(((Global->csLastTelemetry.reading[41]) >> 7)); //12 bits, don't care about least significant 2
    G_SET(csBeacon.beaconMsg[V3d3_SW2_V], &tempChar);

    tempChar = intToBeaconChar(((Global->csLastTelemetry.reading[40]) >> 7)); //12 bits, don't care about least significant 2
    G_SET(csBeacon.beaconMsg[V3d3_SW2_A], &tempChar);


    tempChar = intToBeaconChar(((Global->csLastTelemetry.reading[39]) >> 7)); //12 bits, don't care about least significant 2
    G_SET(csBeacon.beaconMsg[V3d3_SW3_V], &tempChar);

    tempChar = intToBeaconChar(((Global->csLastTelemetry.reading[38]) >> 7)); //12 bits, don't care about least significant 2
    G_SET(csBeacon.beaconMsg[V3d3_SW3_A], &tempChar);


    tempChar = intToBeaconChar(((Global->csLastTelemetry.reading[18]) >> 7)); //12 bits, don't care about least significant 2
    G_SET(csBeacon.beaconMsg[V3d3_SW4_V], &tempChar);

    tempChar = intToBeaconChar(((Global->csLastTelemetry.reading[17]) >> 7)); //12 bits, don't care about least significant 2
    G_SET(csBeacon.beaconMsg[V3d3_SW4_A], &tempChar);



    tempChar = intToBeaconChar(((Global->csLastTelemetry.reading[12]) >> 7)); //12 bits, don't care about least significant 2
    G_SET(csBeacon.beaconMsg[V5_V], &tempChar);

    tempChar = intToBeaconChar(((Global->csLastTelemetry.reading[11]) >> 7)); //12 bits, don't care about least significant 2
    G_SET(csBeacon.beaconMsg[V5_A], &tempChar);


    tempChar = intToBeaconChar(((Global->csLastTelemetry.reading[14]) >> 7)); //12 bits, don't care about least significant 2
    G_SET(csBeacon.beaconMsg[V5_SW5_V], &tempChar);

    tempChar = intToBeaconChar(((Global->csLastTelemetry.reading[13]) >> 7)); //12 bits, don't care about least significant 2
    G_SET(csBeacon.beaconMsg[V5_SW5_A], &tempChar);


    tempChar = intToBeaconChar(((Global->csLastTelemetry.reading[8]) >> 7)); //12 bits, don't care about least significant 2
    G_SET(csBeacon.beaconMsg[V12_V], &tempChar);

    tempChar = intToBeaconChar(((Global->csLastTelemetry.reading[7]) >> 7)); //12 bits, don't care about least significant 2
    G_SET(csBeacon.beaconMsg[V12_A], &tempChar);


    tempChar = intToBeaconChar(((Global->csLastTelemetry.reading[10]) >> 7)); //12 bits, don't care about least significant 2
    G_SET(csBeacon.beaconMsg[V12_SW6_V], &tempChar);

    tempChar = intToBeaconChar(((Global->csLastTelemetry.reading[9]) >> 7)); //12 bits, don't care about least significant 2
    G_SET(csBeacon.beaconMsg[V12_SW6_A], &tempChar);


    tempChar = getTempChar(24);
    G_SET(csBeacon.beaconMsg[RADIO_T], &tempChar);

}

/*
 * beaconMsgUpdateSingle
 * INPUT: beacon_msg_index_t index - enumerated names for each of the beacon characters
 *        char val - character to be written
 * OUTPUT: none
 * INFO: Enumerated index values are used so that if the beacon is reorganized any functions calling this function
 *       does not need to be rewritten.
 *       The first few characters are all set manually under certain conditions. Additionally,
 *       if the satellite is in safe hold/listen, then only a limited subset of telemetry values
 *       may be collected and updated and this function can be used to update them in the beacon appropriately.
 *       Inappropriate characters that are not in the ranges of 0-9 or A-Z  are not accepted. Beause E and T are
 *       not allowed, but potentially could just be bad character shifts, they are bumped up by one to put them
 *       into allowed values.
 *       All allowed values are then set into the provided index value.
 */
void beaconMsgUpdateSingle(beacon_msg_index_t index, char val){
    //validation of character.
    //if it isn't within the ranges of A-Z or 0-9 discard the input
    if(val < '0'){ //ASCII character with the lowest decimal value
        return;
    }
    else if(val > '9' && val < 'A'){
        return;
    }
    else if(val > 'Z'){
        return;
    }
    else if(val == 'E' || val == 'T'){ //These could potentially be minor input mistakes, just bump them up to F & U respectively
        val++;
    }

    G_SET(csBeacon.beaconMsg[index],&val);
}

/*
 * intToBeaconChar
 * INPUT: uint16_t val - numeri value to be turned into a character
 * OUTPUT: char - a character that is within acceptable beacon character ranges
 * INFO: While uint16 values are accepted, 0-33 are the only accepted and processed.
 *      The input value is converted to the appropriate character as per the beacon
 *      requirements.
 */

char intToBeaconChar(uint16_t val){
    if(val >= 34){
        dprintf("intToBeaconChar received an invalit integer input %ul\r\n", val);
        return 'A';
    }
    char ret;
    if(val < 24){ //0-23
        ret = val + 65;
        if(ret >= 'S'){ //skipping E and T
            ret += 2;
        }
        else if(ret >= 'E'){ //just skipping E
            ret++;
        }
    }
    else{ // val >= 24 && val <34
        ret = val + 24; //'0' == 48
    }

    //dprintf ("intToBeaconChar input of %ul became %c",val, ret);
    return ret;
}

/*
 * getTempChar
 * INPUT: uint8_t index - index of the temperature value within the csLastTelemetry structure
 * OUTPUT: uint8_t - technically char, which represents the temperature as per the beacon requirements
 * INFO: The index passed in is used to pull out the last count value stored for the specified temp.
 *       First it is adjusted subtracting a count of 1385, which sets the minimum that is to be looked for.
 *       If it is <= 0, then return the minimum character.
 *       After this, the 7 most signifcant bits (since all of the ADC collected telemetry are 12-bits)
 *       are used to determine the character. 0-33 are set as the specified chracter, but over 33 is also set as the max character.
 */
uint8_t getTempChar(uint8_t index){
    char tempChar;
    //get value and adjust it down by 1385
    int16_t temp = (Global->csLastTelemetry.reading[index] - 1385);
    //negative counts are all too low, set to the min
    if(temp <= 0){
        tempChar = intToBeaconChar(0);
    }
    //everything else needs step 2
    else{
        //only care about 7 most significant bits
        temp = (temp >> 5);
        //over range goes to max char
        if (temp >= 33){
            tempChar = intToBeaconChar(33);
        }
        //otherwise get the appropriate character
        else{
            tempChar = intToBeaconChar(temp);
        }
    }
    return tempChar;
}
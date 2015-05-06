/*Last Modified by Jean Korkis on 06-30-2014
 * File:   Globals.h
 * Author: Greg
 * Stable 06302014
 * Created on November 11, 2013, 11:24 AM
 *
 *
 * Very straight forward implementation. Simply create a struct in GlobalX of the file name followed by X.
 * Then declare a variable filename at the end of the struct declaration. Repeat this for functions inside the file
 * IMPORTANT Global needs to be declared as a global variable preferebly in the main file as
 * struct GlobalX Global;
 *
 * 6/25/14-3:10pm: added interrupt_counter variable to csRadio struct. JuanMoreno
 */

#ifndef GLOBALS_H
#define	GLOBALS_H

#include <stddef.h>

#include "csCRC.h"
#include "types.h"
#include "CSlinearBuf.h"
#include "CSpendingCommand.h"
#include "CSlink.h"
#include "CStimers.h"
#include "CScubesat.h"
#include "CSresponsePoll.h"

// Mark an argument as unused.
#define UNUSED __attribute__((unused))

/**
 * These enumerated types defines the possible states and substates the CubeSat can be in.
 */
typedef enum {RESET_STATE=1, STARTUP, SAFE_HOLD, COMMAND_RESPONSE, STATUS_MONITORING, ANOMALY } MainState;
typedef enum {DIAGNOSTIC_CHECK=1, ALL_QUIET, BEACON_ON, PENDING_PROCESS} StatMonState;


/*type definition of structures*/
typedef struct{
    uint16 n;
    uint16 hiVal;
    uint16 lowVal;
    uint16 avg;
    uint32 hiTime;
    uint32 lowTime;
}csSingleBasicTelemetry;

typedef struct {
    csSingleBasicTelemetry csSingleTelemetry[44]; //44
    uint32 anomalyModeTime[5];
    uint16 battRecentTemp[3];
    int16 battDeltaTemp;
    uint16 anomalyModeBasicInfo[5];
    uint8 anomalyslot;
    uint8 battSlot;
}csBasicTelemetryX;

typedef struct{
    uint16 reading[44];
}csLastTelemetryX;

// represents time since last fault and flag indicating whether we can return to
// SafeHoldListen
typedef struct {
    uint32_t faultTime;
    bool voidSHL;
}csSetProgramStateX;

typedef struct {
    struct CSlinearbufX{
        LinearBuf buf;
    }csTelemetry;

    csBasicTelemetryX csBasicTelemetry;

    csLastTelemetryX csLastTelemetry;

    sequence_t csSequence;

    response_poll_t csResponsePoll;

    struct csFlashOpX{
        unsigned int tmp,flags;
    } csFlashOp;

    struct csStateX{
        MainState mainState;
        MainState previousState;
        startup_state_t startupState;
        StatMonState statMonState;
        StatMonState statMonPrevState;
        TimerMode timerMode;
        uint8_t diagDay;
    }csState;

    cubesat_event_t csEvents;  // Events to be handled in the main loop.

    //| Records whether various one-time setup steps have been completed.
    struct csOneTimeInitX {
      bool warmedUp;         // True if the hardware has been warmed up.
      bool antennaDeployed;  // True if the antennae have already been deployed.
      bool beaconPowered;    // True if the beacon has been powered.
    } csOneTimeInit;


    //| See CSlink.h for the functions that access this struct directly.
    struct {
      link_mode_t mode;  // comms link mode -- see link_mode_t
      bool use_default_challenge;  // Use the default challenge instead of the rotating challenge pad.

      // TODO: optimize this. Maybe make "0" mean "default challenge",
      // and anything else be the index+1 into the rotating pad?
      challenge_t current_challenge;    // We store the current challenge triple here
      uint16_t current_challenge_index;  // Index of the last rotating challenge sent to ground.

      // State held by commands during streaming.
      struct {
        opcode_t opcode;  // Which command is currently executing? (0 means none)
        union {
            struct {
                FSFILE* handle;
                uint32_t bytesLeft;
            } getFile;

            struct {
                UINT32 epochStart;
                UINT32 epochEnd;
                UINT32 epochStartCur;
                UINT32 epochEndCur;
                UINT8  offset;
                UINT32 fileOffset;
                UINT8  useFileOffset;
                UINT8  overFlow;
                UINT8  num_4Bits;
                UINT8  bufferBits;
            } csGetTelemetryStream;
    
        };
      } csStreamState;
    } csLink;

    CallbackFunction TimeoutCallback;

    csSetProgramStateX csSetProgramState;

    struct csRadioX{
        //@todo fragment_array and complete_packet should be combined once
        //      the code is verified working

        char in_payload[566];        // Hold the current incomming payload. 255 bytes for fragment + 18 bytes for AX.25 header and FCS + 10 Bytes for Radio header and chksum.
                                     // Possibly double for two packets coming in at once.
        int  number_of_fragments[8]; // Hold the total number of fragments, according to each packet that is sent
        int  fragment_id[8];         // Hold the fragment ID that was received
        int  command_id[8];          // hold the command ID according to the received packet
        int  packet_length[8];       // Hold the length of each packet

       // int  fragment_type[8];
       // int  fragment_length[8];

        char complete_packet[2056]; // Array to assemble the complete packet for demo2
        int  complete_packet_size;

        uint8  interrupt_counter;     //Counter to keep track of how many times interrupt 1 is called(number of packets coming in)
                                    //this will allow us to check if there are any more packets at the end of the radioServiceRoutine.
    } csRadio;

    struct {
        bool beacon_enabled;          //Defines if the beacon is enabled when the radio link is not enabled
        char beaconMsg[31];
    } csBeacon;

    struct {
        BYTE switchStatePCA_1;         //Holder of the correct state of the pins on PCA_1 -- Virtual//
        BYTE switchStatePCA_2;         //Holder of the correct state of the pins on PCA_2//
        BYTE switchStatePCA_3;         //Holder of the correct state of the pins on PCA_3//
        BYTE switchStatePCA_4;         //Holder of the correct state of the pins on PCA_4 -- JPL//
    } csI2C;

} GlobalX;

extern GlobalX* const Global;

typedef uint16_t global_ptr_t;  // An offset into a GlobalX.


/**
 * Initializes global state.
 */
void Global_Init();

/**
 * Checks the global data structure against its other two counterparts and makes
 * each of them hold the same data. If there are any discrepencies bewteen them,
 * it performs a sort of "vote" to decide which data to use.
 */
void SettleGlobal();

/**
 * Writes `size` bytes from `src` into the Global field at `offset`.
 * This is basically just a memcpy wrapper which makes all globals the same
 * beforehand and keeps them the same for the copy. Only perform this operation
 * if `offset` is not greater than sizeof(GlobalX), and the field at `offset`
 * is large enough to contain `size` bytes.
 *
 * @param offset Offset in bytes from &Global to the desired field.
 * @param src Pointer to the source data.
 * @param size Size (in bytes) of data to copy.
 * @return BOOL TRUE if the variable was set.
 */
BOOL globalMod(size_t offset, void const* src, size_t size);

// Convenience macro for getting the offset of a member of GlobalX.
#define G_OFFSET(dest) ((global_ptr_t)offsetof(GlobalX, dest))

// Convenience macro which copies `byte_len` bytes from `src` to the `dest` global.
#define G_CPY(dest, src, byte_len) globalMod(G_OFFSET(dest), (src), (byte_len))

// Convenience macro which assumes the `src` buffer has the same byte-size as `dest`.
#define G_SET(dest, src) G_CPY(dest, src, sizeof(((GlobalX*)NULL)->dest))

#endif	/* GLOBALS_H */

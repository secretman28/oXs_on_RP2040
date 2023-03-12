#include "tools.h"
#include "pico/stdlib.h"
#include "config.h"
#include "stdio.h"
#include "pico/util/queue.h"
#include "pico/multicore.h"
#include "math.h"
#include "sport.h"
#include "ms4525.h"
#include "sdp3x.h"

extern queue_t qSensorData; 



// useful for button but moved to button2.cpp
/////////////////////////////////////////////////////////////////
// Added by Mstrens to be able to get state of boot button on rp2040
#include "hardware/sync.h"
#include "hardware/structs/ioqspi.h"
#include "hardware/structs/sio.h"
// Picoboard has a button attached to the flash CS pin, which the bootrom
// checks, and jumps straight to the USB bootcode if the button is pressed
// (pulling flash CS low). We can check this pin in by jumping to some code in
// SRAM (so that the XIP interface is not required), floating the flash CS
// pin, and observing whether it is pulled low.
//
// This doesn't work if others are trying to access flash at the same time,
// e.g. XIP streamer, or the other core.

bool __no_inline_not_in_flash_func(get_bootsel_button)() {
    const uint CS_PIN_INDEX = 1;
    //startTimerUs(0);
    multicore_lockout_start_blocking();
    // Must disable interrupts, as interrupt handlers may be in flash, and we
    // are about to temporarily disable flash access!
    uint32_t flags = save_and_disable_interrupts();

    // Set chip select to Hi-Z
    hw_write_masked(&ioqspi_hw->io[CS_PIN_INDEX].ctrl,
                    GPIO_OVERRIDE_LOW << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
                    IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);

    // Note we can't call into any sleep functions in flash right now
    //for (volatile int i = 0; i < 1000; ++i);

    // The HI GPIO registers in SIO can observe and control the 6 QSPI pins.
    // Note the button pulls the pin *low* when pressed.
    bool button_state = !(sio_hw->gpio_hi_in & (1u << CS_PIN_INDEX));

    // Need to restore the state of chip select, else we are going to have a
    // bad time when we return to code in flash!
    hw_write_masked(&ioqspi_hw->io[CS_PIN_INDEX].ctrl,
                    GPIO_OVERRIDE_NORMAL << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
                    IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);

    restore_interrupts(flags);
    multicore_lockout_end_blocking();
    //getTimerUs(0);
    return button_state;
}

int32_t int_round(int32_t n, uint32_t d)
{
    if (d <= 0) return n;
    int32_t offset;
    offset = ((n >= 0) ? d : -(int32_t) d) ;
    offset = offset >>1; 
    //printf("n=%d   d=%d   offset %d  return %d\n", n , d , offset, (n+offset)/ (int32_t)d);
    return (n + offset) / (int32_t) d;
}


uint32_t millisRp(){
    return  to_ms_since_boot( get_absolute_time());
}

uint32_t microsRp() {
    return  to_us_since_boot(get_absolute_time ());
}

void waitUs(uint32_t delayUs){
    uint32_t nowUs = microsRp();
    while (( microsRp() - nowUs) < delayUs) {microsRp();}
}

void enlapsedTime(uint8_t idx){
    static uint32_t prevTime[10] = {0};
    uint32_t currTime;
    if (idx >= sizeof(prevTime)) return ;
    currTime = microsRp() ;
    printf("Eus%d=%d\n", idx , currTime-prevTime[idx]);
    prevTime[idx]=currTime;
}

uint32_t startAtUs[10] = {0};
void startTimerUs(uint8_t idx){
    if (idx >= sizeof(startAtUs)) return ;
    startAtUs[idx] = microsRp() ;
}

void getTimerUs(uint8_t idx){
    if (idx >= sizeof(startAtUs)) return ;
    printf("FSus %d= %d\n", idx , microsRp()-startAtUs[idx]);
}

void sent2Core0( uint8_t fieldType, int32_t value){
    queue_entry_t entry;
    entry.type = fieldType;
    entry.data = value ;
    queue_try_add(&qSensorData, &entry);
    //printf("sending %d = %10.0f\n", entry.type , (float) entry.data);
}


float difPressureAirspeedSumPa = 0 ; // calculate a moving average on x values
uint32_t difPressureAirspeedCount = 0 ;
float difPressureCompVspeedSumPa = 0 ; // calculate a moving average on x values
uint32_t difPressureCompVspeedCount = 0 ;
float temperatureKelvin;
uint32_t prevAirspeedCalculatedUs;
uint32_t prevAirspeedAvailableMs;
//uint32_t prevCompVspeedCalculatedUs;
//uint32_t prevCompVspeedAvailableMs;
float smoothAirspeedCmS;
extern MS4525 ms4525;
extern SDP3X sdp3x;
extern float actualPressurePa;


void calculateAirspeed(){
    if (ms4525.airspeedInstalled == false && sdp3x.airspeedInstalled == false ) return; // skip if no sensor installed
    uint32_t nowUs = microsRp(); 
    if ( ( nowUs - prevAirspeedCalculatedUs) < 20000 ) return; // skip if there is less than 20 msec
    prevAirspeedCalculatedUs = nowUs;
    float difPressureAvg = difPressureAirspeedSumPa / difPressureAirspeedCount ; // calculate a moving average on x values
    difPressureAirspeedSumPa = 0 ;  // reset 
    difPressureAirspeedCount = 0 ;
    if ( difPressureAvg < 0 ) difPressureAvg = 0;
    // calculate airspeed based on pressure, altitude and temperature
    // airspeed (m/sec) = sqr(2 * differential_pressure_in_Pa / air_mass_kg_per_m3) 
    // air_mass_kg_per_m3 = pressure_in_pa / (287.05 * (Temp celcius + 273.15))
    // so airspeed m/sec =sqr( 2 * 287.05 * differential_pressure_pa * (temperature Celsius + 273.15) / pressure_in_pa )
    // rawAirSpeed cm/sec =  23,96 * 100 * sqrt( (float) abs(smoothDifPressureAdc) * temperature4525  /  actualPressurePa) ); // in cm/sec ;
    // actual pressure must be in pa (so 101325 about at sea level)
    
    //#ifdef AIRSPEED_AT_SEA_LEVEL_AND_15C
    //smoothAirSpeed =  131.06 * sqrt( (float) ( abs_smoothDifPressureAdc ) ); // indicated airspeed is calculated at 15 Celsius and 101325 pascal
    float rawAirspeedPa = 2396 *  sqrt( difPressureAvg * temperatureKelvin / actualPressurePa );
    
    #define EXPOSMOOTH_AIRSPEED_FACTOR 0.1
    smoothAirspeedCmS += ( EXPOSMOOTH_AIRSPEED_FACTOR * ( rawAirspeedPa - smoothAirspeedCmS )) ; 
    // publish the new value every 200 ms
    if ( (millisRp() - prevAirspeedAvailableMs) > 200) { // make the new value available once per 200 msec
        prevAirspeedAvailableMs = millisRp();
        //if ( smoothAirSpeedCmS >  0) {  // normally send only if positive and greater than 300 cm/sec , otherwise send 0 but for test we keep all values to check for drift  
        sent2Core0(AIRSPEED, (int32_t) smoothAirspeedCmS); 
    }
} 
// check if offset must be reset
//              if (airSpeedData.airspeedReset) { // adjust the offset if a reset command is received from Tx
//                    offset4525 =  offset4525  + smoothDifPressureAdc ;
//                    airSpeedData.airspeedReset = false ; // avoid that offset is changed again and again if PPM do not send a command
//              }

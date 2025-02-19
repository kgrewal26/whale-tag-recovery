/*
 * state_machine.c
 *
 *  Created on: Aug 22, 2023
 *      Author: Kaveet
 */

#include "Lib Inc/state_machine.h"
#include "Lib Inc/threads.h"
#include "main.h"

//Event flags for signaling changes in state
TX_EVENT_FLAGS_GROUP state_machine_event_flags_group;

//Threads array
extern Thread_HandleTypeDef threads[NUM_THREADS];

void state_machine_thread_entry(ULONG thread_input){

	//Event flags for triggering state changes
	tx_event_flags_create(&state_machine_event_flags_group, "State Machine Event Flags");

	//If simulating, set the simulation state defined in the header file, else, enter data capture as a default
	State state = (IS_SIMULATING) ? SIMULATING_STATE : STATE_WAITING;

	//Check the initial state and start in the appropriate state
	if (state == STATE_CRITICAL){
		enter_critical();
    } else if (state == STATE_WAITING){
		enter_waiting();
	} else if (state == STATE_APRS){
		enter_waiting();
		enter_aprs_recovery();
	} else if (state == STATE_GPS_COLLECT){
		enter_waiting();
		enter_gps_collection();
	}

	//Enter main thread execution loop ONLY if we arent simulating
	if (!IS_SIMULATING){
		while (1){

			ULONG actual_flags;
			tx_event_flags_get(&state_machine_event_flags_group, ALL_STATE_FLAGS, TX_OR_CLEAR, &actual_flags, TX_WAIT_FOREVER);

			//Received STOP command from Pi
			if (actual_flags & STATE_COMMS_STOP_FLAG){

				//Enter waiting mode (stop APRS or GPS collect)
				if (state == STATE_APRS){
					exit_aprs_recovery();
				}
				else if (state == STATE_GPS_COLLECT){
					exit_gps_collection();
				}

				state = STATE_WAITING;
			}

			//Received Start APRS command from Pi
			if (actual_flags & STATE_COMMS_APRS_FLAG){

				//Start APRS recovery. Stop GPS collection if its currently running (since only one thread should access GPS hardware)
				if (state == STATE_GPS_COLLECT){
					exit_gps_collection();
				}

				enter_aprs_recovery();

				state = STATE_APRS;
			}

			//Received Start GPS data collection from Pi
			if (actual_flags & STATE_COMMS_COLLECT_GPS_FLAG){

				//Start GPS collection. Stop APRS collection if its currently running (since only one thread should access GPS hardware)
				if (state == STATE_GPS_COLLECT){
					exit_aprs_recovery();
				}

				enter_gps_collection();

				state = STATE_GPS_COLLECT;
			}

			//Received Critically low battery flag from Battery Monitor
			if (actual_flags & STATE_CRITICAL_LOW_BATTERY_FLAG){

				//Exit everything and enter low power mode (idle thread)
				enter_critical();

				state = STATE_CRITICAL;
			}
		}
	}
}


//Starts the APRS recovery thread
void enter_aprs_recovery(){

	tx_thread_resume(&threads[APRS_THREAD].thread);

	//TODO: Enable Power FET to turn GPS on
}

//Suspends the APRS recovery thread
void exit_aprs_recovery(){

	tx_thread_suspend(&threads[APRS_THREAD].thread);

	//TODO: Turn GPS off (through power FET)
}

//Starts the GPS data collection thread
void enter_gps_collection(){

	tx_thread_resume(&threads[GPS_COLLECTION_THREAD].thread);

	//TODO: Enable power FET to turn GPS on
}

//Exit GPS data collection thread
void exit_gps_collection(){

	tx_thread_suspend(&threads[GPS_COLLECTION_THREAD].thread);

	//TODO: Turn GPS off (through power FET)
}

//Starts the threads that are active while waiting (comms, battery monitoring)
void enter_waiting(){

	tx_thread_resume(&threads[BATTERY_MONITOR_THREAD].thread);
	tx_thread_resume(&threads[PI_COMMS_TX_THREAD].thread);
	tx_thread_resume(&threads[PI_COMMS_RX_THREAD].thread);
}

//Exits the waiting threads
void exit_waiting(){

	tx_thread_suspend(&threads[BATTERY_MONITOR_THREAD].thread);
	tx_thread_suspend(&threads[PI_COMMS_TX_THREAD].thread);
	tx_thread_suspend(&threads[PI_COMMS_RX_THREAD].thread);
}

//Enters critical low power state (everything off). Once we enter this, we cant exit it.
void enter_critical(){

	//Exit everything
	exit_aprs_recovery();
	exit_gps_collection();
	exit_waiting();
}

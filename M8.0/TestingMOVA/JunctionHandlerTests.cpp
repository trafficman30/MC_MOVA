#include "stdafx.h"
#include "CppUnitTest.h"

#include <string.h>
#include <stdlib.h>

#include "common_data.h"
#include "DataManagerForTesting.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace MOVATest
{
	TEST_CLASS(JunctionHandlerTests)
	{
	public:

		///////////////////   Setup and Cleanup   /////////////////////
		TEST_CLASS_INITIALIZE(Init_JunctionHandlerTests) {	}

		TEST_CLASS_CLEANUP(CleaningUp_JunctionHandlerTests) {}

		TEST_METHOD_INITIALIZE(will_be_called_before_any_test_mesthod)
		{
			DataManagerForTesting::ResetingMovaDataSet();
			DataManagerForTesting::ResetingMovaDynamicData();
		}
		////////////////////////////////////////////////////////////////
		

		/** \test TEST CASE: handle_junction_end_of_stage_updates_TC01 \n
				  INPUT(S):
							- Work on stage number 1;
							- Not in the initialisation stage;
							- Not in warmup;
							- 20 msec passed in that stage;
							- No ped demand.	*/
		TEST_METHOD(handle_junction_end_of_stage_updates_TC01)
		{
			/*Inputs*/
			
			junction.warmup_counter = 2; //Not in warmup (as number of stages = 1)
			stages.lambda[0] = 3;
			junction.previous_stage = 1;
			timers.stage_cycle_timer[0] = 20; //Any number > 0
			junction.ped_demand_marker = 0;  //No ped demand

			//inputs related to the function: update_skipped_stages_data_at_endstage();
			DI_set_stages_count(1);

			//inputs related to the function: update_stage_lambdas(...)
			timers.signal_state_timer = 5;
			
			//input for the function: calc_stages_drift_max
			DI_set_total_green_time(140);
			junction.smoothed_cycle_time = 120;
			

			/*Calling the function UT*/
			handle_junction_end_of_stage_updates();

			/*Checking the output(s)*/
			Assert::AreEqual<int32>(9, stages.lambda[0]);
			Assert::AreEqual<int32>(6, stages.alt_lambda[0]);
			Assert::AreEqual<int32>(0, timers.stage_cycle_timer[0]);
			Assert::AreEqual<int32>(96, junction.smoothed_cycle_time);
			Assert::AreEqual<int32>(9, junction.total_lambda);
			Assert::AreEqual<int32>(190, stages.drift_max[0]);
			Assert::AreEqual<uint8>(1, junction.last_stage);
			Assert::AreEqual<int32>(0, junction.var_min_green_time);
			Assert::AreEqual<int32>(0, timers.signal_state_timer);
			Assert::AreEqual<int32>(0, timers.wait_stage_change_timer);
			Assert::AreEqual<int32>(0, timers.stage_max_green_timer);
			Assert::AreEqual<int32>(0, timers.ep_ext_max_timer);
		}

		/** \test TEST CASE: handle_junction_end_of_stage_updates_TC02 \n
				  INPUT(S):
							- Work on stage number 1;
							- Not in the initialisation stage;
							- Not in warmup;
							- No time has passed in that stage;
							- No ped demand. */
		TEST_METHOD(handle_junction_end_of_stage_updates_TC02)
		{
			/*Inputs*/
			
			junction.warmup_counter = 2; //Not in warmup (as number of stages = 1)
			stages.lambda[0] = 3;
			junction.previous_stage = 1;
			timers.stage_cycle_timer[0] = 0; //no time has passed
			junction.ped_demand_marker = 0;  //No ped demand

			//inputs related to the function: update_skipped_stages_data_at_endstage();
			DI_set_stages_count(1);

			//inputs related to the function: update_stage_lambdas(...)
			timers.signal_state_timer = 5;
			
			//input for the function: calc_stages_drift_max
			DI_set_total_green_time(140);
			junction.smoothed_cycle_time = 120;
			

			/*Calling the function UT*/
			handle_junction_end_of_stage_updates();

			/*Checking the output(s)*/
			Assert::AreEqual<int32>(3, junction.total_lambda);
			Assert::AreEqual<int32>(3, stages.lambda[0]);
			Assert::AreEqual<int8>(0, stages.endsat_marker[0]);
			Assert::AreEqual<int32>(0, stages.drift_max[0]);
			Assert::AreEqual<int8>(STAGE_DEMAND_NO_DEMAND, stages.demand_marker[0]);
			Assert::AreEqual<int8>(EMERGENCY_DEMAND_NONE, stages.emergency_demand_marker[0]);
			Assert::AreEqual<int8>(PRIORITY_DEMAND_NONE, stages.priority_demand_marker[0]);
			Assert::AreEqual<uint8>(1, junction.last_stage);
			Assert::AreEqual<int32>(0, junction.var_min_green_time);
			Assert::AreEqual<int32>(0, timers.signal_state_timer);
			Assert::AreEqual<int32>(0, timers.wait_stage_change_timer);
			Assert::AreEqual<int32>(0, timers.stage_max_green_timer);
			Assert::AreEqual<int32>(0, timers.ep_ext_max_timer);
		}


		/** \test TEST CASE: handle_junction_end_of_stage_updates_TC03 \n
				  INPUT(S):
							- Work on stage number 1;
							- Not in the initialisation stage;
							- Not in warmup;
							- 20 msec passed in that stage; 
							- A ped demand exists*/
		TEST_METHOD(handle_junction_end_of_stage_updates_TC03)
		{
			/*Inputs*/
			junction.warmup_counter = 2; //Not in warmup (as number of stages = 1)
			stages.lambda[0] = 3;
			junction.previous_stage = 1;
			timers.stage_cycle_timer[0] = 20;
			junction.ped_demand_marker = 1;  //Any number > 0

			//inputs related to the function: update_skipped_stages_data_at_endstage();
			DI_set_stages_count(1);

			//inputs related to the function: update_stage_lambdas(...)
			timers.signal_state_timer = 5;
			
			//input for the function: calc_stages_drift_max
			DI_set_total_green_time(140);
			junction.smoothed_cycle_time = 120;
			

			/*Calling the function UT*/
			handle_junction_end_of_stage_updates();

			/*Checking the output(s)*/
			Assert::AreEqual<int32>(3, junction.total_lambda);
			Assert::AreEqual<int32>(3, stages.lambda[0]);
			Assert::AreEqual<int8>(0, stages.endsat_marker[0]);
			Assert::AreEqual<int32>(0, stages.drift_max[0]);
			Assert::AreEqual<int8>(STAGE_DEMAND_NO_DEMAND, stages.demand_marker[0]);
			Assert::AreEqual<int8>(EMERGENCY_DEMAND_NONE, stages.emergency_demand_marker[0]);
			Assert::AreEqual<int8>(PRIORITY_DEMAND_NONE, stages.priority_demand_marker[0]);
			Assert::AreEqual<uint8>(1, junction.last_stage);
			Assert::AreEqual<int32>(0, junction.var_min_green_time);
			Assert::AreEqual<int32>(0, timers.signal_state_timer);
			Assert::AreEqual<int32>(0, timers.wait_stage_change_timer);
			Assert::AreEqual<int32>(0, timers.stage_max_green_timer);
			Assert::AreEqual<int32>(0, timers.ep_ext_max_timer);
		}

		/** \test TEST CASE: handle_junction_end_of_stage_updates_TC04 \n
				  INPUT(S):
							- Work on stage number 1;
							- Not in the initialisation stage;
							- In warmup;
							- 20 msec passed in that stage; 
							- No ped demand. */
		TEST_METHOD(handle_junction_end_of_stage_updates_TC04)
		{
			/*Inputs*/
			junction.warmup_counter = 1; //Not in warmup but after initialisation
			stages.lambda[0] = 3;
			junction.previous_stage = 1;
			timers.stage_cycle_timer[0] = 0; //no time has passed
			junction.ped_demand_marker = 0;

			//inputs related to the function: update_skipped_stages_data_at_endstage();
			DI_set_stages_count(1);

			//inputs related to the function: update_stage_lambdas(...)
			timers.signal_state_timer = 5;
			
			//input for the function: calc_stages_drift_max
			DI_set_total_green_time(140);
			junction.smoothed_cycle_time = 120;
			

			/*Calling the function UT*/
			handle_junction_end_of_stage_updates();

			/*Checking the output(s)*/
			Assert::AreEqual<int32>(3, junction.total_lambda);
			Assert::AreEqual<int32>(3, stages.lambda[0]);
			Assert::AreEqual<int8>(0, stages.endsat_marker[0]);
			Assert::AreEqual<int32>(0, stages.drift_max[0]);
			Assert::AreEqual<int8>(STAGE_DEMAND_NO_DEMAND, stages.demand_marker[0]);
			Assert::AreEqual<int8>(EMERGENCY_DEMAND_NONE, stages.emergency_demand_marker[0]);
			Assert::AreEqual<int8>(PRIORITY_DEMAND_NONE, stages.priority_demand_marker[0]);
			Assert::AreEqual<uint8>(1, junction.last_stage);
			Assert::AreEqual<int32>(0, junction.var_min_green_time);
			Assert::AreEqual<int32>(0, timers.signal_state_timer);
			Assert::AreEqual<int32>(0, timers.wait_stage_change_timer);
			Assert::AreEqual<int32>(0, timers.stage_max_green_timer);
			Assert::AreEqual<int32>(0, timers.ep_ext_max_timer);
		}

		/** \test TEST CASE: handle_junction_end_of_stage_updates_TC05 \n
				  INPUT(S):
							- Work on stage number 1;
							- Not in the initialisation stage;
							- Not in warmup;
							- 20 msec passed in that stage; 
							- A ped demand exists
							- PEDMAX(2) = 100
							- The junction is oversaturated	*/
		TEST_METHOD(handle_junction_end_of_stage_updates_TC05)
		{
			/*Inputs*/
			DI_set_ped_max_2(1, 100);

			junction.warmup_counter = 2; //Not in warmup (as number of stages = 1)
			stages.lambda[0] = 3;
			junction.previous_stage = 1;
			timers.stage_cycle_timer[0] = 20;
			junction.ped_demand_marker = 1;  //Any number > 0
			junction.oversat_counter = 2; //To make it oversat junction


			//inputs related to the function: update_skipped_stages_data_at_endstage();
			DI_set_stages_count(1);

			//inputs related to the function: update_stage_lambdas(...)
			timers.signal_state_timer = 5;
			
			//input for the function: calc_stages_drift_max
			DI_set_total_green_time(140);
			junction.smoothed_cycle_time = 120;
			

			/*Calling the function UT*/
			handle_junction_end_of_stage_updates();

			/*Checking the output(s)*/
			Assert::AreEqual<int32>(9, stages.lambda[0]);
			Assert::AreEqual<int32>(6, stages.alt_lambda[0]);
			Assert::AreEqual<int32>(0, timers.stage_cycle_timer[0]);
			Assert::AreEqual<int32>(96, junction.smoothed_cycle_time);
			Assert::AreEqual<int32>(9, junction.total_lambda);
			Assert::AreEqual<int32>(190, stages.drift_max[0]);
			Assert::AreEqual<uint8>(1, junction.last_stage);
			Assert::AreEqual<int32>(0, junction.var_min_green_time);
			Assert::AreEqual<int32>(0, timers.signal_state_timer);
			Assert::AreEqual<int32>(0, timers.wait_stage_change_timer);
			Assert::AreEqual<int32>(0, timers.stage_max_green_timer);
			Assert::AreEqual<int32>(0, timers.ep_ext_max_timer);
		}

		/** \test TEST CASE: handle_junction_end_of_stage_updates_TC06 \n
				  INPUT(S):
							- Work on stage number 1 and lane number 1;
							- Not in the initialisation stage;
							- Not in warmup;
							- 20 msec passed in that stage; 
							- A ped demand exists
							- PEDMAX(2) != 100
							- Unsaturated link
							*/
		TEST_METHOD(handle_junction_end_of_stage_updates_TC06)
		{
			/*Inputs*/
			DI_set_ped_max_1(1, 60);
			DI_set_ped_max_2(1, 50);

			junction.warmup_counter = 2; //Not in warmup (as number of stages = 1)
			stages.lambda[0] = 3;
			junction.previous_stage = 1;
			timers.stage_cycle_timer[0] = 20;
			junction.ped_demand_marker = 1;  //Any number > 0
			timers.signal_state_timer = 45;

			//inputs for the function: calc_stage_revised_max_green_time(...)
			DI_set_links_count(1);
			DI_set_link_lanes_count(1, 1);
			DI_set_lane_num(1, 0, 1);
			DI_set_link_green_status(1, 1, LINK_STATUS_GREEN_IN_SEVERAL_STAGES);
			DI_set_stage_max_green_time(1, 70);
			lanes.oversat_counter[0] = 3; //this will make 'max_oversat_counter' to be set to 3
				
			//inputs related to the function: update_skipped_stages_data_at_endstage();
			DI_set_stages_count(1);
									
			//inputs for the function: calc_stages_drift_max
			DI_set_total_green_time(140);
			junction.smoothed_cycle_time = 120;
			
			/*Calling the function UT*/
			handle_junction_end_of_stage_updates();

			/*Checking the output(s)*/
			Assert::AreEqual<int32>(59, stages.lambda[0]);
			Assert::AreEqual<int32>(56, stages.alt_lambda[0]);
			Assert::AreEqual<int32>(96, junction.smoothed_cycle_time);
			Assert::AreEqual<int32>(0, timers.stage_cycle_timer[0]);
			Assert::AreEqual<int32>(59, junction.total_lambda);
			Assert::AreEqual<int32>(245, stages.drift_max[0]);			
			Assert::AreEqual<uint8>(1, junction.last_stage);
			Assert::AreEqual<int32>(0, junction.var_min_green_time);
			Assert::AreEqual<int32>(0, timers.signal_state_timer);
			Assert::AreEqual<int32>(0, timers.wait_stage_change_timer);
			Assert::AreEqual<int32>(0, timers.stage_max_green_timer);
			Assert::AreEqual<int32>(0, timers.ep_ext_max_timer);
		}



		////////////////////////////////////////////////////////////////////////////////////
		////////////////////   handle_junction_platoon_checking   //////////////////////////
		////////////////////////////////////////////////////////////////////////////////////
		

		/** \test TEST CASE: handle_junction_platoon_checking_TC01 \n
			  check if there is a platoon in lane number 1.*/
		TEST_METHOD(handle_junction_platoon_checking_TC01)
		{
			mv_bool res;

			/*Inputs*/
			DI_set_stages_count(1);
			DI_set_links_count(1);
			DI_set_link_lanes_count(1, 1);
			DI_set_lane_num(1, 0, 1);
			DI_set_det_num(1, X_DET, 30);

			DI_set_lane_critical_gap(1, 50); //5 secs
			DI_set_lane_portion_of_critical_gap(1, 50); //50% of CRITG
			
			dets.old_gap[29] =  20; //where 29 is the X-DET index, and 20 is a number < 25 (50 x 50/100)
			dets.time_off[29] = 21; //where 29 is the X-DET index, and 21 is a number < 25 (50 x 50/100)

			links.green_flag[0] = LINK_FLAG_GREEN;
			links.endsat_marker[0] = LINK_ENDSAT_NORMAL;

			junction.current_stage = 1; //The current stage number is: 1
			junction.next_stage = 2;    //The next stage number is: 2

			stages.endsat_marker[1] = STAGE_ENDSAT_REACHED_FOR_ALL_REL_LINKS; //setting that for the next stage (number 2)

			/*Calling the function UT*/
			res = handle_junction_platoon_checking();

			/*Checking the output*/
			Assert::AreEqual<int>(mv_true, res);
			Assert::AreEqual<uint32>(LINK_ENDSAT_NOT_YET, links.endsat_marker[0]);

		}

		/** \test TEST CASE: handle_junction_platoon_checking_TC02 \n
			  Check if there is no platoon in lane number 1.*/
		TEST_METHOD(handle_junction_platoon_checking_TC02)
		{
			mv_bool res;

			/*Inputs*/
			DI_set_stages_count(1);
			DI_set_links_count(1);
			DI_set_link_lanes_count(1, 1);
			DI_set_lane_num(1, 0, 1);
			DI_set_det_num(1, X_DET, 30);
			
			DI_set_lane_critical_gap(1, 50); //5 secs
			DI_set_lane_portion_of_critical_gap(1, 50); //50% of CRITG
			
			dets.old_gap[29] =  20; //where 29 is the X-DET index, and 20 is a number < 25 (50 x 50/100)
			dets.time_off[29] = 26; //where 29 is the X-DET index, and 26 is a number > 25 (50 x 50/100)

			links.green_flag[0] = LINK_FLAG_GREEN;
			links.endsat_marker[0] = LINK_ENDSAT_NORMAL;

			junction.current_stage = 1; //The current stage number is: 1
			junction.next_stage = 2;    //The next stage number is: 2

			stages.endsat_marker[1] = STAGE_ENDSAT_REACHED_FOR_ALL_REL_LINKS; //setting that for the next stage (number 2)

			/*Calling the function UT*/
			res = handle_junction_platoon_checking();

			/*Checking the output*/
			Assert::AreEqual<int>(mv_false, res);
			Assert::AreEqual<uint32>(LINK_ENDSAT_NORMAL, links.endsat_marker[0]);
		}

	};
}
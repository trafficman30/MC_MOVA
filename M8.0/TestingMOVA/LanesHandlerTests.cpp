#include "stdafx.h"
#include "CppUnitTest.h"

#include <string.h>
#include <stdlib.h>
#include <algorithm>


#include "common_data.h"
#include "DataManagerForTesting.h"

	
using namespace Microsoft::VisualStudio::CppUnitTestFramework;

//char str[20];
//_itoa( x , str, 10);
//Logger::WriteMessage(str);

namespace MOVATest
{
	TEST_CLASS(LanesHandlerTests)
	{
	public:

		///////////////////   Setup and Cleanup   /////////////////////
		TEST_CLASS_INITIALIZE(Init_LanesHandlerTests) {	}

		TEST_CLASS_CLEANUP(CleaningUp_LanesHandlerTests) {}

		TEST_METHOD_INITIALIZE(will_be_called_before_any_test_mesthod)
		{
			DataManagerForTesting::ResetingMovaDataSet();
			DataManagerForTesting::ResetingMovaDynamicData();

		}
		////////////////////////////////////////////////////////////////
		

		/** \test TEST CASE: update_lane_oversat_counters_TC01 \n
				  INPUT(S):
							- Work on link number 1, lane number 1 and stage number 1 (index = 0);
							- Not a CMOVA lane;
							- Not a short lane;
							- IN-DET to be used to check oversat;
							- Cycle time is still before the oversat checking time; 
							- IN-DET is not suspected; and
							- IN-DET ON time is larger than' det_min_on_time_for_queue'. */
		TEST_METHOD(update_lane_oversat_counters_TC01)
		{
			/*Inputs*/
			DI_set_det_num(1, IN_DET, 22); //To make it not a CMOVA lane by assiging an IN-DET number 22 
			DI_set_oversat_det(1, OVERSAT_DET_IN_DET);
			DI_set_in_det_cruise_time(1, 95);
			DI_set_oversat_critical_veh_count(1, 5); //this is to check it after executing the function UT
			timers.cycle_time_timer[0] = 12; //12 is any number < 95*10
			dets.time_on[21] = 40; //21 is the IN-DET index. 40 is any number > 0
			set_det_suspicion_status(22, DET_FAULT_STATUS_OK); 
			
			//The following inputs are related to the function: update_stages_oversat_markers(...)
			DI_set_stages_count(1);
			DI_set_link_green_status(1, 1, LINK_STATUS_GREEN_IN_ONE_STAGE); //any value other than 0 or 4
			lanes.oversat_counter[0] = 1; // any number > 0

			/*Calling the function UT*/
			update_lane_oversat_counters(0,0);

			/*Checking the output(s)*/
			Assert::AreEqual<int32>(1, lanes.oversat_counter[0]); //In this scenario, its value will not be changed
			Assert::AreEqual<int32>(5, lanes.oversat_init_count[0]);
			Assert::AreEqual<int32>(0, stages.oversat_weighting_factor[0]); //because temp_factor = 0 in this case
			Assert::AreEqual<int32>(1, stages.oversat_max_counter[0]);
			Assert::AreEqual<int32>(1, junction.max_lane_oversat_counter);
		}

		

		/** \test TEST CASE: update_lane_oversat_counters_TC01 \n
				  INPUT(S):
							- Work on link number 1, lane number 1 and stage number 1 (index = 0);
							- Not a CMOVA lane;
							- Not a short lane;
							- IN-DET to be used to check oversat;
							- Cycle time is still before the oversat checking time; 
							- IN-DET is SUSPECTED; and
							- IN-DET ON time is larger than' det_min_on_time_for_queue'. 

					NOTE: This test case is no longer valid after implementing M8 new feature 1.6. */
		BEGIN_TEST_METHOD_ATTRIBUTE(update_lane_oversat_counters_TC02)
			TEST_IGNORE() 
		END_TEST_METHOD_ATTRIBUTE()		
		TEST_METHOD(update_lane_oversat_counters_TC02)
		{
			/*Inputs*/
			DI_set_det_num(1, IN_DET, 22); //To make it not a CMOVA lane by assiging an IN-DET number 22 
			DI_set_oversat_det(1, OVERSAT_DET_IN_DET);
			DI_set_in_det_cruise_time(1, 95);
			DI_set_oversat_critical_veh_count(1, 5); //this is to check it after executing the function UT
			DI_set_lane_weighting_factor(1, 1);
			
			timers.cycle_time_timer[0] = 12; //12 is any number < 95*10
			dets.time_on[21] = 40; //21 is the IN-DET index. 40 is any number > 0
			set_det_suspicion_status(22, DET_FAULT_STATUS_SUSPECTED_DUE_TO_LONG_OFF); //any suspicious status
			junction.oversat_counter = 2;

			//The following inputs are related to the function: update_stages_oversat_markers(...)
			DI_set_stages_count(1);
			DI_set_link_green_status(1, 1, LINK_STATUS_GREEN_IN_ONE_STAGE); //any value other than 0 or 4
			lanes.oversat_counter[0] = 1; // any number > 0

			/*Calling the function UT*/
			update_lane_oversat_counters(0,0);
			
			/*Checking the output(s)*/
			Assert::AreEqual<int32>(0, lanes.oversat_counter[0]);
			Assert::AreEqual<int32>(-1, links.oversat_weighting_factor[0]);
			Assert::AreEqual<int32>(0, stages.oversat_weighting_factor[0]);		
			Assert::AreEqual<int32>(1, junction.oversat_counter);			
			Assert::AreEqual<int>(mv_true, junction.is_oversat_changed);
			
		}

		/** \test TEST CASE: update_lane_oversat_counters_TC03 \n
			INPUT(S):
					- Work on link number 1, lane number 1 and stage number 1 (index = 0);
					- Not a CMOVA lane;
					- Not a short lane;
					- IN-DET to be used to check oversat;
					- Cycle time is equals to the oversat checking time; 
					- IN-DET is not suspected; and
					- IN-DET ON time is larger than 'det_min_on_time_for_queue'. 
					- Count of IN-DET is greater than 'oversat_critical_veh_count' */
		TEST_METHOD(update_lane_oversat_counters_TC03)
		{
			/*Inputs*/
			DI_set_det_num(1, IN_DET, 22); //To make it not a CMOVA lane by assiging an IN-DET number 22 
			DI_set_oversat_det(1, OVERSAT_DET_IN_DET);
			DI_set_in_det_cruise_time(1, 95);
			DI_set_oversat_critical_veh_count(1, 5); //this is to check it after executing the function UT
			timers.cycle_time_timer[0] = 95*10;
			dets.time_on[21] = -1; //21 is the IN-DET index. less than zero to avoid the setting of: lanes.oversat_init_count in the function
			set_det_suspicion_status(22, DET_FAULT_STATUS_OK); 
			lanes.oversat_init_count[0] = 10;
			
			//The following inputs are related to the function: update_stages_oversat_markers(...)
			DI_set_stages_count(1);
			DI_set_link_green_status(1, 1, LINK_STATUS_GREEN_IN_ONE_STAGE); //any value other than 0 or 4
			lanes.oversat_counter[0] = 1; // any number > 0

			/*Calling the function UT*/
			update_lane_oversat_counters(0,0);

			/*Checking the output(s)*/
			Assert::AreEqual<int>(mv_true, junction.is_oversat_increased);
			Assert::AreEqual<int32>(2, lanes.oversat_counter[0]);
			Assert::AreEqual<int32>(1, junction.oversat_counter);
			Assert::AreEqual<int32>(0, stages.oversat_weighting_factor[0]);
			Assert::AreEqual<int32>(2, stages.oversat_max_counter[0]);
			Assert::AreEqual<int32>(2, junction.max_lane_oversat_counter);
		}

		/** \test TEST CASE: update_lane_oversat_counters_TC04 \n
			INPUT(S):
					- Work on link number 1, lane number 1 and stage number 1 (index = 0);
					- Not a CMOVA lane;
					- Not a short lane;
					- IN-DET to be used to check oversat;
					- Cycle time is equals to the oversat checking time; 
					- IN-DET is not suspected; and
					- IN-DET ON time is larger than 'det_min_on_time_for_queue'. 
					- Count of IN-DET is less than 'oversat_critical_veh_count' */
		TEST_METHOD(update_lane_oversat_counters_TC04)
		{
			/*Inputs*/
			DI_set_det_num(1, IN_DET, 22); //To make it not a CMOVA lane by assiging an IN-DET number 22 
			DI_set_oversat_det(1, OVERSAT_DET_IN_DET);
			DI_set_in_det_cruise_time(1, 95);
			DI_set_oversat_critical_veh_count(1, 5); //this is to check it after executing the function UT
			
			timers.cycle_time_timer[0] = 95*10;
			dets.time_on[21] = -1; //21 is the IN-DET index. less than zero to avoid the setting of: lanes.oversat_init_count in the function
			set_det_suspicion_status(22, DET_FAULT_STATUS_OK); 
			lanes.oversat_init_count[0] = 2; //this is to miss the condition: if (count_diff >= DI_get_oversat_critical_veh_count(lane_num))
			
			//The following inputs are related to the function: update_stages_oversat_markers(...)
			DI_set_stages_count(1);
			DI_set_link_green_status(1, 1, LINK_STATUS_GREEN_IN_ONE_STAGE); //any value other than 0 or 4
			lanes.oversat_counter[0] = 1; // any number > 0

			/*Calling the function UT*/
			update_lane_oversat_counters(0,0);

			/*Checking the output(s)*/
			Assert::AreEqual<int32>(-1, junction.oversat_counter);
			Assert::AreEqual<int32>(0, lanes.oversat_counter[0]);
			Assert::AreEqual<int>(mv_true, junction.is_oversat_changed);
			Assert::AreEqual<int32>(0, links.oversat_weighting_factor[0]);
			Assert::AreEqual<int32>(0, stages.oversat_weighting_factor[0]);
		}

		/** \test TEST CASE: update_lane_oversat_counters_TC05 \n
				  INPUT(S):
							- Work on link number 1, lane number 1 and stage number 1 (index = 0);
							- Not a CMOVA lane;
							- Not a short lane;
							- IN-DET to be used to check oversat;
							- Cycle time is beyond the oversat checking time; */
		TEST_METHOD(update_lane_oversat_counters_TC05)
		{
			/*Inputs*/
			DI_set_det_num(1, IN_DET, 22); //To make it not a CMOVA lane by assiging an IN-DET number 22 
			DI_set_oversat_det(1, OVERSAT_DET_IN_DET);
			DI_set_in_det_cruise_time(1, 95);
			DI_set_oversat_critical_veh_count(1, 5); //this is to check it after executing the function UT
			timers.cycle_time_timer[0] = 1000; //1000 is any number > 95*10
			
			//The following inputs are related to the function: update_stages_oversat_markers(...)
			DI_set_stages_count(1);
			DI_set_link_green_status(1, 1, LINK_STATUS_GREEN_IN_ONE_STAGE); //any value other than 0 or 4
			lanes.oversat_counter[0] = 1; // any number > 0

			/*Calling the function UT*/
			update_lane_oversat_counters(0,0);

			/*Checking the output(s)*/
			Assert::AreEqual<int32>(0, stages.oversat_weighting_factor[0]);
			Assert::AreEqual<int32>(1, stages.oversat_max_counter[0]);
			Assert::AreEqual<int32>(1, junction.max_lane_oversat_counter);

		}

		/** \test TEST CASE: update_lane_oversat_counters_TC06 \n
				  INPUT(S):
							- Work on link number 1, lane number 1 and stage number 1 (index = 0);
							- CMOVA lane;
							- X-DET to be used to check oversat;
							- Cycle time is still before the oversat checking time; 
							- X-DET is not suspected; and
							- X-DET ON time is larger than' det_min_on_time_for_queue'. */
		TEST_METHOD(update_lane_oversat_counters_TC06)
		{
			/*Inputs*/
			DI_set_short_lane_lenght(1, COMPACT_MOVA_LANE);
			DI_set_det_num(1, X_DET, 23);
			DI_set_oversat_det(1, OVERSAT_DET_CMOVA_X_DET);
			DI_set_oversat_time(1, 80);
			DI_set_oversat_critical_veh_count(1, 5); //this is to check it after executing the function UT
			timers.cycle_time_timer[0] = 12; //12 is any number < 80*10
			dets.time_on[22] = 40; //23 is the X_DET index. 40 is any number > 0
			set_det_suspicion_status(23, DET_FAULT_STATUS_OK); 
			
			//The following inputs are related to the function: update_stages_oversat_markers(...)
			DI_set_stages_count(1);
			DI_set_link_green_status(1, 1, LINK_STATUS_GREEN_IN_ONE_STAGE); //any value other than 0 or 4
			lanes.oversat_counter[0] = 1; // any number > 0

			/*Calling the function UT*/
			update_lane_oversat_counters(0,0);

			/*Checking the output(s)*/
			Assert::AreEqual<int32>(5, lanes.oversat_init_count[0]);
			Assert::AreEqual<int32>(0, stages.oversat_weighting_factor[0]);
			Assert::AreEqual<int32>(1, stages.oversat_max_counter[0]);
			Assert::AreEqual<int32>(1, junction.max_lane_oversat_counter);
		}

		/** \test TEST CASE: update_lane_oversat_counters_TC07 \n
				  INPUT(S):
							- Work on link number 1, lane number 1 and stage number 1 (index = 0);
							- CMOVA lane;
							- COMB-X to be used to check oversat;
							- Cycle time is still before the oversat checking time; 
							- COMB-X is not suspected; and
							- COMB-X ON time is larger than' det_min_on_time_for_queue'. */
		TEST_METHOD(update_lane_oversat_counters_TC07)
		{
			/*Inputs*/
			DI_set_short_lane_lenght(1, COMPACT_MOVA_LANE);
			DI_set_det_num(1, X_DET, 23);
			DI_set_det_num(1, COMB_X_DET, 24);			
			DI_set_oversat_det(1, OVERSAT_DET_CMOVA_COMB_X);
			DI_set_oversat_time(1, 80);
			DI_set_oversat_critical_veh_count(1, 5); //this is to check it after executing the function UT

			timers.cycle_time_timer[0] = 12; //12 is any number < 80*10
			dets.time_on[22] = 40; //22 is the X-DET index. 40 is any number > 0 (det_min_on_time_for_queue)
			set_det_suspicion_status(23, DET_FAULT_STATUS_OK); 
			
			//The following inputs are related to the function: update_stages_oversat_markers(...)
			DI_set_stages_count(1);
			DI_set_link_green_status(1, 1, LINK_STATUS_GREEN_IN_ONE_STAGE); //any value other than 0 or 4
			lanes.oversat_counter[0] = 1; // any number > 0

			/*Calling the function UT*/
			update_lane_oversat_counters(0,0);

			/*Checking the output(s)*/
			Assert::AreEqual<int32>(5, lanes.oversat_init_count[0]);
			Assert::AreEqual<int32>(0, stages.oversat_weighting_factor[0]);
			Assert::AreEqual<int32>(1, stages.oversat_max_counter[0]);
			Assert::AreEqual<int32>(1, junction.max_lane_oversat_counter);
		}


		/////////////////////////////////////////////////////////////////////
		////////// The following test cases realted to Feature (1.6) ////////

		/** \test TEST CASE: update_lane_oversat_counters_TC08 \n
				  INPUT(S):
							- Work on link number 1, lane number 1 and stage number 1 (index = 0);
							- Not a CMOVA lane;
							- Not a short lane;
							- IN-DET to be used to check oversat;
							- Cycle time is still before the oversat checking time; 
							- IN-DET is SUSPECTED; 
							- The lane has an X-DET; and
							- The lane's oversat counter = 0*/
		TEST_METHOD(update_lane_oversat_counters_TC08)
		{
			/*Inputs*/
			DI_set_det_num(1, IN_DET, 22); //To make it not a CMOVA lane by assiging an IN-DET number 22 
			DI_set_det_num(1, X_DET, 23); 
			DI_set_oversat_det(1, OVERSAT_DET_IN_DET);
			DI_set_oversat_time(1, 80);
			DI_set_in_det_cruise_time(1, 95);
			DI_set_oversat_critical_veh_count_compact(1, 6);

			lanes.oversat_counter[0] = 0;
			timers.cycle_time_timer[0] = 12; //12 is any number < 95*10
			dets.time_on[22] = 40; //23 is the X_DET index. 40 is any number > 0
			set_det_suspicion_status(22, DET_FAULT_STATUS_SUSPECTED_DUE_TO_LONG_OFF); //any suspicious status

			//The following inputs are related to the function: update_stages_oversat_markers(...)
			DI_set_stages_count(1);
			DI_set_link_green_status(1, 1, LINK_STATUS_GREEN_IN_ONE_STAGE); //any value other than 0 or 4
			lanes.oversat_counter[0] = 1; // any number > 0

			/*Calling the function UT*/
			update_lane_oversat_counters(0,0);
			
			/*Checking the output(s)*/
			Assert::AreEqual<int32>(6, lanes.oversat_init_count[0]);
			Assert::AreEqual<int32>(0, stages.oversat_weighting_factor[0]);
			Assert::AreEqual<int32>(1, stages.oversat_max_counter[0]);
			Assert::AreEqual<int32>(1, junction.max_lane_oversat_counter);
		}


		/** \test TEST CASE: update_lane_oversat_counters_TC09 \n
				  INPUT(S):
							- Work on link number 1, lane number 1 and stage number 1 (index = 0);
							- Not a CMOVA lane;
							- Not a short lane;
							- IN-DET to be used to check oversat;
							- Cycle time is still before the oversat checking time; 
							- IN-DET is SUSPECTED; 
							- The lane has a COMB-X det; and
							- The lane's oversat counter = 0*/
		TEST_METHOD(update_lane_oversat_counters_TC09)
		{
			/*Inputs*/
			DI_set_det_num(1, IN_DET, 22);
			DI_set_det_num(1, X_DET, 23);
			DI_set_det_num(1, COMB_X_DET, 24);			
			DI_set_oversat_det(1, OVERSAT_DET_IN_DET);
			DI_set_oversat_time(1, 80);
			DI_set_oversat_critical_veh_count_compact(1, 6);

			timers.cycle_time_timer[0] = 12; //12 is any number < 80*10
			dets.time_on[22] = 40; //22 is the X-DET index. 40 is any number > 0 (det_min_on_time_for_queue)
			set_det_suspicion_status(22, DET_FAULT_STATUS_SUSPECTED_DUE_TO_LONG_OFF); //any suspicious status
			
			//The following inputs are related to the function: update_stages_oversat_markers(...)
			DI_set_stages_count(1);
			DI_set_link_green_status(1, 1, LINK_STATUS_GREEN_IN_ONE_STAGE); //any value other than 0 or 4
			lanes.oversat_counter[0] = 1; // any number > 0

			/*Calling the function UT*/
			update_lane_oversat_counters(0,0);

			/*Checking the output(s)*/
			Assert::AreEqual<int32>(6, lanes.oversat_init_count[0]);
			Assert::AreEqual<int32>(0, stages.oversat_weighting_factor[0]);
			Assert::AreEqual<int32>(1, stages.oversat_max_counter[0]);
			Assert::AreEqual<int32>(1, junction.max_lane_oversat_counter);
		}
		



	};
}
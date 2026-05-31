#include "stdafx.h"
#include "CppUnitTest.h"

#include "common_data.h"
#include "DataManagerForTesting.h"

	
using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace MOVATest
{
	TEST_CLASS(LanesEndsatHandlerTests)
	{
	public:

		///////////////////   Setup and Cleanup   /////////////////////
		TEST_CLASS_INITIALIZE(Init_LaneEndSatsHandlerTests) {	}

		TEST_CLASS_CLEANUP(CleaningUp_LanesEndsatHandlerTests) {}

		TEST_METHOD_INITIALIZE(will_be_called_before_any_test_mesthod)
		{
			DataManagerForTesting::ResetingMovaDataSet();
			DataManagerForTesting::ResetingMovaDynamicData();

		}
		////////////////////////////////////////////////////////////////
		
		/*NOTE: the focus of the following test cases is on the implementation
				of the function: calc_corrected_lane_critical_gap(...). */
		
		////////////////////////////////////////////////////////////////


		/** \test TEST CASE: use_x_det_to_check_endsat_TC01 \n
				  INPUT(S):
							- Work on Link number 1, lane number 1 and X-Det number 22;
							- No queue over the X-DET; 
							- The lane is not oversaturated; and
							- The X-DET off time is greater than critical gap;
							*/
		TEST_METHOD(use_x_det_to_check_endsat_TC01)
		{
			/* Test case local vars */
			mv_bool res;

			/*Inputs*/
			DI_set_det_num(1, X_DET, 22);
			DI_set_det_min_on_time_for_queue(1, 53); //any number > 0
			DI_set_scan_period(5);
			DI_set_lane_critical_gap(1, 34);

			set_det_suspicion_status(22, DET_FAULT_STATUS_OK);
			dets.time_off[21] = 35;	//any number > 34 (the critical gap)

			//inputes releated to the function: calc_corrected_lane_critical_gap(...)
			lanes.oversat_counter[0] = 0;


			/*Calling the function UT*/
			res = use_x_det_to_check_endsat(0, 0);

			/*Checking the output(s)*/
			Assert::AreEqual<int>(mv_false, res);
		}

		/** \test TEST CASE: use_x_det_to_check_endsat_TC02 \n
				  INPUT(S):
							- Work on Link number 1, lane number 1 and X-Det number 22;
							- No queue over the X-DET; 
							- The lane is not oversaturated; and
							- The X-DET off time is less than critical gap;
							*/
		TEST_METHOD(use_x_det_to_check_endsat_TC02)
		{
			/* Test case local vars */
			mv_bool res;

			/*Inputs*/
			DI_set_det_num(1, X_DET, 22);
			DI_set_det_min_on_time_for_queue(1, 53); //any number > 0
			DI_set_scan_period(5);
			DI_set_lane_critical_gap(1, 34);

			set_det_suspicion_status(22, DET_FAULT_STATUS_OK);
			dets.time_off[21] = 20;	//any number < 34 (the critical gap)

			//inputes releated to the function: calc_corrected_lane_critical_gap(...)
			lanes.oversat_counter[0] = 0;


			/*Calling the function UT*/
			res = use_x_det_to_check_endsat(0, 0);

			/*Checking the output(s)*/
			Assert::AreEqual<int>(mv_true, res);
		}

		/** \test TEST CASE: use_x_det_to_check_endsat_TC03 \n
				  INPUT(S):
							- Work on Link number 1, lane number 1 and X-Det number 22;
							- No queue over the X-DET; 
							- The lane is oversaturated;
							- The X-DET off time is less than critical gap;

							- The lengthened critical gap = 50%; and
							- The green time (signal_state_timer) is larger than the moving queue over IN-DET time. */
		TEST_METHOD(use_x_det_to_check_endsat_TC03)
		{
			/* Test case local vars */
			mv_bool res;

			/*Inputs*/
			DI_set_det_num(1, X_DET, 22);
			DI_set_det_min_on_time_for_queue(1, 53); //any number > 0
			DI_set_scan_period(5);
			DI_set_lane_critical_gap(1, 19);
			DI_set_lane_lengthened_critical_gap(1, 50);
						
			set_det_suspicion_status(22, DET_FAULT_STATUS_OK);
			dets.time_off[21] = 20;	//any number < 34 (the critical gap)

			//inputes releated to the function: calc_corrected_lane_critical_gap(...)
			DI_set_moving_q_over_in_det(1,60); //any number < 79 but > 50
			
			lanes.oversat_counter[0] = 2; //to make the lane oversat
			timers.signal_state_timer = 79; //should be > 50 to avid the +50 in the if condition


			/*Calling the function UT*/
			res = use_x_det_to_check_endsat(0, 0);

			/*Checking the output(s)*/
			Assert::AreEqual<int>(mv_true, res);
		}

		/** \test TEST CASE: use_x_det_to_check_endsat_TC04 \n
				  INPUT(S):
							- Work on Link number 1, lane number 1 and X-Det number 22;
							- No queue over the X-DET; 
							- The lane is oversaturated;
							- The X-DET off time is less than critical gap;

							- The lengthened critical gap = 50%; and
							- The green time (signal_state_timer) is larger than the moving queue over X-DET time. */
		TEST_METHOD(use_x_det_to_check_endsat_TC04)
		{
			/* Test case local vars */
			mv_bool res;

			/*Inputs*/
			DI_set_det_num(1, X_DET, 22);
			DI_set_det_min_on_time_for_queue(1, 53); //any number > 0
			DI_set_scan_period(5);
			DI_set_lane_critical_gap(1, 19);
			DI_set_lane_lengthened_critical_gap(1, 50);
						
			set_det_suspicion_status(22, DET_FAULT_STATUS_OK);
			dets.time_off[21] = 20;	//any number < 34 (the critical gap)

			//inputes releated to the function: calc_corrected_lane_critical_gap(...)
			DI_set_moving_q_over_in_det(1,0); //just to avid its effect in: max(...)
			DI_set_moving_q_over_x_det(1,21); //any number + 50 < 79
			
			lanes.oversat_counter[0] = 2; //to make the lane oversat
			timers.signal_state_timer = 79;


			/*Calling the function UT*/
			res = use_x_det_to_check_endsat(0, 0);

			/*Checking the output(s)*/
			Assert::AreEqual<int>(mv_true, res);
		}
	};
}

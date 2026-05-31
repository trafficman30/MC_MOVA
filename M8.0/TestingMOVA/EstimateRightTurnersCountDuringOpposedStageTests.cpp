#include "stdafx.h"
#include "CppUnitTest.h"

#include <string.h>
#include <stdlib.h>

#include "common_data.h"
#include "DataManagerForTesting.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace MOVATest
{
	TEST_CLASS(EstimateRightTurnersCountDuringOpposedStageTests)
	{
	public:

		///////////////////   Setup and Cleanup   /////////////////////
		TEST_CLASS_INITIALIZE(Init_EstimateRightTurnersCountDuringOpposedStageTests) {	}

		TEST_CLASS_CLEANUP(CleaningUp_EstimateRightTurnersCountDuringOpposedStageTests) {}

		TEST_METHOD_INITIALIZE(will_be_called_before_any_test_mesthod)
		{
			DataManagerForTesting::ResetingMovaDataSet();
			DataManagerForTesting::ResetingMovaDynamicData();
		}
		////////////////////////////////////////////////////////////////
		

		/** \test TEST CASE: ALG_estimate_right_turners_count_during_opposed_stage_TC01 \n
				  INPUT(S):
							- The current stage (opposed) = 1;
							- The next stage (unopposed) = 3; 
							- The lane number of the lane that has the RT = 1;
							- One lane is opposing and its number = 6;
							- The index of the X-DET on the opposed lane = 22; and
							- nl, So, S0, Ns, r and f as defined below.*/
		TEST_METHOD(ALG_estimate_right_turners_count_during_opposed_stage_TC01)
		{
			uint8 get_away_veh_count;

			// 1)Inputs

			//The following two vars, set any number to make lambda 
			//(the ration between them) to be 0.5.
			timers.signal_state_timer = 500;
			timers.stage_cycle_timer[0] = 1000; // c = 100 sec
			
			junction.net_gaps[0] = 50;		// gap1 = 5 sec
			junction.net_gaps[1] = 73;		// gap2 = 7.3 sec
			junction.net_gaps[2] = 98;		// gap3 = 9.8 sec
			junction.net_gaps_count = 3;

			DI_set_opposed_lane_num(1, 0, 6);		// nl = 1
			DI_set_opposed_lane_num(1, 1, 0);
			DI_set_opposed_lane_num(1, 2, 0);

			DI_set_satinc_time(6, 20);				// To make: So = 1800
			DI_set_satinc_time(1, 17);				// To make: S0 = 2080

			DI_set_right_turn_storage_capacity(1, 1);   // Ns = 1
			DI_set_right_turn_radius(1, 20);			// r = 20 m
			DI_set_right_turn_veh_proportion(1, 1);		// f = 1

			// 2)Execution 
			get_away_veh_count = ALG_estimate_right_turners_count_during_opposed_stage(1,  //Stage number
																					   1,  //Opposed lane number
																					   junction.net_gaps,
																					   junction.net_gaps_count);


			// 3)Checking
			Assert::AreEqual<uint8>(3, get_away_veh_count);
		}

		
	};
}
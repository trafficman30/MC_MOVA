
#pragma once

extern "C"
{
	#define UNIT_TESTING

	#include "..\\kernel\\dataset_interface.h"
	
	#include "..\kernel\dynamic_data.h"
	#include "..\kernel\timers_manager.h"

	#include "..\kernel\lanes_handler.h"
	#include "..\kernel\lanes_endsat_handler.h"
	#include "..\kernel\links_handler.h"
	#include "..\kernel\links_endsat_handler.h"
	#include "..\kernel\links_ep_handler.h"
	#include "..\kernel\junction_handler.h"
	#include "..\kernel\detectors_handler.h"


	#include "..\kernel\core_interface.h"

	#include "..\kernel\errhand.h"

	#include "..\kernel\estimate_right_turners_count_during_opposed_stage.h"

	extern struct Lanes lanes;
	extern struct Links links;
	extern struct Detectors dets;
	extern struct Stages stages;
	extern struct Junction junction;
}



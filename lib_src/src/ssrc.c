// ===========================================================================
// ===========================================================================
//	
// File: SSRC.c
//
// Top level implementation file for the SSRC
//
// Target:	MS Windows
// Version: 1.0
//
// ===========================================================================
// ===========================================================================


// ===========================================================================
//
// Includes
//
// ===========================================================================
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <math.h>

// Integer arithmetic include
#include "IntArithmetic.h"
// SSRC include
#include "ssrc.h"

// ===========================================================================
//
// Defines
//
// ===========================================================================


// State init value
#define		SSRC_STATE_INIT						0

// Random number generator / dithering
#define		SSRC_R_CONS							32767
#define		SSRC_R_BASE							1664525	
#define		SSRC_RPDF_BITS_SHIFT				16						// Shift to select bits in pseudo-random number
#define		SSRC_RPDF_MASK						0x0000007F				// For dithering at 24bits (in 2.30)
#define		SSRC_DATA24_MASK					0xFFFFFF00				// Mask for 24bits data (once rescaled to 1.31)
#define		SSRC_DITHER_BIAS					0xFFFFFFC0				// TPDF dither bias for compensating masking at 24bits but expressed in 2.30

// Cycle counter 
#define		SSRC_FIR_OS2_OVERHEAD_CYCLE_COUNT	(15.0)
#define		SSRC_FIR_OS2_TAP_CYCLE_COUNT		(1.875)
#define		SSRC_FIR_DS2_OVERHEAD_CYCLE_COUNT	(15.0)
#define		SSRC_FIR_DS2_TAP_CYCLE_COUNT		(2.125)
#define		SSRC_FIR_SYNC_OVERHEAD_CYCLE_COUNT	(15.0)
#define		SSRC_FIR_SYNC_TAP_CYCLE_COUNT		(2.125)
#define		SSRC_FIR_PP_OVERHEAD_CYCLE_COUNT	(15.0 + SSRC_FIR_SYNC_OVERHEAD_CYCLE_COUNT)
#define		SSRC_FIR_PP_TAP_CYCLE_COUNT			(2.125)
#define		SSRC_DITHER_SAMPLE_COUNT			(20.0)						



// ===========================================================================
//
// Variables
//
// ===========================================================================

SSRCFiltersIDs_t		sFiltersIDs[SSRC_N_FS][SSRC_N_FS] =				// Filter configuration table [Fsin][Fsout]
{
	{	// Fsin = 44.1kHz
		// F1							F2								F3								Phase step
		{FILTER_DEFS_FIR_NONE_ID,		FILTER_DEFS_FIR_NONE_ID,		FILTER_DEFS_PPFIR_NONE_ID,		FILTER_DEFS_PPFIR_PHASE_STEP_0},		// Fsout = 44.1kHz
		{FILTER_DEFS_FIR_UP_ID,			FILTER_DEFS_FIR_NONE_ID,		FILTER_DEFS_PPFIR_HS320_ID,		FILTER_DEFS_PPFIR_PHASE_STEP_294},		// Fsout = 48kHz
		{FILTER_DEFS_FIR_UP_ID,			FILTER_DEFS_FIR_NONE_ID,		FILTER_DEFS_PPFIR_NONE_ID,		FILTER_DEFS_PPFIR_PHASE_STEP_0},		// Fsout = 88.2kHz
		{FILTER_DEFS_FIR_UP_ID,			FILTER_DEFS_FIR_NONE_ID,		FILTER_DEFS_PPFIR_HS320_ID,		FILTER_DEFS_PPFIR_PHASE_STEP_147},		// Fsout = 96kHz
		{FILTER_DEFS_FIR_UP_ID,			FILTER_DEFS_FIR_OS_ID,			FILTER_DEFS_PPFIR_NONE_ID,		FILTER_DEFS_PPFIR_PHASE_STEP_0},		// Fsout = 176.4kHz
		{FILTER_DEFS_FIR_UP_ID,			FILTER_DEFS_FIR_OS_ID,			FILTER_DEFS_PPFIR_HS320_ID,		FILTER_DEFS_PPFIR_PHASE_STEP_147}		// Fsout = 192kHz
	},
	{	// Fsin = 48kHz
		// F1							F2								F3								Phase step
		{FILTER_DEFS_FIR_UP4844_ID,		FILTER_DEFS_FIR_NONE_ID,		FILTER_DEFS_PPFIR_HS294_ID,		FILTER_DEFS_PPFIR_PHASE_STEP_320},		// Fsout = 44.1kHz
		{FILTER_DEFS_FIR_NONE_ID,		FILTER_DEFS_FIR_NONE_ID,		FILTER_DEFS_PPFIR_NONE_ID,		FILTER_DEFS_PPFIR_PHASE_STEP_0},		// Fsout = 48kHz
		{FILTER_DEFS_FIR_UP_ID,			FILTER_DEFS_FIR_NONE_ID,		FILTER_DEFS_PPFIR_HS294_ID,		FILTER_DEFS_PPFIR_PHASE_STEP_160},		// Fsout = 88.2kHz
		{FILTER_DEFS_FIR_UP_ID,			FILTER_DEFS_FIR_NONE_ID,		FILTER_DEFS_PPFIR_NONE_ID,		FILTER_DEFS_PPFIR_PHASE_STEP_0},		// Fsout = 96kHz
		{FILTER_DEFS_FIR_UP_ID,			FILTER_DEFS_FIR_OS_ID,			FILTER_DEFS_PPFIR_HS294_ID,		FILTER_DEFS_PPFIR_PHASE_STEP_160},		// Fsout = 176.4kHz
		{FILTER_DEFS_FIR_UP_ID,			FILTER_DEFS_FIR_OS_ID,			FILTER_DEFS_PPFIR_NONE_ID,		FILTER_DEFS_PPFIR_PHASE_STEP_0}			// Fsout = 192kHz
	},
	{	// Fsin = 88.2kHz
		// F1							F2								F3								Phase step
		{FILTER_DEFS_FIR_BL_ID,			FILTER_DEFS_FIR_NONE_ID,		FILTER_DEFS_PPFIR_NONE_ID,		FILTER_DEFS_PPFIR_PHASE_STEP_0},		// Fsout = 44.1kHz
		{FILTER_DEFS_FIR_BL8848_ID,		FILTER_DEFS_FIR_NONE_ID,		FILTER_DEFS_PPFIR_HS320_ID,		FILTER_DEFS_PPFIR_PHASE_STEP_294},		// Fsout = 48kHz
		{FILTER_DEFS_FIR_NONE_ID,		FILTER_DEFS_FIR_NONE_ID,		FILTER_DEFS_PPFIR_NONE_ID,		FILTER_DEFS_PPFIR_PHASE_STEP_0},		// Fsout = 88.2kHz
		{FILTER_DEFS_FIR_UP_ID,			FILTER_DEFS_FIR_NONE_ID,		FILTER_DEFS_PPFIR_HS320_ID,		FILTER_DEFS_PPFIR_PHASE_STEP_294},		// Fsout = 96kHz
		{FILTER_DEFS_FIR_UP_ID,			FILTER_DEFS_FIR_NONE_ID,		FILTER_DEFS_PPFIR_NONE_ID,		FILTER_DEFS_PPFIR_PHASE_STEP_0},		// Fsout = 176.4kHz
		{FILTER_DEFS_FIR_UP_ID,			FILTER_DEFS_FIR_NONE_ID,		FILTER_DEFS_PPFIR_HS320_ID,		FILTER_DEFS_PPFIR_PHASE_STEP_147}		// Fsout = 192kHz
	},
	{	// Fsin = 96kHz
		// F1							F2								F3								Phase step
		{FILTER_DEFS_FIR_BL9644_ID,		FILTER_DEFS_FIR_NONE_ID,		FILTER_DEFS_PPFIR_HS294_ID,		FILTER_DEFS_PPFIR_PHASE_STEP_320},		// Fsout = 44.1kHz
		{FILTER_DEFS_FIR_BL_ID,			FILTER_DEFS_FIR_NONE_ID,		FILTER_DEFS_PPFIR_NONE_ID,		FILTER_DEFS_PPFIR_PHASE_STEP_0},		// Fsout = 48kHz
		{FILTER_DEFS_FIR_UP4844_ID,		FILTER_DEFS_FIR_NONE_ID,		FILTER_DEFS_PPFIR_HS294_ID,		FILTER_DEFS_PPFIR_PHASE_STEP_320},		// Fsout = 88.2kHz
		{FILTER_DEFS_FIR_NONE_ID,		FILTER_DEFS_FIR_NONE_ID,		FILTER_DEFS_PPFIR_NONE_ID,		FILTER_DEFS_PPFIR_PHASE_STEP_0},		// Fsout = 96kHz
		{FILTER_DEFS_FIR_UP_ID,			FILTER_DEFS_FIR_NONE_ID,		FILTER_DEFS_PPFIR_HS294_ID,		FILTER_DEFS_PPFIR_PHASE_STEP_160},		// Fsout = 176.4kHz
		{FILTER_DEFS_FIR_UP_ID,			FILTER_DEFS_FIR_NONE_ID,		FILTER_DEFS_PPFIR_NONE_ID,		FILTER_DEFS_PPFIR_PHASE_STEP_0}			// Fsout = 192kHz
	},
	{	// Fsin = 176.4kHz
		// F1							F2								F3								Phase step
		{FILTER_DEFS_FIR_DS_ID,			FILTER_DEFS_FIR_BL_ID,			FILTER_DEFS_PPFIR_NONE_ID,		FILTER_DEFS_PPFIR_PHASE_STEP_0},		// Fsout = 44.1kHz
		{FILTER_DEFS_FIR_DS_ID,			FILTER_DEFS_FIR_BL8848_ID,		FILTER_DEFS_PPFIR_HS320_ID,		FILTER_DEFS_PPFIR_PHASE_STEP_294},		// Fsout = 48kHz
		{FILTER_DEFS_FIR_BL_ID,			FILTER_DEFS_FIR_NONE_ID,		FILTER_DEFS_PPFIR_NONE_ID,		FILTER_DEFS_PPFIR_PHASE_STEP_0},		// Fsout = 88.2kHz
		{FILTER_DEFS_FIR_BL17696_ID,	FILTER_DEFS_FIR_NONE_ID,		FILTER_DEFS_PPFIR_HS320_ID,		FILTER_DEFS_PPFIR_PHASE_STEP_294},		// Fsout = 96kHz
		{FILTER_DEFS_FIR_NONE_ID,		FILTER_DEFS_FIR_NONE_ID,		FILTER_DEFS_PPFIR_NONE_ID,		FILTER_DEFS_PPFIR_PHASE_STEP_0},		// Fsout = 176.4kHz
		{FILTER_DEFS_FIR_UPF_ID,		FILTER_DEFS_FIR_NONE_ID,		FILTER_DEFS_PPFIR_HS320_ID,		FILTER_DEFS_PPFIR_PHASE_STEP_294}		// Fsout = 192kHz
	},
	{	// Fsin = 192kHz
		// F1							F2								F3								Phase step
		{FILTER_DEFS_FIR_DS_ID,			FILTER_DEFS_FIR_BL9644_ID,		FILTER_DEFS_PPFIR_HS294_ID,		FILTER_DEFS_PPFIR_PHASE_STEP_320},		// Fsout = 44.1kHz
		{FILTER_DEFS_FIR_DS_ID,			FILTER_DEFS_FIR_BL_ID,			FILTER_DEFS_PPFIR_NONE_ID,		FILTER_DEFS_PPFIR_PHASE_STEP_0},		// Fsout = 48kHz
		{FITLER_DEFS_FIR_BL19288_ID,	FILTER_DEFS_FIR_NONE_ID,		FILTER_DEFS_PPFIR_HS294_ID,		FILTER_DEFS_PPFIR_PHASE_STEP_320},		// Fsout = 88.2kHz
		{FILTER_DEFS_FIR_BL_ID,			FILTER_DEFS_FIR_NONE_ID,		FILTER_DEFS_PPFIR_NONE_ID,		FILTER_DEFS_PPFIR_PHASE_STEP_0},		// Fsout = 96kHz
		{FILTER_DEFS_FIR_UP192176_ID,	FILTER_DEFS_FIR_NONE_ID,		FILTER_DEFS_PPFIR_HS294_ID,		FILTER_DEFS_PPFIR_PHASE_STEP_320},		// Fsout = 176.4kHz
		{FILTER_DEFS_FIR_NONE_ID,		FILTER_DEFS_FIR_NONE_ID,		FILTER_DEFS_PPFIR_NONE_ID,		FILTER_DEFS_PPFIR_PHASE_STEP_0}			// Fsout = 192kHz
	}
};



// ===========================================================================
//
// Local Functions prototypes
//
// ===========================================================================


SSRCReturnCodes_t				SSRC_proc_F1_F2(SSRCCtrl_t* psSSRCCtrl);
SSRCReturnCodes_t				SSRC_proc_F3(SSRCCtrl_t* psSSRCCtrl);
SSRCReturnCodes_t				SSRC_proc_dither(SSRCCtrl_t* psSSRCCtrl);


// ===========================================================================
//
// Functions implementations
//
// ===========================================================================



// ==================================================================== //
// Function:		SSRC_init											//
// Arguments:		SSRCCtrl_t 	*psSSRCCtrl: Ctrl strct.				//
// Return values:	SSRC_NO_ERROR on success							//
//					SSRC_ERROR on failure								//
// Description:		Inits the SSRC passed as argument					//
// ==================================================================== //
SSRCReturnCodes_t				SSRC_init(SSRCCtrl_t* psSSRCCtrl)	
{
	SSRCFiltersIDs_t*			psFiltersID;
	FIRDescriptor_t*			psFIRDescriptor;
	PPFIRDescriptor_t*			psPPFIRDescriptor;


	// Check if state is allocated
	if(psSSRCCtrl->psState == 0)
		return SSRC_ERROR;

	// Check if stack is allocated
	if(psSSRCCtrl->piStack == 0)
		return SSRC_ERROR;

	// Check if valid Fsin and Fsout have been provided
	if( (psSSRCCtrl->eInFs < SSRC_FS_MIN) || (psSSRCCtrl->eInFs > SSRC_FS_MAX))
		return SSRC_ERROR;
	if( (psSSRCCtrl->eOutFs < SSRC_FS_MIN) || (psSSRCCtrl->eOutFs > SSRC_FS_MAX))
		return SSRC_ERROR;

	// Check that number of input samples is allocated and is a multiple of 4
	if(psSSRCCtrl->uiNInSamples == 0)
		return SSRC_ERROR;
	if((psSSRCCtrl->uiNInSamples & 0x3) != 0x0)
		return SSRC_ERROR;

	// Load filters ID and number of samples
	psFiltersID		= &sFiltersIDs[psSSRCCtrl->eInFs][psSSRCCtrl->eOutFs];
	
	// Configure filters from filters ID and number of samples
	
	// Filter F1
	// ---------
	psFIRDescriptor								= &sFirDescriptor[psFiltersID->uiFID[SSRC_F1_INDEX]];
	// Set number of input samples and input samples step
	psSSRCCtrl->sFIRF1Ctrl.uiNInSamples		= psSSRCCtrl->uiNInSamples;
	psSSRCCtrl->sFIRF1Ctrl.uiInStep			= SSRC_CHANNELS_PER_CORE;
	
	// Set delay line base pointer
	if( (psFiltersID->uiFID[SSRC_F1_INDEX] == FILTER_DEFS_FIR_DS_ID) || (psFiltersID->uiFID[SSRC_F1_INDEX] == FILTER_DEFS_FIR_OS_ID) )
		psSSRCCtrl->sFIRF1Ctrl.piDelayB		= psSSRCCtrl->psState->iDelayFIRShort;
	else
		psSSRCCtrl->sFIRF1Ctrl.piDelayB		= psSSRCCtrl->psState->iDelayFIRLong;

	// Set output buffer step
	if(psFiltersID->uiFID[SSRC_F2_INDEX] == FILTER_DEFS_FIR_OS_ID)
		// F2 in use in over-sampling by 2 mode
		psSSRCCtrl->sFIRF1Ctrl.uiOutStep	= 2 * SSRC_CHANNELS_PER_CORE;
	else
		psSSRCCtrl->sFIRF1Ctrl.uiOutStep	= SSRC_CHANNELS_PER_CORE;

	// Call init for FIR F1
	if(FIR_init_from_desc(&psSSRCCtrl->sFIRF1Ctrl, psFIRDescriptor) != FIR_NO_ERROR)
		return FIR_ERROR;
	

	// Filter F2
	// ---------
	psFIRDescriptor							= &sFirDescriptor[psFiltersID->uiFID[SSRC_F2_INDEX]];
	
	// Set number of input samples and input samples step
	psSSRCCtrl->sFIRF2Ctrl.uiNInSamples		= psSSRCCtrl->sFIRF1Ctrl.uiNOutSamples;
	psSSRCCtrl->sFIRF2Ctrl.uiInStep			= psSSRCCtrl->sFIRF1Ctrl.uiOutStep;
		
	// Set delay line base pointer
	if( (psFiltersID->uiFID[SSRC_F2_INDEX] == FILTER_DEFS_FIR_DS_ID) || (psFiltersID->uiFID[SSRC_F2_INDEX] == FILTER_DEFS_FIR_OS_ID) )
		psSSRCCtrl->sFIRF2Ctrl.piDelayB		= psSSRCCtrl->psState->iDelayFIRShort;
	else
		psSSRCCtrl->sFIRF2Ctrl.piDelayB		= psSSRCCtrl->psState->iDelayFIRLong;

	// Set output buffer step
	psSSRCCtrl->sFIRF2Ctrl.uiOutStep	= SSRC_CHANNELS_PER_CORE;
	
	// Call init for FIR F1
	if(FIR_init_from_desc(&psSSRCCtrl->sFIRF2Ctrl, psFIRDescriptor) != FIR_NO_ERROR)
		return FIR_ERROR;


	// Filter F3
	// ---------
	psPPFIRDescriptor								= &sPPFirDescriptor[psFiltersID->uiFID[SSRC_F3_INDEX]];
	
	// Set number of input samples and input samples step
	if(psFiltersID->uiFID[SSRC_F2_INDEX] == FILTER_DEFS_FIR_NONE_ID)
		psSSRCCtrl->sPPFIRF3Ctrl.uiNInSamples	= psSSRCCtrl->sFIRF1Ctrl.uiNOutSamples;
	else
		psSSRCCtrl->sPPFIRF3Ctrl.uiNInSamples	= psSSRCCtrl->sFIRF2Ctrl.uiNOutSamples;	
	psSSRCCtrl->sPPFIRF3Ctrl.uiInStep		= psSSRCCtrl->sFIRF2Ctrl.uiOutStep;
		
	// Set delay line base pointer
	psSSRCCtrl->sPPFIRF3Ctrl.piDelayB		= psSSRCCtrl->psState->iDelayPPFIR;
		
	// Set output buffer step
	psSSRCCtrl->sPPFIRF3Ctrl.uiOutStep		= SSRC_CHANNELS_PER_CORE;

	// Set phase step 
	psSSRCCtrl->sPPFIRF3Ctrl.uiPhaseStep	= psFiltersID->uiPPFIRPhaseStep;
		
	// Call init for PPFIR F3
	if(PPFIR_init_from_desc(&psSSRCCtrl->sPPFIRF3Ctrl, psPPFIRDescriptor) != FIR_NO_ERROR)
		return FIR_ERROR;


	// Setup input/output buffers
	// --------------------------
	// We first set them all to stack base (some will be overwritten depending on configuration)

	// F1 input is never from stack, so don't set it
	psSSRCCtrl->sFIRF2Ctrl.piIn				= psSSRCCtrl->piStack;
	psSSRCCtrl->sPPFIRF3Ctrl.piIn			= psSSRCCtrl->piStack;

	psSSRCCtrl->sFIRF1Ctrl.piOut			= psSSRCCtrl->piStack;
	psSSRCCtrl->sFIRF2Ctrl.piOut			= psSSRCCtrl->piStack;
	// F3 output is never to stack, so don't set it
	

	// Finally setup pointer to output buffer that needs to be modified for data output depending on filter configuration
	// Also set pointer to number of output samples
	if(psFiltersID->uiFID[SSRC_F3_INDEX] != FILTER_DEFS_PPFIR_NONE_ID)
	{
		// F3 is in use so take output from F3 output
		psSSRCCtrl->ppiOut			= &psSSRCCtrl->sPPFIRF3Ctrl.piOut;
		psSSRCCtrl->puiNOutSamples	= &psSSRCCtrl->sPPFIRF3Ctrl.uiNOutSamples;
	}
	else
	{
		if(psFiltersID->uiFID[SSRC_F2_INDEX] != FILTER_DEFS_FIR_NONE_ID)
		{
			// F3 not in use but F2 in use, take output from F2 output
			psSSRCCtrl->ppiOut			= &psSSRCCtrl->sFIRF2Ctrl.piOut;
			psSSRCCtrl->puiNOutSamples	= &psSSRCCtrl->sFIRF2Ctrl.uiNOutSamples;
		}
		else
		{
			// F3 and F2 not in use but F1 in use or not. Set output from F1 output
			// Note that we also set it to F1 output, even if F1 is not in use (Fsin = Fsout case, this won't cause any problem)
			psSSRCCtrl->ppiOut		= &psSSRCCtrl->sFIRF1Ctrl.piOut;
			
			if(psFiltersID->uiFID[SSRC_F1_INDEX] != FILTER_DEFS_FIR_NONE_ID)
				// F1 in use so set number of output sample pointer to number of output sample field of F1
				psSSRCCtrl->puiNOutSamples	= &psSSRCCtrl->sFIRF1Ctrl.uiNOutSamples;
			else
				// F1 not in use so set number of output sample pointer to number of input sample field
				psSSRCCtrl->puiNOutSamples	= &psSSRCCtrl->uiNInSamples;
		}
	}

	// Call sync function
	if(SSRC_sync(psSSRCCtrl) != SSRC_NO_ERROR)
		return SSRC_ERROR;

	return SSRC_NO_ERROR;
}


// ==================================================================== //
// Function:		SSRC_sync											//
// Arguments:		SSRCCtrl_t 	*psSSRCCtrl: Ctrl strct.				//
// Return values:	SSRC_NO_ERROR on success							//
//					SSRC_ERROR on failure								//
// Description:		Syncs the SSRC passed as argument					//
// ==================================================================== //
SSRCReturnCodes_t				SSRC_sync(SSRCCtrl_t* psSSRCCtrl)
{	
	// Sync the FIR and PPFIR
	if(FIR_sync(&psSSRCCtrl->sFIRF1Ctrl) != FIR_NO_ERROR)
		return SSRC_ERROR;
	if(FIR_sync(&psSSRCCtrl->sFIRF2Ctrl) != FIR_NO_ERROR)
		return SSRC_ERROR;
	if(PPFIR_sync(&psSSRCCtrl->sPPFIRF3Ctrl) != FIR_NO_ERROR)
		return SSRC_ERROR;

	// Reset random seeds to initial values
	psSSRCCtrl->psState->uiRndSeed	= psSSRCCtrl->uiRndSeedInit;

	return SSRC_NO_ERROR;
}
	

// ==================================================================== //
// Function:		SSRC_proc											//
// Arguments:		SSRCCtrl_t 	*psSSRCCtrl: Ctrl strct.				//
// Return values:	SSRC_NO_ERROR on success							//
//					SSRC_ERROR on failure								//
// Description:		Processes the SSRC passed as argument				//
// ==================================================================== //
#pragma stackfunction 128  //Very large stack allocation (probably needs just a handful through F1_F2, ASM etc).

SSRCReturnCodes_t				SSRC_proc(SSRCCtrl_t* psSSRCCtrl)		
{
	// Setup input / output buffers
	// ----------------------------
	psSSRCCtrl->sFIRF1Ctrl.piIn			= psSSRCCtrl->piIn;
	*(psSSRCCtrl->ppiOut)				= psSSRCCtrl->piOut;
	
	// F1 and F2 process
	// -----------------
	//start_timer();
	if( SSRC_proc_F1_F2(psSSRCCtrl) != SSRC_NO_ERROR)
		return SSRC_ERROR;
    //printf("SSRC_proc_F1_F2 took %d ticks\n", stop_timer());

	// F3 process
	// ----------
	//start_timer();
	if( SSRC_proc_F3(psSSRCCtrl)	!= SSRC_NO_ERROR)
		return SSRC_ERROR;	
    //printf("SSRC_proc_F3 took %d ticks\n", stop_timer());

	// Dither process
	// --------------
    //start_timer();
	if( SSRC_proc_dither(psSSRCCtrl) != SSRC_NO_ERROR)
		return SSRC_ERROR;
    //printf("SSRC_proc_dither took %d ticks\n", stop_timer());

	return SSRC_NO_ERROR;
}


// ==================================================================== //
// Function:		SSRC_proc_F1_F2										//
// Arguments:		SSRCCtrl_t 	*psSSRCCtrl: Ctrl strct.				//
// Return values:	SSRC_NO_ERROR on success							//
//					SSRC_ERROR on failure								//
// Description:		Processes F1 and F2 for a channel					//
// ==================================================================== //
SSRCReturnCodes_t				SSRC_proc_F1_F2(SSRCCtrl_t* psSSRCCtrl)
{
	int*			piIn		= psSSRCCtrl->piIn;
	int*			piOut		= psSSRCCtrl->piOut;
	unsigned int	ui;


	// Check if F1 is disabled, in which case we just copy input to output as all filters are disabled
	if(psSSRCCtrl->sFIRF1Ctrl.eEnable == FIR_OFF)
	{
	    //start_timer();
		// F1 is not enabled, which means that we are in 1:1 rate, so just copy input to output
		for(ui = 0; ui < psSSRCCtrl->uiNInSamples; ui+= SSRC_CHANNELS_PER_CORE)
			piOut[ui]		= piIn[ui];

	    //printf("F1 bypass took %d ticks\n", stop_timer());

		return SSRC_NO_ERROR;
	}

	// F1 is enabled, so call F1

    //start_timer();
	if(psSSRCCtrl->sFIRF1Ctrl.pvProc(&psSSRCCtrl->sFIRF1Ctrl) != FIR_NO_ERROR)
		return SSRC_ERROR;
    //printf("F1 took %d ticks\n", stop_timer());


	// Check if F2 is enabled
	if(psSSRCCtrl->sFIRF2Ctrl.eEnable == FIR_ON)
	{
		// F2 is enabled, so call F2
	    //start_timer();
		if(psSSRCCtrl->sFIRF2Ctrl.pvProc(&psSSRCCtrl->sFIRF2Ctrl) != FIR_NO_ERROR)
			return SSRC_ERROR;
	    //printf("F1 took %d ticks\n", stop_timer());

	}

	return SSRC_NO_ERROR;
}


// ==================================================================== //
// Function:		SSRC_proc_F3										//
// Arguments:		SSRCCtrl_t 	*psSSRCCtrl: Ctrl strct.				//
// Return values:	SSRC_NO_ERROR on success							//
//					SSRC_ERROR on failure								//
// Description:		Processes F3 for a channel							//
// ==================================================================== //
SSRCReturnCodes_t				SSRC_proc_F3(SSRCCtrl_t* psSSRCCtrl)
{

	// Check if F3 is enabled
	if(psSSRCCtrl->sPPFIRF3Ctrl.eEnable == FIR_ON)
	{
		// F3 is enabled, so call F3
		if(PPFIR_proc(&psSSRCCtrl->sPPFIRF3Ctrl) != FIR_NO_ERROR)
			return SSRC_ERROR;
	}

	return SSRC_NO_ERROR;
}


// ==================================================================== //
// Function:		SSRC_proc_dither									//
// Arguments:		SSRCCtrl_t 	*psSSRCCtrl: Ctrl strct.				//
// Return values:	SSRC_NO_ERROR on success							//
//					SSRC_ERROR on failure								//
// Description:		Processes dither for a channel						//
// ==================================================================== //
SSRCReturnCodes_t				SSRC_proc_dither(SSRCCtrl_t* psSSRCCtrl)
{
	int*			piData;
	unsigned int	uiR;
	int				iDither;
	__int64			i64Acc;
	unsigned int	ui;


	// Apply dither if required
	if(psSSRCCtrl->uiDitherOnOff == SSRC_DITHER_ON)
	{
		// Get data buffer
		piData	= psSSRCCtrl->piOut;
		// Get random seed
		uiR		= psSSRCCtrl->psState->uiRndSeed;

		// Loop through samples
		for(ui = 0; ui < *(psSSRCCtrl->puiNOutSamples) * SSRC_CHANNELS_PER_CORE; ui += SSRC_CHANNELS_PER_CORE)
		{
			// Compute dither sample (TPDF)
			iDither		= SSRC_DITHER_BIAS;

			uiR			= (unsigned int)(SSRC_R_BASE * uiR);
			uiR			= (unsigned int)(SSRC_R_CONS + uiR);
			iDither		+= ((uiR>>SSRC_RPDF_BITS_SHIFT) & SSRC_RPDF_MASK);

			uiR			= (unsigned int)(SSRC_R_BASE * uiR);
			uiR			= (unsigned int)(SSRC_R_CONS + uiR);
			iDither		+= ((uiR>>SSRC_RPDF_BITS_SHIFT) & SSRC_RPDF_MASK);

			// Use MACC instruction to saturate and dither + signal
			i64Acc		= ((__int64)iDither <<32);	// On XMOS this is not necessary, just load dither in the top word of the ACC register
			MACC(&i64Acc, piData[ui], 0x7FFFFFFF);
			LSAT30(&i64Acc);
			// Extract 32bits result
			EXT30(&piData[ui], i64Acc);

			// Mask to 24bits
			piData[ui]	&= SSRC_DATA24_MASK;

		}

		// Write random seed back
		psSSRCCtrl->psState->uiRndSeed	= uiR;
	}

	return SSRC_NO_ERROR;
}
/**
@verbatim

Copyright (C) 2004,2005  ADDI-DATA GmbH for the source code of this module.

        ADDI-DATA GmbH
        Dieselstrasse 3
        D-77833 Ottersweier
        Tel: +19(0)7223/9493-0
        Fax: +49(0)7223/9493-92
        http://www.addi-data-com
        info@addi-data.com

This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

You shoud also find the complete GPL in the COPYING file accompanying this source code.

@endverbatim
*/
/*
  +-----------------------------------------------------------------------+
  | (C) ADDI-DATA GmbH          Dieselstraße 3       D-77833 Ottersweier  |
  +-----------------------------------------------------------------------+
  | Tel : +49 (0) 7223/9493-0     | email    : info@addi-data.com         |
  | Fax : +49 (0) 7223/9493-92    | Internet : http://www.addi-data.com   |
  +-----------------------------------------------------------------------+
  | Project     : API APCI1710    | Compiler : gcc                        |
  | Module name : INC_CPT.C       | Version  : 2.96                       |
  +-------------------------------+---------------------------------------+
  | Project manager: Eric Stolz   | Date     :  02/12/2002                |
  +-----------------------------------------------------------------------+
  | Description :   APCI-1710 incremental counter module                  |
  |                                                                       |
  |                                                                       |
  +-----------------------------------------------------------------------+
  |                             UPDATES                                   |
  +-----------------------------------------------------------------------+
  |   Date   |   Author  |          Description of updates                |
  +----------+-----------+------------------------------------------------+
  |          |           |                                                |
  |----------|-----------|------------------------------------------------|
  | 08/05/00 | Guinot C  | - 0400/0228 All Function in RING 0             |
  |          |           |   available                                    |
  +-----------------------------------------------------------------------+
  | 29/06/01 | Guinot C. | - 1100/0231 -> 0701/0232                       |
  |          |           | See i_APCI1710_DisableFrequencyMeasurement     |
  +-----------------------------------------------------------------------+
*/

/*
+----------------------------------------------------------------------------+
|                               Included files                               |
+----------------------------------------------------------------------------+
*/

#include "APCI1710_INCCPT.h"

/*
+----------------------------------------------------------------------------+
| INT	i_APCI1710_InsnConfigINCCPT(struct comedi_device *dev,struct comedi_subdevice *s,
struct comedi_insn *insn,unsigned int *data)

+----------------------------------------------------------------------------+
| Task              : Configuration function for INC_CPT                             |
+----------------------------------------------------------------------------+
| Input Parameters  :														 |
+----------------------------------------------------------------------------+
| Output Parameters : *data
+----------------------------------------------------------------------------+
| Return Value      :                 |
+----------------------------------------------------------------------------+
*/

INT i_APCI1710_InsnConfigINCCPT(struct comedi_device * dev, struct comedi_subdevice * s,
	struct comedi_insn * insn, unsigned int * data)
{
	UINT ui_ConfigType;
	INT i_ReturnValue = 0;
	ui_ConfigType = CR_CHAN(insn->chanspec);

	printk("\nINC_CPT");

	devpriv->tsk_Current = current;	// Save the current process task structure
	switch (ui_ConfigType) {
	case APCI1710_INCCPT_INITCOUNTER:
		i_ReturnValue = i_APCI1710_InitCounter(dev,
			CR_AREF(insn->chanspec),
			(BYTE) data[0],
			(BYTE) data[1],
			(BYTE) data[2], (BYTE) data[3], (BYTE) data[4]);
		break;

	case APCI1710_INCCPT_COUNTERAUTOTEST:
		i_ReturnValue = i_APCI1710_CounterAutoTest(dev,
			(PBYTE) & data[0]);
		break;

	case APCI1710_INCCPT_INITINDEX:
		i_ReturnValue = i_APCI1710_InitIndex(dev,
			CR_AREF(insn->chanspec),
			(BYTE) data[0],
			(BYTE) data[1], (BYTE) data[2], (BYTE) data[3]);
		break;

	case APCI1710_INCCPT_INITREFERENCE:
		i_ReturnValue = i_APCI1710_InitReference(dev,
			CR_AREF(insn->chanspec), (BYTE) data[0]);
		break;

	case APCI1710_INCCPT_INITEXTERNALSTROBE:
		i_ReturnValue = i_APCI1710_InitExternalStrobe(dev,
			CR_AREF(insn->chanspec),
			(BYTE) data[0], (BYTE) data[1]);
		break;

	case APCI1710_INCCPT_INITCOMPARELOGIC:
		i_ReturnValue = i_APCI1710_InitCompareLogic(dev,
			CR_AREF(insn->chanspec), (UINT) data[0]);
		break;

	case APCI1710_INCCPT_INITFREQUENCYMEASUREMENT:
		i_ReturnValue = i_APCI1710_InitFrequencyMeasurement(dev,
			CR_AREF(insn->chanspec),
			(BYTE) data[0],
			(BYTE) data[1], (ULONG) data[2], (PULONG) & data[0]);
		break;

	default:
		printk("Insn Config : Config Parameter Wrong\n");

	}

	if (i_ReturnValue >= 0)
		i_ReturnValue = insn->n;
	return (i_ReturnValue);
}

/*
+----------------------------------------------------------------------------+
| Function Name     : _INT_ i_APCI1710_InitCounter                           |
|                               (BYTE_          b_BoardHandle,               |
|                                BYTE_          b_ModulNbr,                  |
|                                BYTE_          b_CounterRange,              |
|                                BYTE_          b_FirstCounterModus,         |
|                                BYTE_          b_FirstCounterOption,        |
|                                BYTE_          b_SecondCounterModus,        |
|                                BYTE_          b_SecondCounterOption)       |
+----------------------------------------------------------------------------+
| Task              : Configure the counter operating mode from selected     |
|                     module (b_ModulNbr). You must calling this function be |
|                     for you call any other function witch access of        |
|                     counters.                                              |
|                                                                            |
|                          Counter range                                     |
|                          -------------                                     |
| +------------------------------------+-----------------------------------+ |
| | Parameter       Passed value       |        Description                | |
| |------------------------------------+-----------------------------------| |
| |b_ModulNbr   APCI1710_16BIT_COUNTER |  The module is configured for     | |
| |                                    |  two 16-bit counter.              | |
| |                                    |  - b_FirstCounterModus and        | |
| |                                    |    b_FirstCounterOption           | |
| |                                    |    configure the first 16 bit     | |
| |                                    |    counter.                       | |
| |                                    |  - b_SecondCounterModus and       | |
| |                                    |    b_SecondCounterOption          | |
| |                                    |    configure the second 16 bit    | |
| |                                    |    counter.                       | |
| |------------------------------------+-----------------------------------| |
| |b_ModulNbr   APCI1710_32BIT_COUNTER |  The module is configured for one | |
| |                                    |  32-bit counter.                  | |
| |                                    |  - b_FirstCounterModus and        | |
| |                                    |    b_FirstCounterOption           | |
| |                                    |    configure the 32 bit counter.  | |
| |                                    |  - b_SecondCounterModus and       | |
| |                                    |    b_SecondCounterOption          | |
| |                                    |    are not used and have no       | |
| |                                    |    importance.                    | |
| +------------------------------------+-----------------------------------+ |
|                                                                            |
|                      Counter operating mode                                |
|                      ----------------------                                |
|                                                                            |
| +--------------------+-------------------------+-------------------------+ |
| |    Parameter       |     Passed value        |    Description          | |
| |--------------------+-------------------------+-------------------------| |
| |b_FirstCounterModus | APCI1710_QUADRUPLE_MODE | In the quadruple mode,  | |
| |       or           |                         | the edge analysis       | |
| |b_SecondCounterModus|                         | circuit generates a     | |
| |                    |                         | counting pulse from     | |
| |                    |                         | each edge of 2 signals  | |
| |                    |                         | which are phase shifted | |
| |                    |                         | in relation to each     | |
| |                    |                         | other.                  | |
| |--------------------+-------------------------+-------------------------| |
| |b_FirstCounterModus |   APCI1710_DOUBLE_MODE  | Functions in the same   | |
| |       or           |                         | way as the quadruple    | |
| |b_SecondCounterModus|                         | mode, except that only  | |
| |                    |                         | two of the four edges   | |
| |                    |                         | are analysed per        | |
| |                    |                         | period                  | |
| |--------------------+-------------------------+-------------------------| |
| |b_FirstCounterModus |   APCI1710_SIMPLE_MODE  | Functions in the same   | |
| |       or           |                         | way as the quadruple    | |
| |b_SecondCounterModus|                         | mode, except that only  | |
| |                    |                         | one of the four edges   | |
| |                    |                         | is analysed per         | |
| |                    |                         | period.                 | |
| |--------------------+-------------------------+-------------------------| |
| |b_FirstCounterModus |   APCI1710_DIRECT_MODE  | In the direct mode the  | |
| |       or           |                         | both edge analysis      | |
| |b_SecondCounterModus|                         | circuits are inactive.  | |
| |                    |                         | The inputs A, B in the  | |
| |                    |                         | 32-bit mode or A, B and | |
| |                    |                         | C, D in the 16-bit mode | |
| |                    |                         | represent, each, one    | |
| |                    |                         | clock pulse gate circuit| |
| |                    |                         | There by frequency and  | |
| |                    |                         | pulse duration          | |
| |                    |                         | measurements can be     | |
| |                    |                         | performed.              | |
| +--------------------+-------------------------+-------------------------+ |
|                                                                            |
|                                                                            |
|       IMPORTANT!                                                           |
|       If you have configured the module for two 16-bit counter, a mixed    |
|       mode with a counter in quadruple/double/single mode                  |
|       and the other counter in direct mode is not possible!                |
|                                                                            |
|                                                                            |
|         Counter operating option for quadruple/double/simple mode          |
|         ---------------------------------------------------------          |
|                                                                            |
| +----------------------+-------------------------+------------------------+|
| |       Parameter      |     Passed value        |  Description           ||
| |----------------------+-------------------------+------------------------||
| |b_FirstCounterOption  | APCI1710_HYSTERESIS_ON  | In both edge analysis  ||
| |        or            |                         | circuits is available  ||
| |b_SecondCounterOption |                         | one hysteresis circuit.||
| |                      |                         | It suppresses each     ||
| |                      |                         | time the first counting||
| |                      |                         | pulse after a change   ||
| |                      |                         | of rotation.           ||
| |----------------------+-------------------------+------------------------||
| |b_FirstCounterOption  | APCI1710_HYSTERESIS_OFF | The first counting     ||
| |       or             |                         | pulse is not suppress  ||
| |b_SecondCounterOption |                         | after a change of      ||
| |                      |                         | rotation.              ||
| +----------------------+-------------------------+------------------------+|
|                                                                            |
|                                                                            |
|       IMPORTANT!                                                           |
|       This option are only avaible if you have selected the direct mode.   |
|                                                                            |
|                                                                            |
|               Counter operating option for direct mode                     |
|               ----------------------------------------                     |
|                                                                            |
| +----------------------+--------------------+----------------------------+ |
| |      Parameter       |     Passed value   |       Description          | |
| |----------------------+--------------------+----------------------------| |
| |b_FirstCounterOption  | APCI1710_INCREMENT | The counter increment for  | |
| |       or             |                    | each counting pulse        | |
| |b_SecondCounterOption |                    |                            | |
| |----------------------+--------------------+----------------------------| |
| |b_FirstCounterOption  | APCI1710_DECREMENT | The counter decrement for  | |
| |       or             |                    | each counting pulse        | |
| |b_SecondCounterOption |                    |                            | |
| +----------------------+--------------------+----------------------------+ |
|                                                                            |
+----------------------------------------------------------------------------+
| Input Parameters  : BYTE_ b_BoardHandle         : Handle of board APCI-1710|
|                     BYTE_ b_ModulNbr            : Module number to         |
|                                                   configure (0 to 3)       |
|                     BYTE_ b_CounterRange        : Selection form counter   |
|                                                   range.                   |
|                     BYTE_ b_FirstCounterModus   : First counter operating  |
|                                                   mode.                    |
|                     BYTE_ b_FirstCounterOption  : First counter  option.   |
|                     BYTE_ b_SecondCounterModus  : Second counter operating |
|                                                   mode.                    |
|                     BYTE_ b_SecondCounterOption : Second counter  option.  |
+----------------------------------------------------------------------------+
| Output Parameters : -                                                      |
+----------------------------------------------------------------------------+
| Return Value      : 0: No error                                            |
|                    -1: The handle parameter of the board is wrong          |
|                    -2: The module is not a counter module                  |
|                    -3: The selected counter range is wrong.                |
|                    -4: The selected first counter operating mode is wrong. |
|                    -5: The selected first counter operating option is wrong|
|                    -6: The selected second counter operating mode is wrong.|
|                    -7: The selected second counter operating option is     |
|                        wrong.                                              |
+----------------------------------------------------------------------------+
*/

INT i_APCI1710_InitCounter(struct comedi_device * dev,
	BYTE b_ModulNbr,
	BYTE b_CounterRange,
	BYTE b_FirstCounterModus,
	BYTE b_FirstCounterOption,
	BYTE b_SecondCounterModus, BYTE b_SecondCounterOption)
{
	INT i_ReturnValue = 0;

	/*******************************/
	/* Test if incremental counter */
	/*******************************/

	if ((devpriv->s_BoardInfos.
			dw_MolduleConfiguration[b_ModulNbr] & 0xFFFF0000UL) ==
		APCI1710_INCREMENTAL_COUNTER) {
	   /**************************/
		/* Test the counter range */
	   /**************************/

		if (b_CounterRange == APCI1710_16BIT_COUNTER
			|| b_CounterRange == APCI1710_32BIT_COUNTER) {
	      /********************************/
			/* Test the first counter modus */
	      /********************************/

			if (b_FirstCounterModus == APCI1710_QUADRUPLE_MODE ||
				b_FirstCounterModus == APCI1710_DOUBLE_MODE ||
				b_FirstCounterModus == APCI1710_SIMPLE_MODE ||
				b_FirstCounterModus == APCI1710_DIRECT_MODE) {
		 /*********************************/
				/* Test the first counter option */
		 /*********************************/

				if ((b_FirstCounterModus == APCI1710_DIRECT_MODE
						&& (b_FirstCounterOption ==
							APCI1710_INCREMENT
							|| b_FirstCounterOption
							== APCI1710_DECREMENT))
					|| (b_FirstCounterModus !=
						APCI1710_DIRECT_MODE
						&& (b_FirstCounterOption ==
							APCI1710_HYSTERESIS_ON
							|| b_FirstCounterOption
							==
							APCI1710_HYSTERESIS_OFF)))
				{
		    /**************************/
					/* Test if 16-bit counter */
		    /**************************/

					if (b_CounterRange ==
						APCI1710_16BIT_COUNTER) {
		       /*********************************/
						/* Test the second counter modus */
		       /*********************************/

						if ((b_FirstCounterModus !=
								APCI1710_DIRECT_MODE
								&&
								(b_SecondCounterModus
									==
									APCI1710_QUADRUPLE_MODE
									||
									b_SecondCounterModus
									==
									APCI1710_DOUBLE_MODE
									||
									b_SecondCounterModus
									==
									APCI1710_SIMPLE_MODE))
							|| (b_FirstCounterModus
								==
								APCI1710_DIRECT_MODE
								&&
								b_SecondCounterModus
								==
								APCI1710_DIRECT_MODE))
						{
			  /**********************************/
							/* Test the second counter option */
			  /**********************************/

							if ((b_SecondCounterModus == APCI1710_DIRECT_MODE && (b_SecondCounterOption == APCI1710_INCREMENT || b_SecondCounterOption == APCI1710_DECREMENT)) || (b_SecondCounterModus != APCI1710_DIRECT_MODE && (b_SecondCounterOption == APCI1710_HYSTERESIS_ON || b_SecondCounterOption == APCI1710_HYSTERESIS_OFF))) {
								i_ReturnValue =
									0;
							} else {
			     /*********************************************************/
								/* The selected second counter operating option is wrong */
			     /*********************************************************/

								DPRINTK("The selected second counter operating option is wrong\n");
								i_ReturnValue =
									-7;
							}
						} else {
			  /*******************************************************/
							/* The selected second counter operating mode is wrong */
			  /*******************************************************/

							DPRINTK("The selected second counter operating mode is wrong\n");
							i_ReturnValue = -6;
						}
					}
				} else {
		    /********************************************************/
					/* The selected first counter operating option is wrong */
		    /********************************************************/

					DPRINTK("The selected first counter operating option is wrong\n");
					i_ReturnValue = -5;
				}
			} else {
		 /******************************************************/
				/* The selected first counter operating mode is wrong */
		 /******************************************************/
				DPRINTK("The selected first counter operating mode is wrong\n");
				i_ReturnValue = -4;
			}
		} else {
	      /***************************************/
			/* The selected counter range is wrong */
	      /***************************************/

			DPRINTK("The selected counter range is wrong\n");
			i_ReturnValue = -3;
		}

	   /*************************/
		/* Test if a error occur */
	   /*************************/

		if (i_ReturnValue == 0) {
	      /**************************/
			/* Test if 16-Bit counter */
	      /**************************/

			if (b_CounterRange == APCI1710_32BIT_COUNTER) {
				devpriv->
					s_ModuleInfo[b_ModulNbr].
					s_SiemensCounterInfo.
					s_ModeRegister.
					s_ByteModeRegister.
					b_ModeRegister1 = b_CounterRange |
					b_FirstCounterModus |
					b_FirstCounterOption;
			} else {
				devpriv->
					s_ModuleInfo[b_ModulNbr].
					s_SiemensCounterInfo.
					s_ModeRegister.
					s_ByteModeRegister.
					b_ModeRegister1 = b_CounterRange |
					(b_FirstCounterModus & 0x5) |
					(b_FirstCounterOption & 0x20) |
					(b_SecondCounterModus & 0xA) |
					(b_SecondCounterOption & 0x40);

		 /***********************/
				/* Test if direct mode */
		 /***********************/

				if (b_FirstCounterModus == APCI1710_DIRECT_MODE) {
					devpriv->
						s_ModuleInfo[b_ModulNbr].
						s_SiemensCounterInfo.
						s_ModeRegister.
						s_ByteModeRegister.
						b_ModeRegister1 = devpriv->
						s_ModuleInfo[b_ModulNbr].
						s_SiemensCounterInfo.
						s_ModeRegister.
						s_ByteModeRegister.
						b_ModeRegister1 |
						APCI1710_DIRECT_MODE;
				}
			}

	      /***************************/
			/* Write the configuration */
	      /***************************/

			outl(devpriv->s_ModuleInfo[b_ModulNbr].
				s_SiemensCounterInfo.
				s_ModeRegister.
				dw_ModeRegister1_2_3_4,
				devpriv->s_BoardInfos.
				ui_Address + 20 + (64 * b_ModulNbr));

			devpriv->
				s_ModuleInfo[b_ModulNbr].
				s_SiemensCounterInfo.
				s_InitFlag.b_CounterInit = 1;
		}
	} else {
	   /**************************************/
		/* The module is not a counter module */
	   /**************************************/

		DPRINTK("The module is not a counter module\n");
		i_ReturnValue = -2;
	}

	return (i_ReturnValue);
}

/*
+----------------------------------------------------------------------------+
| Function Name     : _INT_ i_APCI1710_CounterAutoTest                       |
|                                               (BYTE_     b_BoardHandle,    |
|                                                PBYTE_   pb_TestStatus)     |
+----------------------------------------------------------------------------+
| Task              : A test mode is intended for testing the component and  |
|                     the connected periphery. All the 8-bit counter chains  |
|                     are operated internally as down counters.              |
|                     Independently from the external signals,               |
|                     all the four 8-bit counter chains are decremented in   |
|                     parallel by each negative clock pulse edge of CLKX.    |
|                                                                            |
|                       Counter auto test conclusion                         |
|                       ----------------------------                         |
|              +-----------------+-----------------------------+             |
|              | pb_TestStatus   |    Error description        |             |
|              |     mask        |                             |             |
|              |-----------------+-----------------------------|             |
|              |    0000         |     No error detected       |             |
|              |-----------------|-----------------------------|             |
|              |    0001         | Error detected of counter 0 |             |
|              |-----------------|-----------------------------|             |
|              |    0010         | Error detected of counter 1 |             |
|              |-----------------|-----------------------------|             |
|              |    0100         | Error detected of counter 2 |             |
|              |-----------------|-----------------------------|             |
|              |    1000         | Error detected of counter 3 |             |
|              +-----------------+-----------------------------+             |
+----------------------------------------------------------------------------+
| Input Parameters  : BYTE_   b_BoardHandle : Handle of board APCI-1710      |  |
+----------------------------------------------------------------------------+
| Output Parameters : PBYTE_ pb_TestStatus  : Auto test conclusion. See table|
+----------------------------------------------------------------------------+
| Return Value      :  0: No error                                           |
|                     -1: The handle parameter of the board is wrong         |
|                     -2: No counter module found                            |
+----------------------------------------------------------------------------+
*/

INT i_APCI1710_CounterAutoTest(struct comedi_device * dev, PBYTE pb_TestStatus)
{
	BYTE b_ModulCpt = 0;
	INT i_ReturnValue = 0;
	DWORD dw_LathchValue;

	*pb_TestStatus = 0;

	/********************************/
	/* Test if counter module found */
	/********************************/

	if ((devpriv->s_BoardInfos.
			dw_MolduleConfiguration[0] & 0xFFFF0000UL) ==
		APCI1710_INCREMENTAL_COUNTER
		|| (devpriv->s_BoardInfos.
			dw_MolduleConfiguration[1] & 0xFFFF0000UL) ==
		APCI1710_INCREMENTAL_COUNTER
		|| (devpriv->s_BoardInfos.
			dw_MolduleConfiguration[2] & 0xFFFF0000UL) ==
		APCI1710_INCREMENTAL_COUNTER
		|| (devpriv->s_BoardInfos.
			dw_MolduleConfiguration[3] & 0xFFFF0000UL) ==
		APCI1710_INCREMENTAL_COUNTER) {
		for (b_ModulCpt = 0; b_ModulCpt < 4; b_ModulCpt++) {
	      /*******************************/
			/* Test if incremental counter */
	      /*******************************/

			if ((devpriv->s_BoardInfos.
					dw_MolduleConfiguration[b_ModulCpt] &
					0xFFFF0000UL) ==
				APCI1710_INCREMENTAL_COUNTER) {
		 /******************/
				/* Start the test */
		 /******************/

				outl(3, devpriv->s_BoardInfos.
					ui_Address + 16 + (64 * b_ModulCpt));

		 /*********************/
				/* Tatch the counter */
		 /*********************/

				outl(1, devpriv->s_BoardInfos.
					ui_Address + (64 * b_ModulCpt));

		 /************************/
				/* Read the latch value */
		 /************************/

				dw_LathchValue = inl(devpriv->s_BoardInfos.
					ui_Address + 4 + (64 * b_ModulCpt));

				if ((dw_LathchValue & 0xFF) !=
					((dw_LathchValue >> 8) & 0xFF)
					&& (dw_LathchValue & 0xFF) !=
					((dw_LathchValue >> 16) & 0xFF)
					&& (dw_LathchValue & 0xFF) !=
					((dw_LathchValue >> 24) & 0xFF)) {
					*pb_TestStatus =
						*pb_TestStatus | (1 <<
						b_ModulCpt);
				}

		 /*****************/
				/* Stop the test */
		 /*****************/

				outl(0, devpriv->s_BoardInfos.
					ui_Address + 16 + (64 * b_ModulCpt));
			}
		}
	} else {
	   /***************************/
		/* No counter module found */
	   /***************************/

		DPRINTK("No counter module found\n");
		i_ReturnValue = -2;
	}

	return (i_ReturnValue);
}

/*
+----------------------------------------------------------------------------+
| Function Name     : _INT_ i_APCI1710_InitIndex (BYTE_ b_BoardHandle,       |
|                                                 BYTE_ b_ModulNbr,          |
|                                                 BYTE_ b_ReferenceAction,   |
|                                                 BYTE_ b_IndexOperation,    |
|                                                 BYTE_ b_AutoMode,          |
|                                                 BYTE_ b_InterruptEnable)   |
+----------------------------------------------------------------------------+
| Task              : Initialise the index corresponding to the selected     |
|                     module (b_ModulNbr). If a INDEX flag occur, you have   |
|                     the possibility to clear the 32-Bit counter or to latch|
|                     the current 32-Bit value in to the first latch         |
|                     register. The b_IndexOperation parameter give the      |
|                     possibility to choice the INDEX action.                |
|                     If you have enabled the automatic mode, each INDEX     |
|                     action is cleared automatically, else you must read    |
|                     the index status ("i_APCI1710_ReadIndexStatus")        |
|                     after each INDEX action.                               |
|                                                                            |
|                                                                            |
|                               Index action                                 |
|                               ------------                                 |
|                                                                            |
|           +------------------------+------------------------------------+  |
|           |   b_IndexOperation     |         Operation                  |  |
|           |------------------------+------------------------------------|  |
|           |APCI1710_LATCH_COUNTER  | After a index signal, the counter  |  |
|           |                        | value (32-Bit) is latched in to    |  |
|           |                        | the first latch register           |  |
|           |------------------------|------------------------------------|  |
|           |APCI1710_CLEAR_COUNTER  | After a index signal, the counter  |  |
|           |                        | value is cleared (32-Bit)          |  |
|           +------------------------+------------------------------------+  |
+----------------------------------------------------------------------------+
| Input Parameters  : BYTE_ b_BoardHandle     : Handle of board APCI-1710    |
|                     BYTE_ b_ModulNbr        : Module number to configure   |
|                                               (0 to 3)                     |
|                     BYTE_ b_ReferenceAction : Determine if the reference   |
|                                               must set or no for the       |
|                                               acceptance from index        |
|                                               APCI1710_ENABLE :            |
|                                                  Reference must be set for |
|                                                  accepted the index        |
|                                               APCI1710_DISABLE :           |
|                                                  Reference have not        |
|                                                  importance                |
|                     BYTE_ b_IndexOperation  : Index operating mode.        |
|                                               See table.                   |
|                     BYTE_ b_AutoMode        : Enable or disable the        |
|                                               automatic index reset.       |
|                                               APCI1710_ENABLE :            |
|                                                 Enable the automatic mode  |
|                                               APCI1710_DISABLE :           |
|                                                 Disable the automatic mode |
|                     BYTE_ b_InterruptEnable : Enable or disable the        |
|                                               interrupt.                   |
|                                               APCI1710_ENABLE :            |
|                                               Enable the interrupt         |
|                                               APCI1710_DISABLE :           |
|                                               Disable the interrupt        |
+----------------------------------------------------------------------------+
| Output Parameters : -                                                      |
+----------------------------------------------------------------------------+
| Return Value      :  0: No error                                           |
|                     -1: The handle parameter of the board is wrong         |
|                     -2: No counter module found                            |
|                     -3: Counter not initialised see function               |
|                         "i_APCI1710_InitCounter"                           |
|                     -4  The reference action parameter is wrong            |
|                     -5: The index operating mode parameter is wrong        |
|                     -6: The auto mode parameter is wrong                   |
|                     -7: Interrupt parameter is wrong                       |
|                     -8: Interrupt function not initialised.                |
|                         See function "i_APCI1710_SetBoardIntRoutineX"      |
+----------------------------------------------------------------------------+
*/

INT i_APCI1710_InitIndex(struct comedi_device * dev,
	BYTE b_ModulNbr,
	BYTE b_ReferenceAction,
	BYTE b_IndexOperation, BYTE b_AutoMode, BYTE b_InterruptEnable)
{
	INT i_ReturnValue = 0;

	/**************************/
	/* Test the module number */
	/**************************/

	if (b_ModulNbr < 4) {
	   /*******************************/
		/* Test if counter initialised */
	   /*******************************/

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {
	      /********************************/
			/* Test the reference parameter */
	      /********************************/

			if (b_ReferenceAction == APCI1710_ENABLE ||
				b_ReferenceAction == APCI1710_DISABLE) {
		 /****************************/
				/* Test the index parameter */
		 /****************************/

				if (b_IndexOperation ==
					APCI1710_HIGH_EDGE_LATCH_COUNTER
					|| b_IndexOperation ==
					APCI1710_LOW_EDGE_LATCH_COUNTER
					|| b_IndexOperation ==
					APCI1710_HIGH_EDGE_CLEAR_COUNTER
					|| b_IndexOperation ==
					APCI1710_LOW_EDGE_CLEAR_COUNTER
					|| b_IndexOperation ==
					APCI1710_HIGH_EDGE_LATCH_AND_CLEAR_COUNTER
					|| b_IndexOperation ==
					APCI1710_LOW_EDGE_LATCH_AND_CLEAR_COUNTER)
				{
		    /********************************/
					/* Test the auto mode parameter */
		    /********************************/

					if (b_AutoMode == APCI1710_ENABLE ||
						b_AutoMode == APCI1710_DISABLE)
					{
		       /***************************/
						/* Test the interrupt mode */
		       /***************************/

						if (b_InterruptEnable ==
							APCI1710_ENABLE
							|| b_InterruptEnable ==
							APCI1710_DISABLE) {

			     /************************************/
							/* Makte the configuration commando */
			     /************************************/

							if (b_ReferenceAction ==
								APCI1710_ENABLE)
							{
								devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister2
									=
									devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister2
									|
									APCI1710_ENABLE_INDEX_ACTION;
							} else {
								devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister2
									=
									devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister2
									&
									APCI1710_DISABLE_INDEX_ACTION;
							}

			     /****************************************/
							/* Test if low level latch or/and clear */
			     /****************************************/

							if (b_IndexOperation ==
								APCI1710_LOW_EDGE_LATCH_COUNTER
								||
								b_IndexOperation
								==
								APCI1710_LOW_EDGE_CLEAR_COUNTER
								||
								b_IndexOperation
								==
								APCI1710_LOW_EDGE_LATCH_AND_CLEAR_COUNTER)
							{
				/*************************************/
								/* Set the index level to low (DQ26) */
				/*************************************/

								devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister4
									=
									devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister4
									|
									APCI1710_SET_LOW_INDEX_LEVEL;
							} else {
				/**************************************/
								/* Set the index level to high (DQ26) */
				/**************************************/

								devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister4
									=
									devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister4
									&
									APCI1710_SET_HIGH_INDEX_LEVEL;
							}

			     /***********************************/
							/* Test if latch and clear counter */
			     /***********************************/

							if (b_IndexOperation ==
								APCI1710_HIGH_EDGE_LATCH_AND_CLEAR_COUNTER
								||
								b_IndexOperation
								==
								APCI1710_LOW_EDGE_LATCH_AND_CLEAR_COUNTER)
							{
				/***************************************/
								/* Set the latch and clear flag (DQ27) */
				/***************************************/

								devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister4
									=
									devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister4
									|
									APCI1710_ENABLE_LATCH_AND_CLEAR;
							}	// if (b_IndexOperation == APCI1710_HIGH_EDGE_LATCH_AND_CLEAR_COUNTER || b_IndexOperation == APCI1710_LOW_EDGE_LATCH_AND_CLEAR_COUNTER)
							else {
				/*****************************************/
								/* Clear the latch and clear flag (DQ27) */
				/*****************************************/

								devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister4
									=
									devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister4
									&
									APCI1710_DISABLE_LATCH_AND_CLEAR;

				/*************************/
								/* Test if latch counter */
				/*************************/

								if (b_IndexOperation == APCI1710_HIGH_EDGE_LATCH_COUNTER || b_IndexOperation == APCI1710_LOW_EDGE_LATCH_COUNTER) {
				   /*********************************/
									/* Enable the latch from counter */
				   /*********************************/

									devpriv->
										s_ModuleInfo
										[b_ModulNbr].
										s_SiemensCounterInfo.
										s_ModeRegister.
										s_ByteModeRegister.
										b_ModeRegister2
										=
										devpriv->
										s_ModuleInfo
										[b_ModulNbr].
										s_SiemensCounterInfo.
										s_ModeRegister.
										s_ByteModeRegister.
										b_ModeRegister2
										|
										APCI1710_INDEX_LATCH_COUNTER;
								} else {
				   /*********************************/
									/* Enable the clear from counter */
				   /*********************************/

									devpriv->
										s_ModuleInfo
										[b_ModulNbr].
										s_SiemensCounterInfo.
										s_ModeRegister.
										s_ByteModeRegister.
										b_ModeRegister2
										=
										devpriv->
										s_ModuleInfo
										[b_ModulNbr].
										s_SiemensCounterInfo.
										s_ModeRegister.
										s_ByteModeRegister.
										b_ModeRegister2
										&
										(~APCI1710_INDEX_LATCH_COUNTER);
								}
							}	// // if (b_IndexOperation == APCI1710_HIGH_EDGE_LATCH_AND_CLEAR_COUNTER || b_IndexOperation == APCI1710_LOW_EDGE_LATCH_AND_CLEAR_COUNTER)

							if (b_AutoMode ==
								APCI1710_DISABLE)
							{
								devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister2
									=
									devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister2
									|
									APCI1710_INDEX_AUTO_MODE;
							} else {
								devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister2
									=
									devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister2
									&
									(~APCI1710_INDEX_AUTO_MODE);
							}

							if (b_InterruptEnable ==
								APCI1710_ENABLE)
							{
								devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister3
									=
									devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister3
									|
									APCI1710_ENABLE_INDEX_INT;
							} else {
								devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister3
									=
									devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister3
									&
									APCI1710_DISABLE_INDEX_INT;
							}

							devpriv->
								s_ModuleInfo
								[b_ModulNbr].
								s_SiemensCounterInfo.
								s_InitFlag.
								b_IndexInit = 1;

						} else {
			  /********************************/
							/* Interrupt parameter is wrong */
			  /********************************/
							DPRINTK("Interrupt parameter is wrong\n");
							i_ReturnValue = -7;
						}
					} else {
		       /************************************/
						/* The auto mode parameter is wrong */
		       /************************************/

						DPRINTK("The auto mode parameter is wrong\n");
						i_ReturnValue = -6;
					}
				} else {
		    /***********************************************/
					/* The index operating mode parameter is wrong */
		    /***********************************************/

					DPRINTK("The index operating mode parameter is wrong\n");
					i_ReturnValue = -5;
				}
			} else {
		 /*******************************************/
				/* The reference action parameter is wrong */
		 /*******************************************/

				DPRINTK("The reference action parameter is wrong\n");
				i_ReturnValue = -4;
			}
		} else {
	      /****************************************/
			/* Counter not initialised see function */
			/* "i_APCI1710_InitCounter"             */
	      /****************************************/

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   /*************************************************/
		/* The selected module number parameter is wrong */
	   /*************************************************/

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return (i_ReturnValue);
}

/*
+----------------------------------------------------------------------------+
| Function Name     : _INT_ i_APCI1710_InitReference                         |
|                                                (BYTE_ b_BoardHandle,       |
|                                                 BYTE_ b_ModulNbr,          |
|                                                 BYTE_ b_ReferenceLevel)    |
+----------------------------------------------------------------------------+
| Task              : Initialise the reference corresponding to the selected |
|                     module (b_ModulNbr).                                   |
|                                                                            |
|                               Reference level                              |
|                               ---------------                              |
|             +--------------------+-------------------------+               |
|             | b_ReferenceLevel   |         Operation       |               |
|             +--------------------+-------------------------+               |
|             |   APCI1710_LOW     |  Reference occur if "0" |               |
|             |--------------------|-------------------------|               |
|             |   APCI1710_HIGH    |  Reference occur if "1" |               |
|             +--------------------+-------------------------+               |
+----------------------------------------------------------------------------+
| Input Parameters  : BYTE_ b_BoardHandle     : Handle of board APCI-1710    |
|                     BYTE_ b_ModulNbr        : Module number to configure   |
|                                               (0 to 3)                     |
|                     BYTE_ b_ReferenceLevel  : Reference level.             |
+----------------------------------------------------------------------------+
| Output Parameters : -                                                      |
+----------------------------------------------------------------------------+
| Return Value      :  0: No error                                           |
|                     -1: The handle parameter of the board is wrong         |
|                     -2: The selected module number parameter is wrong      |
|                     -3: Counter not initialised see function               |
|                         "i_APCI1710_InitCounter"                           |
|                     -4: Reference level parameter is wrong                 |
+----------------------------------------------------------------------------+
*/

INT i_APCI1710_InitReference(struct comedi_device * dev,
	BYTE b_ModulNbr, BYTE b_ReferenceLevel)
{
	INT i_ReturnValue = 0;

	/**************************/
	/* Test the module number */
	/**************************/

	if (b_ModulNbr < 4) {
	   /*******************************/
		/* Test if counter initialised */
	   /*******************************/

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {
	      /**************************************/
			/* Test the reference level parameter */
	      /**************************************/

			if (b_ReferenceLevel == 0 || b_ReferenceLevel == 1) {
				if (b_ReferenceLevel == 1) {
					devpriv->
						s_ModuleInfo[b_ModulNbr].
						s_SiemensCounterInfo.
						s_ModeRegister.
						s_ByteModeRegister.
						b_ModeRegister2 = devpriv->
						s_ModuleInfo[b_ModulNbr].
						s_SiemensCounterInfo.
						s_ModeRegister.
						s_ByteModeRegister.
						b_ModeRegister2 |
						APCI1710_REFERENCE_HIGH;
				} else {
					devpriv->
						s_ModuleInfo[b_ModulNbr].
						s_SiemensCounterInfo.
						s_ModeRegister.
						s_ByteModeRegister.
						b_ModeRegister2 = devpriv->
						s_ModuleInfo[b_ModulNbr].
						s_SiemensCounterInfo.
						s_ModeRegister.
						s_ByteModeRegister.
						b_ModeRegister2 &
						APCI1710_REFERENCE_LOW;
				}

				outl(devpriv->s_ModuleInfo[b_ModulNbr].
					s_SiemensCounterInfo.
					s_ModeRegister.
					dw_ModeRegister1_2_3_4,
					devpriv->s_BoardInfos.ui_Address + 20 +
					(64 * b_ModulNbr));

				devpriv->
					s_ModuleInfo[b_ModulNbr].
					s_SiemensCounterInfo.
					s_InitFlag.b_ReferenceInit = 1;
			} else {
		 /**************************************/
				/* Reference level parameter is wrong */
		 /**************************************/

				DPRINTK("Reference level parameter is wrong\n");
				i_ReturnValue = -4;
			}
		} else {
	      /****************************************/
			/* Counter not initialised see function */
			/* "i_APCI1710_InitCounter"             */
	      /****************************************/

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   /*************************************************/
		/* The selected module number parameter is wrong */
	   /*************************************************/

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return (i_ReturnValue);
}

/*
+----------------------------------------------------------------------------+
| Function Name     : _INT_	i_APCI1710_InitExternalStrobe                |
|					(BYTE_ b_BoardHandle,                |
|					 BYTE_ b_ModulNbr,                   |
|					 BYTE_ b_ExternalStrobe,             |
|					 BYTE_ b_ExternalStrobeLevel)        |
+----------------------------------------------------------------------------+
| Task              : Initialises the external strobe level corresponding to |
|		      the selected module (b_ModulNbr).                      |
+----------------------------------------------------------------------------+
| Input Parameters  : BYTE_ b_BoardHandle     : Handle of board APCI-1710    |
|                     BYTE_ b_ModulNbr        : Module number to configure   |
|                                               (0 to 3)                     |
|		      BYTE_ b_ExternalStrobe  : External strobe selection    |
|						0 : External strobe A        |
|						1 : External strobe B        |
|		      BYTE_ b_ExternalStrobeLevel : External strobe level    |
|						APCI1710_LOW :               |
|						External latch occurs if "0" |
|						APCI1710_HIGH :              |
|						External latch occurs if "1" |
+----------------------------------------------------------------------------+
| Output Parameters : -                                                      |
+----------------------------------------------------------------------------+
| Return Value      :  0: No error                                           |
|                     -1: The handle parameter of the board is wrong         |
|                     -2: The selected module number is wrong                |
|                     -3: Counter not initialised.                           |
|			  See function "i_APCI1710_InitCounter"              |
|                     -4: External strobe selection is wrong                 |
|                     -5: External strobe level parameter is wrong           |
+----------------------------------------------------------------------------+
*/

INT i_APCI1710_InitExternalStrobe(struct comedi_device * dev,
	BYTE b_ModulNbr, BYTE b_ExternalStrobe, BYTE b_ExternalStrobeLevel)
{
	INT i_ReturnValue = 0;

	/**************************/
	/* Test the module number */
	/**************************/

	if (b_ModulNbr < 4) {
	   /*******************************/
		/* Test if counter initialised */
	   /*******************************/

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {
	      /**************************************/
			/* Test the external strobe selection */
	      /**************************************/

			if (b_ExternalStrobe == 0 || b_ExternalStrobe == 1) {
		 /******************/
				/* Test the level */
		 /******************/

				if ((b_ExternalStrobeLevel == APCI1710_HIGH) ||
					((b_ExternalStrobeLevel == APCI1710_LOW
							&& (devpriv->
								s_BoardInfos.
								dw_MolduleConfiguration
								[b_ModulNbr] &
								0xFFFF) >=
							0x3135))) {
		    /*****************/
					/* Set the level */
		    /*****************/

					devpriv->
						s_ModuleInfo[b_ModulNbr].
						s_SiemensCounterInfo.
						s_ModeRegister.
						s_ByteModeRegister.
						b_ModeRegister4 = (devpriv->
						s_ModuleInfo[b_ModulNbr].
						s_SiemensCounterInfo.
						s_ModeRegister.
						s_ByteModeRegister.
						b_ModeRegister4 & (0xFF -
							(0x10 << b_ExternalStrobe))) | ((b_ExternalStrobeLevel ^ 1) << (4 + b_ExternalStrobe));
				} else {
		    /********************************************/
					/* External strobe level parameter is wrong */
		    /********************************************/

					DPRINTK("External strobe level parameter is wrong\n");
					i_ReturnValue = -5;
				}
			}	// if (b_ExternalStrobe == 0 || b_ExternalStrobe == 1)
			else {
		 /**************************************/
				/* External strobe selection is wrong */
		 /**************************************/

				DPRINTK("External strobe selection is wrong\n");
				i_ReturnValue = -4;
			}	// if (b_ExternalStrobe == 0 || b_ExternalStrobe == 1)
		} else {
	      /****************************************/
			/* Counter not initialised see function */
			/* "i_APCI1710_InitCounter"             */
	      /****************************************/

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   /*************************************************/
		/* The selected module number parameter is wrong */
	   /*************************************************/

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return (i_ReturnValue);
}

	/*
	   +----------------------------------------------------------------------------+
	   | Function Name     : _INT_ i_APCI1710_InitCompareLogic                      |
	   |                               (BYTE_   b_BoardHandle,                      |
	   |                                BYTE_   b_ModulNbr,                         |
	   |                                UINT_  ui_CompareValue)                     |
	   +----------------------------------------------------------------------------+
	   | Task              : Set the 32-Bit compare value. At that moment that the  |
	   |                     incremental counter arrive to the compare value        |
	   |                     (ui_CompareValue) a interrupt is generated.            |
	   +----------------------------------------------------------------------------+
	   | Input Parameters  : BYTE_  b_BoardHandle    : Handle of board APCI-1710    |
	   |                     BYTE_  b_ModulNbr       : Module number to configure   |
	   |                                               (0 to 3)                     |
	   |                     UINT_ ui_CompareValue   : 32-Bit compare value         |
	   +----------------------------------------------------------------------------+
	   | Output Parameters : -
	   +----------------------------------------------------------------------------+
	   | Return Value      :  0: No error                                           |
	   |                     -1: The handle parameter of the board is wrong         |
	   |                     -2: No counter module found                            |
	   |                     -3: Counter not initialised see function               |
	   |                         "i_APCI1710_InitCounter"                           |
	   +----------------------------------------------------------------------------+
	 */

INT i_APCI1710_InitCompareLogic(struct comedi_device * dev,
	BYTE b_ModulNbr, UINT ui_CompareValue)
{
	INT i_ReturnValue = 0;

	/**************************/
	/* Test the module number */
	/**************************/

	if (b_ModulNbr < 4) {
	   /*******************************/
		/* Test if counter initialised */
	   /*******************************/

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {

			outl(ui_CompareValue, devpriv->s_BoardInfos.
				ui_Address + 28 + (64 * b_ModulNbr));

			devpriv->
				s_ModuleInfo[b_ModulNbr].
				s_SiemensCounterInfo.
				s_InitFlag.b_CompareLogicInit = 1;
		} else {
	      /****************************************/
			/* Counter not initialised see function */
			/* "i_APCI1710_InitCounter"             */
	      /****************************************/

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   /*************************************************/
		/* The selected module number parameter is wrong */
	   /*************************************************/

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return (i_ReturnValue);
}

/*
+----------------------------------------------------------------------------+
| Function Name     : _INT_ i_APCI1710_InitFrequencyMeasurement              |
|				(BYTE_		 b_BoardHandle,              |
|				 BYTE_		 b_ModulNbr,                 |
|				 BYTE_		 b_PCIInputClock,            |
|				 BYTE_		 b_TimingUnity,              |
|				 ULONG_ 	ul_TimingInterval,           |
|				 PULONG_       pul_RealTimingInterval)       |
+----------------------------------------------------------------------------+
| Task              : Sets the time for the frequency measurement.           |
|		      Configures the selected TOR incremental counter of the |
|		      selected module (b_ModulNbr). The ul_TimingInterval and|
|		      ul_TimingUnity determine the time base for the         |
|		      measurement. The pul_RealTimingInterval returns the    |
|		      real time value. You must call up this function before |
|		      you call up any other function which gives access to   |
|		      the frequency measurement.                             |
+----------------------------------------------------------------------------+
| Input Parameters  : BYTE_  b_BoardHandle    : Handle of board APCI-1710    |
|		      BYTE_  b_ModulNbr	      :	Number of the module to be   |
|						configured (0 to 3)          |
|		      BYTE_  b_PCIInputClock  :	Selection of the PCI bus     |
|						clock                        |
|						- APCI1710_30MHZ :           |
|						  The PC has a PCI bus clock |
|						  of 30 MHz                  |
|						- APCI1710_33MHZ :           |
|						  The PC has a PCI bus clock |
|						  of 33 MHz                  |
|		      BYTE_  b_TimingUnity    : Base time unit (0 to 2)      |
|						  0 : ns                     |
|						  1 : æs                     |
|						  2 : ms                     |
|		      ULONG_ ul_TimingInterval: Base time value.             |
+----------------------------------------------------------------------------+
| Output Parameters : PULONG_ pul_RealTimingInterval : Real base time value. |
+----------------------------------------------------------------------------+
| Return Value      :  0: No error                                           |
|                     -1: The handle parameter of the board is wrong         |
|                     -2: The selected module number is wrong                |
|                     -3: Counter not initialised see function               |
|			  "i_APCI1710_InitCounter"                           |
|                     -4: The selected PCI input clock is wrong              |
|                     -5: Timing unity selection is wrong                    |
|                     -6: Base timing selection is wrong                     |
|		      -7: 40MHz quartz not on board                          |
+----------------------------------------------------------------------------+
*/

INT i_APCI1710_InitFrequencyMeasurement(struct comedi_device * dev,
	BYTE b_ModulNbr,
	BYTE b_PCIInputClock,
	BYTE b_TimingUnity,
	ULONG ul_TimingInterval, PULONG pul_RealTimingInterval)
{
	INT i_ReturnValue = 0;
	ULONG ul_TimerValue = 0;
	double d_RealTimingInterval;
	DWORD dw_Status = 0;

	/**************************/
	/* Test the module number */
	/**************************/

	if (b_ModulNbr < 4) {
	   /*******************************/
		/* Test if counter initialised */
	   /*******************************/

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {
	      /**************************/
			/* Test the PCI bus clock */
	      /**************************/

			if ((b_PCIInputClock == APCI1710_30MHZ) ||
				(b_PCIInputClock == APCI1710_33MHZ) ||
				(b_PCIInputClock == APCI1710_40MHZ)) {
		 /************************/
				/* Test the timing unit */
		 /************************/

				if (b_TimingUnity <= 2) {
		    /**********************************/
					/* Test the base timing selection */
		    /**********************************/

					if (((b_PCIInputClock == APCI1710_30MHZ)
							&& (b_TimingUnity == 0)
							&& (ul_TimingInterval >=
								266)
							&& (ul_TimingInterval <=
								8738133UL))
						|| ((b_PCIInputClock ==
								APCI1710_30MHZ)
							&& (b_TimingUnity == 1)
							&& (ul_TimingInterval >=
								1)
							&& (ul_TimingInterval <=
								8738UL))
						|| ((b_PCIInputClock ==
								APCI1710_30MHZ)
							&& (b_TimingUnity == 2)
							&& (ul_TimingInterval >=
								1)
							&& (ul_TimingInterval <=
								8UL))
						|| ((b_PCIInputClock ==
								APCI1710_33MHZ)
							&& (b_TimingUnity == 0)
							&& (ul_TimingInterval >=
								242)
							&& (ul_TimingInterval <=
								7943757UL))
						|| ((b_PCIInputClock ==
								APCI1710_33MHZ)
							&& (b_TimingUnity == 1)
							&& (ul_TimingInterval >=
								1)
							&& (ul_TimingInterval <=
								7943UL))
						|| ((b_PCIInputClock ==
								APCI1710_33MHZ)
							&& (b_TimingUnity == 2)
							&& (ul_TimingInterval >=
								1)
							&& (ul_TimingInterval <=
								7UL))
						|| ((b_PCIInputClock ==
								APCI1710_40MHZ)
							&& (b_TimingUnity == 0)
							&& (ul_TimingInterval >=
								200)
							&& (ul_TimingInterval <=
								6553500UL))
						|| ((b_PCIInputClock ==
								APCI1710_40MHZ)
							&& (b_TimingUnity == 1)
							&& (ul_TimingInterval >=
								1)
							&& (ul_TimingInterval <=
								6553UL))
						|| ((b_PCIInputClock ==
								APCI1710_40MHZ)
							&& (b_TimingUnity == 2)
							&& (ul_TimingInterval >=
								1)
							&& (ul_TimingInterval <=
								6UL))) {
		       /**********************/
						/* Test if 40MHz used */
		       /**********************/

						if (b_PCIInputClock ==
							APCI1710_40MHZ) {
			  /******************************/
							/* Test if firmware >= Rev1.5 */
			  /******************************/

							if ((devpriv->s_BoardInfos.dw_MolduleConfiguration[b_ModulNbr] & 0xFFFF) >= 0x3135) {
			     /*********************************/
								/* Test if 40MHz quartz on board */
			     /*********************************/

								/*INPDW (ps_APCI1710Variable->
								   s_Board [b_BoardHandle].
								   s_BoardInfos.
								   ui_Address + 36 + (64 * b_ModulNbr), &dw_Status); */
								dw_Status =
									inl
									(devpriv->
									s_BoardInfos.
									ui_Address
									+ 36 +
									(64 * b_ModulNbr));

			     /******************************/
								/* Test the quartz flag (DQ0) */
			     /******************************/

								if ((dw_Status & 1) != 1) {
				/*****************************/
									/* 40MHz quartz not on board */
				/*****************************/

									DPRINTK("40MHz quartz not on board\n");
									i_ReturnValue
										=
										-7;
								}
							} else {
			     /*****************************/
								/* 40MHz quartz not on board */
			     /*****************************/
								DPRINTK("40MHz quartz not on board\n");
								i_ReturnValue =
									-7;
							}
						}	// if (b_PCIInputClock == APCI1710_40MHZ)

		       /***************************/
						/* Test if not error occur */
		       /***************************/

						if (i_ReturnValue == 0) {
			  /****************************/
							/* Test the INC_CPT version */
			  /****************************/

							if ((devpriv->s_BoardInfos.dw_MolduleConfiguration[b_ModulNbr] & 0xFFFF) >= 0x3131) {

				/**********************/
								/* Test if 40MHz used */
				/**********************/

								if (b_PCIInputClock == APCI1710_40MHZ) {
				   /*********************************/
									/* Enable the 40MHz quarz (DQ30) */
				   /*********************************/

									devpriv->
										s_ModuleInfo
										[b_ModulNbr].
										s_SiemensCounterInfo.
										s_ModeRegister.
										s_ByteModeRegister.
										b_ModeRegister4
										=
										devpriv->
										s_ModuleInfo
										[b_ModulNbr].
										s_SiemensCounterInfo.
										s_ModeRegister.
										s_ByteModeRegister.
										b_ModeRegister4
										|
										APCI1710_ENABLE_40MHZ_FREQUENCY;
								}	// if (b_PCIInputClock == APCI1710_40MHZ)
								else {
				   /**********************************/
									/* Disable the 40MHz quarz (DQ30) */
				   /**********************************/

									devpriv->
										s_ModuleInfo
										[b_ModulNbr].
										s_SiemensCounterInfo.
										s_ModeRegister.
										s_ByteModeRegister.
										b_ModeRegister4
										=
										devpriv->
										s_ModuleInfo
										[b_ModulNbr].
										s_SiemensCounterInfo.
										s_ModeRegister.
										s_ByteModeRegister.
										b_ModeRegister4
										&
										APCI1710_DISABLE_40MHZ_FREQUENCY;

								}	// if (b_PCIInputClock == APCI1710_40MHZ)

			     /********************************/
								/* Calculate the division fator */
			     /********************************/

								fpu_begin();
								switch (b_TimingUnity) {
				/******/
									/* ns */
				/******/

								case 0:

					/******************/
									/* Timer 0 factor */
					/******************/

									ul_TimerValue
										=
										(ULONG)
										(ul_TimingInterval
										*
										(0.00025 * b_PCIInputClock));

					/*******************/
									/* Round the value */
					/*******************/

									if ((double)((double)ul_TimingInterval * (0.00025 * (double)b_PCIInputClock)) >= ((double)((double)ul_TimerValue + 0.5))) {
										ul_TimerValue
											=
											ul_TimerValue
											+
											1;
									}

					/*****************************/
									/* Calculate the real timing */
					/*****************************/

									*pul_RealTimingInterval
										=
										(ULONG)
										(ul_TimerValue
										/
										(0.00025 * (double)b_PCIInputClock));
									d_RealTimingInterval
										=
										(double)
										ul_TimerValue
										/
										(0.00025
										*
										(double)
										b_PCIInputClock);

									if ((double)((double)ul_TimerValue / (0.00025 * (double)b_PCIInputClock)) >= (double)((double)*pul_RealTimingInterval + 0.5)) {
										*pul_RealTimingInterval
											=
											*pul_RealTimingInterval
											+
											1;
									}

									ul_TimingInterval
										=
										ul_TimingInterval
										-
										1;
									ul_TimerValue
										=
										ul_TimerValue
										-
										2;

									break;

				/******/
									/* æs */
				/******/

								case 1:

					/******************/
									/* Timer 0 factor */
					/******************/

									ul_TimerValue
										=
										(ULONG)
										(ul_TimingInterval
										*
										(0.25 * b_PCIInputClock));

					/*******************/
									/* Round the value */
					/*******************/

									if ((double)((double)ul_TimingInterval * (0.25 * (double)b_PCIInputClock)) >= ((double)((double)ul_TimerValue + 0.5))) {
										ul_TimerValue
											=
											ul_TimerValue
											+
											1;
									}

					/*****************************/
									/* Calculate the real timing */
					/*****************************/

									*pul_RealTimingInterval
										=
										(ULONG)
										(ul_TimerValue
										/
										(0.25 * (double)b_PCIInputClock));
									d_RealTimingInterval
										=
										(double)
										ul_TimerValue
										/
										(
										(double)
										0.25
										*
										(double)
										b_PCIInputClock);

									if ((double)((double)ul_TimerValue / (0.25 * (double)b_PCIInputClock)) >= (double)((double)*pul_RealTimingInterval + 0.5)) {
										*pul_RealTimingInterval
											=
											*pul_RealTimingInterval
											+
											1;
									}

									ul_TimingInterval
										=
										ul_TimingInterval
										-
										1;
									ul_TimerValue
										=
										ul_TimerValue
										-
										2;

									break;

				/******/
									/* ms */
				/******/

								case 2:

					/******************/
									/* Timer 0 factor */
					/******************/

									ul_TimerValue
										=
										ul_TimingInterval
										*
										(250.0
										*
										b_PCIInputClock);

					/*******************/
									/* Round the value */
					/*******************/

									if ((double)((double)ul_TimingInterval * (250.0 * (double)b_PCIInputClock)) >= ((double)((double)ul_TimerValue + 0.5))) {
										ul_TimerValue
											=
											ul_TimerValue
											+
											1;
									}

					/*****************************/
									/* Calculate the real timing */
					/*****************************/

									*pul_RealTimingInterval
										=
										(ULONG)
										(ul_TimerValue
										/
										(250.0 * (double)b_PCIInputClock));
									d_RealTimingInterval
										=
										(double)
										ul_TimerValue
										/
										(250.0
										*
										(double)
										b_PCIInputClock);

									if ((double)((double)ul_TimerValue / (250.0 * (double)b_PCIInputClock)) >= (double)((double)*pul_RealTimingInterval + 0.5)) {
										*pul_RealTimingInterval
											=
											*pul_RealTimingInterval
											+
											1;
									}

									ul_TimingInterval
										=
										ul_TimingInterval
										-
										1;
									ul_TimerValue
										=
										ul_TimerValue
										-
										2;

									break;
								}

								fpu_end();
			     /*************************/
								/* Write the timer value */
			     /*************************/

								outl(ul_TimerValue, devpriv->s_BoardInfos.ui_Address + 32 + (64 * b_ModulNbr));

			     /*******************************/
								/* Set the initialisation flag */
			     /*******************************/

								devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_InitFlag.
									b_FrequencyMeasurementInit
									= 1;
							} else {
			     /***************************/
								/* Counter not initialised */
			     /***************************/

								DPRINTK("Counter not initialised\n");
								i_ReturnValue =
									-3;
							}
						}	// if (i_ReturnValue == 0)
					} else {
		       /**********************************/
						/* Base timing selection is wrong */
		       /**********************************/

						DPRINTK("Base timing selection is wrong\n");
						i_ReturnValue = -6;
					}
				} else {
		    /***********************************/
					/* Timing unity selection is wrong */
		    /***********************************/

					DPRINTK("Timing unity selection is wrong\n");
					i_ReturnValue = -5;
				}
			} else {
		 /*****************************************/
				/* The selected PCI input clock is wrong */
		 /*****************************************/

				DPRINTK("The selected PCI input clock is wrong\n");
				i_ReturnValue = -4;
			}
		} else {
	      /****************************************/
			/* Counter not initialised see function */
			/* "i_APCI1710_InitCounter"             */
	      /****************************************/

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   /*************************************************/
		/* The selected module number parameter is wrong */
	   /*************************************************/

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return (i_ReturnValue);
}

/*########################################################################### */

							//INSN BITS
/*########################################################################### */

/*
+----------------------------------------------------------------------------+
| Function Name     :INT	i_APCI1710_InsnBitsINCCPT(struct comedi_device *dev,struct comedi_subdevice *s,
struct comedi_insn *insn,unsigned int *data)                   |
+----------------------------------------------------------------------------+
| Task              : Set & Clear Functions for INC_CPT                                          |
+----------------------------------------------------------------------------+
| Input Parameters  :
+----------------------------------------------------------------------------+
| Output Parameters : -                                                      |
+----------------------------------------------------------------------------+
| Return Value      :
+----------------------------------------------------------------------------+
*/

INT i_APCI1710_InsnBitsINCCPT(struct comedi_device * dev, struct comedi_subdevice * s,
	struct comedi_insn * insn, unsigned int * data)
{
	UINT ui_BitsType;
	INT i_ReturnValue = 0;
	ui_BitsType = CR_CHAN(insn->chanspec);
	devpriv->tsk_Current = current;	// Save the current process task structure

	switch (ui_BitsType) {
	case APCI1710_INCCPT_CLEARCOUNTERVALUE:
		i_ReturnValue = i_APCI1710_ClearCounterValue(dev,
			(BYTE) CR_AREF(insn->chanspec));
		break;

	case APCI1710_INCCPT_CLEARALLCOUNTERVALUE:
		i_ReturnValue = i_APCI1710_ClearAllCounterValue(dev);
		break;

	case APCI1710_INCCPT_SETINPUTFILTER:
		i_ReturnValue = i_APCI1710_SetInputFilter(dev,
			(BYTE) CR_AREF(insn->chanspec),
			(BYTE) data[0], (BYTE) data[1]);
		break;

	case APCI1710_INCCPT_LATCHCOUNTER:
		i_ReturnValue = i_APCI1710_LatchCounter(dev,
			(BYTE) CR_AREF(insn->chanspec), (BYTE) data[0]);
		break;

	case APCI1710_INCCPT_SETINDEXANDREFERENCESOURCE:
		i_ReturnValue = i_APCI1710_SetIndexAndReferenceSource(dev,
			(BYTE) CR_AREF(insn->chanspec), (BYTE) data[0]);
		break;

	case APCI1710_INCCPT_SETDIGITALCHLON:
		i_ReturnValue = i_APCI1710_SetDigitalChlOn(dev,
			(BYTE) CR_AREF(insn->chanspec));
		break;

	case APCI1710_INCCPT_SETDIGITALCHLOFF:
		i_ReturnValue = i_APCI1710_SetDigitalChlOff(dev,
			(BYTE) CR_AREF(insn->chanspec));
		break;

	default:
		printk("Bits Config Parameter Wrong\n");
	}

	if (i_ReturnValue >= 0)
		i_ReturnValue = insn->n;
	return (i_ReturnValue);
}

/*
+----------------------------------------------------------------------------+
| Function Name     : _INT_ i_APCI1710_ClearCounterValue                     |
|                               (BYTE_      b_BoardHandle,                   |
|                                BYTE_       b_ModulNbr)                     |
+----------------------------------------------------------------------------+
| Task              : Clear the counter value from selected module           |
|                     (b_ModulNbr).                                          |
+----------------------------------------------------------------------------+
| Input Parameters  : BYTE_ b_BoardHandle : Handle of board APCI-1710        |
|                     BYTE_ b_ModulNbr    : Module number to configure       |
|                                           (0 to 3)                         |
+----------------------------------------------------------------------------+
| Output Parameters : -                                                      |
+----------------------------------------------------------------------------+
| Return Value      :  0: No error                                           |
|                     -1: The handle parameter of the board is wrong         |
|                     -2: The selected module number parameter is wrong      |
|                     -3: Counter not initialised see function               |
|                         "i_APCI1710_InitCounter"                           |
+----------------------------------------------------------------------------+
*/

INT i_APCI1710_ClearCounterValue(struct comedi_device * dev, BYTE b_ModulNbr)
{
	INT i_ReturnValue = 0;

	/**************************/
	/* Test the module number */
	/**************************/

	if (b_ModulNbr < 4) {
	   /*******************************/
		/* Test if counter initialised */
	   /*******************************/

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {
	      /*********************/
			/* Clear the counter */
	      /*********************/

			outl(1, devpriv->s_BoardInfos.
				ui_Address + 16 + (64 * b_ModulNbr));
		} else {
	      /****************************************/
			/* Counter not initialised see function */
			/* "i_APCI1710_InitCounter"             */
	      /****************************************/

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   /*************************************************/
		/* The selected module number parameter is wrong */
	   /*************************************************/

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return (i_ReturnValue);
}

/*
+----------------------------------------------------------------------------+
| Function Name     : _INT_ i_APCI1710_ClearAllCounterValue                  |
|                               (BYTE_      b_BoardHandle)                   |
+----------------------------------------------------------------------------+
| Task              : Clear all counter value.                               |
+----------------------------------------------------------------------------+
| Input Parameters  : BYTE_ b_BoardHandle : Handle of board APCI-1710        |
+----------------------------------------------------------------------------+
| Output Parameters : -                                                      |
+----------------------------------------------------------------------------+
| Return Value      :  0: No error                                           |
|                     -1: The handle parameter of the board is wrong         |
|                     -2: No counter module found                            |
+----------------------------------------------------------------------------+
*/

INT i_APCI1710_ClearAllCounterValue(struct comedi_device * dev)
{
	BYTE b_ModulCpt = 0;
	INT i_ReturnValue = 0;

	/********************************/
	/* Test if counter module found */
	/********************************/

	if ((devpriv->s_BoardInfos.
			dw_MolduleConfiguration[0] & 0xFFFF0000UL) ==
		APCI1710_INCREMENTAL_COUNTER
		|| (devpriv->s_BoardInfos.
			dw_MolduleConfiguration[1] & 0xFFFF0000UL) ==
		APCI1710_INCREMENTAL_COUNTER
		|| (devpriv->s_BoardInfos.
			dw_MolduleConfiguration[2] & 0xFFFF0000UL) ==
		APCI1710_INCREMENTAL_COUNTER
		|| (devpriv->s_BoardInfos.
			dw_MolduleConfiguration[3] & 0xFFFF0000UL) ==
		APCI1710_INCREMENTAL_COUNTER) {
		for (b_ModulCpt = 0; b_ModulCpt < 4; b_ModulCpt++) {
	      /*******************************/
			/* Test if incremental counter */
	      /*******************************/

			if ((devpriv->s_BoardInfos.
					dw_MolduleConfiguration[b_ModulCpt] &
					0xFFFF0000UL) ==
				APCI1710_INCREMENTAL_COUNTER) {
		 /*********************/
				/* Clear the counter */
		 /*********************/

				outl(1, devpriv->s_BoardInfos.
					ui_Address + 16 + (64 * b_ModulCpt));
			}
		}
	} else {
	   /***************************/
		/* No counter module found */
	   /***************************/

		DPRINTK("No counter module found\n");
		i_ReturnValue = -2;
	}

	return (i_ReturnValue);
}

/*
+----------------------------------------------------------------------------+
| Function Name     : _INT_ i_APCI1710_SetInputFilter                        |
|					(BYTE_ b_BoardHandle,                |
|					 BYTE_ b_Module,                     |
|					 BYTE_ b_PCIInputClock,              |
|					 BYTE_ b_Filter)     		     |
+----------------------------------------------------------------------------+
| Task              : Disable or enable the software filter from selected    |
|		      module (b_ModulNbr). b_Filter determine the filter time|
+----------------------------------------------------------------------------+
| Input Parameters  : BYTE_  b_BoardHandle    : Handle of board APCI-1710    |
|		      BYTE_  b_ModulNbr	      :	Number of the module to be   |
|						configured (0 to 3)          |
|		      BYTE_  b_PCIInputClock  :	Selection of the PCI bus     |
|						clock                        |
|						- APCI1710_30MHZ :           |
|						  The PC has a PCI bus clock |
|						  of 30 MHz                  |
|						- APCI1710_33MHZ :           |
|						  The PC has a PCI bus clock |
|						  of 33 MHz                  |
|						- APCI1710_40MHZ :           |
|						  The APCI1710 has a 40MHz    |
|						  quartz		     |
|		      BYTE_  b_Filter	      : Filter selection             |
|                                                                            |
|				30 MHz                                       |
|				------                                       |
|					0:  Software filter not used         |
|					1:  Filter from 266ns  (3.750000MHz) |
|					2:  Filter from 400ns  (2.500000MHz) |
|					3:  Filter from 533ns  (1.876170MHz) |
|					4:  Filter from 666ns  (1.501501MHz) |
|					5:  Filter from 800ns  (1.250000MHz) |
|					6:  Filter from 933ns  (1.071800MHz) |
|					7:  Filter from 1066ns (0.938080MHz) |
|					8:  Filter from 1200ns (0.833333MHz) |
|					9:  Filter from 1333ns (0.750000MHz) |
|					10: Filter from 1466ns (0.682100MHz) |
|					11: Filter from 1600ns (0.625000MHz) |
|					12: Filter from 1733ns (0.577777MHz) |
|					13: Filter from 1866ns (0.535900MHz) |
|					14: Filter from 2000ns (0.500000MHz) |
|					15: Filter from 2133ns (0.468800MHz) |
|									     |
|				33 MHz                                       |
|				------                                       |
|					0:  Software filter not used         |
|					1:  Filter from 242ns  (4.125000MHz) |
|					2:  Filter from 363ns  (2.754820MHz) |
|					3:  Filter from 484ns  (2.066115MHz) |
|					4:  Filter from 605ns  (1.652892MHz) |
|					5:  Filter from 726ns  (1.357741MHz) |
|					6:  Filter from 847ns  (1.180637MHz) |
|					7:  Filter from 968ns  (1.033055MHz) |
|					8:  Filter from 1089ns (0.918273MHz) |
|					9:  Filter from 1210ns (0.826446MHz) |
|					10: Filter from 1331ns (0.751314MHz) |
|					11: Filter from 1452ns (0.688705MHz) |
|					12: Filter from 1573ns (0.635727MHz) |
|					13: Filter from 1694ns (0.590318MHz) |
|					14: Filter from 1815ns (0.550964MHz) |
|					15: Filter from 1936ns (0.516528MHz) |
|									     |
|				40 MHz                                       |
|				------                                       |
|					0:  Software filter not used         |
|					1:  Filter from 200ns  (5.000000MHz) |
|					2:  Filter from 300ns  (3.333333MHz) |
|					3:  Filter from 400ns  (2.500000MHz) |
|					4:  Filter from 500ns  (2.000000MHz) |
|					5:  Filter from 600ns  (1.666666MHz) |
|					6:  Filter from 700ns  (1.428500MHz) |
|					7:  Filter from 800ns  (1.250000MHz) |
|					8:  Filter from 900ns  (1.111111MHz) |
|					9:  Filter from 1000ns (1.000000MHz) |
|					10: Filter from 1100ns (0.909090MHz) |
|					11: Filter from 1200ns (0.833333MHz) |
|					12: Filter from 1300ns (0.769200MHz) |
|					13: Filter from 1400ns (0.714200MHz) |
|					14: Filter from 1500ns (0.666666MHz) |
|					15: Filter from 1600ns (0.625000MHz) |
+----------------------------------------------------------------------------+
| Output Parameters : -                                                      |
+----------------------------------------------------------------------------+
| Return Value      :  0: No error                                           |
|                     -1: The handle parameter of the board is wrong         |
|                     -2: The selected module number is wrong                |
|                     -3: The module is not a counter module                 |
|					  -4: The selected PCI input clock is wrong              |
|					  -5: The selected filter value is wrong                 |
|					  -6: 40MHz quartz not on board                          |
+----------------------------------------------------------------------------+
*/

INT i_APCI1710_SetInputFilter(struct comedi_device * dev,
	BYTE b_ModulNbr, BYTE b_PCIInputClock, BYTE b_Filter)
{
	INT i_ReturnValue = 0;
	DWORD dw_Status = 0;

	/**************************/
	/* Test the module number */
	/**************************/

	if (b_ModulNbr < 4) {
	   /*******************************/
		/* Test if incremental counter */
	   /*******************************/

		if ((devpriv->s_BoardInfos.
				dw_MolduleConfiguration[b_ModulNbr] &
				0xFFFF0000UL) == APCI1710_INCREMENTAL_COUNTER) {
	      /******************************/
			/* Test if firmware >= Rev1.5 */
	      /******************************/

			if ((devpriv->s_BoardInfos.
					dw_MolduleConfiguration[b_ModulNbr] &
					0xFFFF) >= 0x3135) {
		 /**************************/
				/* Test the PCI bus clock */
		 /**************************/

				if ((b_PCIInputClock == APCI1710_30MHZ) ||
					(b_PCIInputClock == APCI1710_33MHZ) ||
					(b_PCIInputClock == APCI1710_40MHZ)) {
		    /*************************/
					/* Test the filter value */
		    /*************************/

					if (b_Filter < 16) {
		       /**********************/
						/* Test if 40MHz used */
		       /**********************/

						if (b_PCIInputClock ==
							APCI1710_40MHZ) {
			  /*********************************/
							/* Test if 40MHz quartz on board */
			  /*********************************/

							dw_Status =
								inl(devpriv->
								s_BoardInfos.
								ui_Address +
								36 +
								(64 * b_ModulNbr));

			  /******************************/
							/* Test the quartz flag (DQ0) */
			  /******************************/

							if ((dw_Status & 1) !=
								1) {
			     /*****************************/
								/* 40MHz quartz not on board */
			     /*****************************/

								DPRINTK("40MHz quartz not on board\n");
								i_ReturnValue =
									-6;
							}
						}	// if (b_PCIInputClock == APCI1710_40MHZ)

		       /***************************/
						/* Test if error not occur */
		       /***************************/

						if (i_ReturnValue == 0) {
			  /**********************/
							/* Test if 40MHz used */
			  /**********************/

							if (b_PCIInputClock ==
								APCI1710_40MHZ)
							{
			     /*********************************/
								/* Enable the 40MHz quarz (DQ31) */
			     /*********************************/

								devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister4
									=
									devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister4
									|
									APCI1710_ENABLE_40MHZ_FILTER;

							}	// if (b_PCIInputClock == APCI1710_40MHZ)
							else {
			     /**********************************/
								/* Disable the 40MHz quarz (DQ31) */
			     /**********************************/

								devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister4
									=
									devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister4
									&
									APCI1710_DISABLE_40MHZ_FILTER;

							}	// if (b_PCIInputClock == APCI1710_40MHZ)

			  /************************/
							/* Set the filter value */
			  /************************/

							devpriv->
								s_ModuleInfo
								[b_ModulNbr].
								s_SiemensCounterInfo.
								s_ModeRegister.
								s_ByteModeRegister.
								b_ModeRegister3
								=
								(devpriv->
								s_ModuleInfo
								[b_ModulNbr].
								s_SiemensCounterInfo.
								s_ModeRegister.
								s_ByteModeRegister.
								b_ModeRegister3
								& 0x1F) |
								((b_Filter &
									0x7) <<
								5);

							devpriv->
								s_ModuleInfo
								[b_ModulNbr].
								s_SiemensCounterInfo.
								s_ModeRegister.
								s_ByteModeRegister.
								b_ModeRegister4
								=
								(devpriv->
								s_ModuleInfo
								[b_ModulNbr].
								s_SiemensCounterInfo.
								s_ModeRegister.
								s_ByteModeRegister.
								b_ModeRegister4
								& 0xFE) |
								((b_Filter &
									0x8) >>
								3);

			  /***************************/
							/* Write the configuration */
			  /***************************/

							outl(devpriv->
								s_ModuleInfo
								[b_ModulNbr].
								s_SiemensCounterInfo.
								s_ModeRegister.
								dw_ModeRegister1_2_3_4,
								devpriv->
								s_BoardInfos.
								ui_Address +
								20 +
								(64 * b_ModulNbr));
						}	// if (i_ReturnValue == 0)
					}	// if (b_Filter < 16)
					else {
		       /**************************************/
						/* The selected filter value is wrong */
		       /**************************************/

						DPRINTK("The selected filter value is wrong\n");
						i_ReturnValue = -5;
					}	// if (b_Filter < 16)
				}	// if ((b_PCIInputClock == APCI1710_30MHZ) || (b_PCIInputClock == APCI1710_33MHZ) || (b_PCIInputClock == APCI1710_40MHZ))
				else {
		    /*****************************************/
					/* The selected PCI input clock is wrong */
		    /*****************************************/

					DPRINTK("The selected PCI input clock is wrong\n");
					i_ReturnValue = 4;
				}	// if ((b_PCIInputClock == APCI1710_30MHZ) || (b_PCIInputClock == APCI1710_33MHZ) || (b_PCIInputClock == APCI1710_40MHZ))
			} else {
		 /**************************************/
				/* The module is not a counter module */
		 /**************************************/

				DPRINTK("The module is not a counter module\n");
				i_ReturnValue = -3;
			}
		} else {
	      /**************************************/
			/* The module is not a counter module */
	      /**************************************/

			DPRINTK("The module is not a counter module\n");
			i_ReturnValue = -3;
		}
	} else {
	   /*************************************************/
		/* The selected module number parameter is wrong */
	   /*************************************************/

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return (i_ReturnValue);
}

/*
+----------------------------------------------------------------------------+
| Function Name     : _INT_ i_APCI1710_LatchCounter (BYTE_ b_BoardHandle,    |
|                                                    BYTE_ b_ModulNbr,       |
|                                                    BYTE_ b_LatchReg)       |
+----------------------------------------------------------------------------+
| Task              : Latch the courant value from selected module           |
|                     (b_ModulNbr) in to the selected latch register         |
|                     (b_LatchReg).                                          |
+----------------------------------------------------------------------------+
| Input Parameters  : BYTE_ b_BoardHandle : Handle of board APCI-1710        |
|                     BYTE_ b_ModulNbr    : Module number to configure       |
|                                           (0 to 3)                         |
|                     BYTE_ b_LatchReg    : Selected latch register          |
|                               0 : for the first latch register             |
|                               1 : for the second latch register            |
+----------------------------------------------------------------------------+
| Output Parameters : -                                                      |
+----------------------------------------------------------------------------+
| Return Value      :  0: No error                                           |
|                     -1: The handle parameter of the board is wrong         |
|                     -2: No counter module found                            |
|                     -3: Counter not initialised see function               |
|                         "i_APCI1710_InitCounter"                           |
|                     -4: The selected latch register parameter is wrong     |
+----------------------------------------------------------------------------+
*/

INT i_APCI1710_LatchCounter(struct comedi_device * dev,
	BYTE b_ModulNbr, BYTE b_LatchReg)
{
	INT i_ReturnValue = 0;

	/**************************/
	/* Test the module number */
	/**************************/

	if (b_ModulNbr < 4) {
	   /*******************************/
		/* Test if counter initialised */
	   /*******************************/

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {
	      /*************************************/
			/* Test the latch register parameter */
	      /*************************************/

			if (b_LatchReg < 2) {
		 /*********************/
				/* Tatch the counter */
		 /*********************/

				outl(1 << (b_LatchReg * 4),
					devpriv->s_BoardInfos.ui_Address +
					(64 * b_ModulNbr));
			} else {
		 /**************************************************/
				/* The selected latch register parameter is wrong */
		 /**************************************************/

				DPRINTK("The selected latch register parameter is wrong\n");
				i_ReturnValue = -4;
			}
		} else {
	      /****************************************/
			/* Counter not initialised see function */
			/* "i_APCI1710_InitCounter"             */
	      /****************************************/

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   /*************************************************/
		/* The selected module number parameter is wrong */
	   /*************************************************/

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return (i_ReturnValue);
}

/*
+----------------------------------------------------------------------------+
| Function Name     : _INT_	i_APCI1710_SetIndexAndReferenceSource        |
|					(BYTE_ b_BoardHandle,                |
|					 BYTE_ b_ModulNbr,                   |
|					 BYTE_ b_SourceSelection)            |
+----------------------------------------------------------------------------+
| Task              : Determine the hardware source for the index and the    |
|		      reference logic. Per default the index logic is        |
|		      connected to the difference input C and the reference  |
|		      logic is connected to the 24V input E                  |
+----------------------------------------------------------------------------+
| Input Parameters  : BYTE_ b_BoardHandle     : Handle of board APCI-1710    |
|                     BYTE_ b_ModulNbr        : Module number to configure   |
|                                               (0 to 3)                     |
|		      BYTE_ b_SourceSelection : APCI1710_SOURCE_0 :          |
|						The index logic is connected |
|						to the difference input C and|
|						the reference logic is       |
|						connected to the 24V input E.|
|						This is the default          |
|						configuration.               |
|						APCI1710_SOURCE_1 :          |
|						The reference logic is       |
|						connected to the difference  |
|						input C and the index logic  |
|						is connected to the 24V      |
|						input E                      |
+----------------------------------------------------------------------------+
| Output Parameters : -                                                      |
+----------------------------------------------------------------------------+
| Return Value      :  0: No error                                           |
|                     -1: The handle parameter of the board is wrong         |
|		      -2: The selected module number is wrong                |
|		      -3: The module is not a counter module.                |
|		      -4: The source selection is wrong                      |
+----------------------------------------------------------------------------+
*/

INT i_APCI1710_SetIndexAndReferenceSource(struct comedi_device * dev,
	BYTE b_ModulNbr, BYTE b_SourceSelection)
{
	INT i_ReturnValue = 0;

	/**************************/
	/* Test the module number */
	/**************************/

	if (b_ModulNbr < 4) {
	   /*******************************/
		/* Test if incremental counter */
	   /*******************************/

		if ((devpriv->s_BoardInfos.
				dw_MolduleConfiguration[b_ModulNbr] &
				0xFFFF0000UL) == APCI1710_INCREMENTAL_COUNTER) {
	      /******************************/
			/* Test if firmware >= Rev1.5 */
	      /******************************/

			if ((devpriv->s_BoardInfos.
					dw_MolduleConfiguration[b_ModulNbr] &
					0xFFFF) >= 0x3135) {
		 /*****************************/
				/* Test the source selection */
		 /*****************************/

				if (b_SourceSelection == APCI1710_SOURCE_0 ||
					b_SourceSelection == APCI1710_SOURCE_1)
				{
		    /******************************************/
					/* Test if invert the index and reference */
		    /******************************************/

					if (b_SourceSelection ==
						APCI1710_SOURCE_1) {
		       /********************************************/
						/* Invert index and reference source (DQ25) */
		       /********************************************/

						devpriv->
							s_ModuleInfo
							[b_ModulNbr].
							s_SiemensCounterInfo.
							s_ModeRegister.
							s_ByteModeRegister.
							b_ModeRegister4 =
							devpriv->
							s_ModuleInfo
							[b_ModulNbr].
							s_SiemensCounterInfo.
							s_ModeRegister.
							s_ByteModeRegister.
							b_ModeRegister4 |
							APCI1710_INVERT_INDEX_RFERENCE;
					} else {
		       /****************************************/
						/* Set the default configuration (DQ25) */
		       /****************************************/

						devpriv->
							s_ModuleInfo
							[b_ModulNbr].
							s_SiemensCounterInfo.
							s_ModeRegister.
							s_ByteModeRegister.
							b_ModeRegister4 =
							devpriv->
							s_ModuleInfo
							[b_ModulNbr].
							s_SiemensCounterInfo.
							s_ModeRegister.
							s_ByteModeRegister.
							b_ModeRegister4 &
							APCI1710_DEFAULT_INDEX_RFERENCE;
					}
				}	// if (b_SourceSelection == APCI1710_SOURCE_0 ||b_SourceSelection == APCI1710_SOURCE_1)
				else {
		    /*********************************/
					/* The source selection is wrong */
		    /*********************************/

					DPRINTK("The source selection is wrong\n");
					i_ReturnValue = -4;
				}	// if (b_SourceSelection == APCI1710_SOURCE_0 ||b_SourceSelection == APCI1710_SOURCE_1)
			} else {
		 /**************************************/
				/* The module is not a counter module */
		 /**************************************/

				DPRINTK("The module is not a counter module\n");
				i_ReturnValue = -3;
			}
		} else {
	      /**************************************/
			/* The module is not a counter module */
	      /**************************************/

			DPRINTK("The module is not a counter module\n");
			i_ReturnValue = -3;
		}
	} else {
	   /***************************************/
		/* The selected module number is wrong */
	   /***************************************/

		DPRINTK("The selected module number is wrong\n");
		i_ReturnValue = -2;
	}

	return (i_ReturnValue);
}

/*
+----------------------------------------------------------------------------+
| Function Name     : _INT_	i_APCI1710_SetDigitalChlOn                   |
|				   (BYTE_  b_BoardHandle,                    |
|				    BYTE_  b_ModulNbr)                       |
+----------------------------------------------------------------------------+
| Task              : Sets the digital output H Setting an output means      |
|		      setting an ouput high.                                 |
+----------------------------------------------------------------------------+
| Input Parameters  : BYTE_  b_BoardHandle    : Handle of board APCI-1710    |
|		      BYTE_  b_ModulNbr	      :	Number of the module to be   |
|						configured (0 to 3)          |
+----------------------------------------------------------------------------+
| Output Parameters : -                                                      |
+----------------------------------------------------------------------------+
| Return Value      :  0: No error                                           |
|                     -1: The handle parameter of the board is wrong         |
|                     -2: The selected module number is wrong                |
|                     -3: Counter not initialised see function               |
|			  "i_APCI1710_InitCounter"                           |
+----------------------------------------------------------------------------+
*/

INT i_APCI1710_SetDigitalChlOn(struct comedi_device * dev, BYTE b_ModulNbr)
{
	INT i_ReturnValue = 0;

	/**************************/
	/* Test the module number */
	/**************************/

	if (b_ModulNbr < 4) {
	   /*******************************/
		/* Test if counter initialised */
	   /*******************************/

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {
			devpriv->
				s_ModuleInfo[b_ModulNbr].
				s_SiemensCounterInfo.
				s_ModeRegister.
				s_ByteModeRegister.
				b_ModeRegister3 = devpriv->
				s_ModuleInfo[b_ModulNbr].
				s_SiemensCounterInfo.
				s_ModeRegister.
				s_ByteModeRegister.b_ModeRegister3 | 0x10;

	      /*********************/
			/* Set the output On */
	      /*********************/

			outl(devpriv->s_ModuleInfo[b_ModulNbr].
				s_SiemensCounterInfo.
				s_ModeRegister.
				dw_ModeRegister1_2_3_4, devpriv->s_BoardInfos.
				ui_Address + 20 + (64 * b_ModulNbr));
		} else {
	      /****************************************/
			/* Counter not initialised see function */
			/* "i_APCI1710_InitCounter"             */
	      /****************************************/

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   /*************************************************/
		/* The selected module number parameter is wrong */
	   /*************************************************/

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return (i_ReturnValue);
}

/*
+----------------------------------------------------------------------------+
| Function Name     : _INT_	i_APCI1710_SetDigitalChlOff                  |
|				   (BYTE_  b_BoardHandle,                    |
|				    BYTE_  b_ModulNbr)                       |
+----------------------------------------------------------------------------+
| Task              : Resets the digital output H. Resetting an output means |
|		      setting an ouput low.                                  |
+----------------------------------------------------------------------------+
| Input Parameters  : BYTE_  b_BoardHandle    : Handle of board APCI-1710    |
|		      BYTE_  b_ModulNbr	      :	Number of the module to be   |
|						configured (0 to 3)          |
+----------------------------------------------------------------------------+
| Output Parameters : -                                                      |
+----------------------------------------------------------------------------+
| Return Value      :  0: No error                                           |
|                     -1: The handle parameter of the board is wrong         |
|                     -2: The selected module number is wrong                |
|                     -3: Counter not initialised see function               |
|			  "i_APCI1710_InitCounter"                           |
+----------------------------------------------------------------------------+
*/

INT i_APCI1710_SetDigitalChlOff(struct comedi_device * dev, BYTE b_ModulNbr)
{
	INT i_ReturnValue = 0;

	/**************************/
	/* Test the module number */
	/**************************/

	if (b_ModulNbr < 4) {
	   /*******************************/
		/* Test if counter initialised */
	   /*******************************/

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {
			devpriv->
				s_ModuleInfo[b_ModulNbr].
				s_SiemensCounterInfo.
				s_ModeRegister.
				s_ByteModeRegister.
				b_ModeRegister3 = devpriv->
				s_ModuleInfo[b_ModulNbr].
				s_SiemensCounterInfo.
				s_ModeRegister.
				s_ByteModeRegister.b_ModeRegister3 & 0xEF;

	      /**********************/
			/* Set the output Off */
	      /**********************/

			outl(devpriv->s_ModuleInfo[b_ModulNbr].
				s_SiemensCounterInfo.
				s_ModeRegister.
				dw_ModeRegister1_2_3_4, devpriv->s_BoardInfos.
				ui_Address + 20 + (64 * b_ModulNbr));
		} else {
	      /****************************************/
			/* Counter not initialised see function */
			/* "i_APCI1710_InitCounter"             */
	      /****************************************/

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   /*************************************************/
		/* The selected module number parameter is wrong */
	   /*************************************************/

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return (i_ReturnValue);
}

/*########################################################################### */

							// INSN WRITE
/*########################################################################### */

/*
+----------------------------------------------------------------------------+
| Function Name     :INT	i_APCI1710_InsnWriteINCCPT(struct comedi_device *dev,struct comedi_subdevice *s,
struct comedi_insn *insn,unsigned int *data)                   |
+----------------------------------------------------------------------------+
| Task              : Enable Disable functions for INC_CPT                                       |
+----------------------------------------------------------------------------+
| Input Parameters  :
+----------------------------------------------------------------------------+
| Output Parameters : -                                                      |
+----------------------------------------------------------------------------+
| Return Value      :
+----------------------------------------------------------------------------+
*/
INT i_APCI1710_InsnWriteINCCPT(struct comedi_device * dev, struct comedi_subdevice * s,
	struct comedi_insn * insn, unsigned int * data)
{
	UINT ui_WriteType;
	INT i_ReturnValue = 0;

	ui_WriteType = CR_CHAN(insn->chanspec);
	devpriv->tsk_Current = current;	// Save the current process task structure

	switch (ui_WriteType) {
	case APCI1710_INCCPT_ENABLELATCHINTERRUPT:
		i_ReturnValue = i_APCI1710_EnableLatchInterrupt(dev,
			(BYTE) CR_AREF(insn->chanspec));
		break;

	case APCI1710_INCCPT_DISABLELATCHINTERRUPT:
		i_ReturnValue = i_APCI1710_DisableLatchInterrupt(dev,
			(BYTE) CR_AREF(insn->chanspec));
		break;

	case APCI1710_INCCPT_WRITE16BITCOUNTERVALUE:
		i_ReturnValue = i_APCI1710_Write16BitCounterValue(dev,
			(BYTE) CR_AREF(insn->chanspec),
			(BYTE) data[0], (UINT) data[1]);
		break;

	case APCI1710_INCCPT_WRITE32BITCOUNTERVALUE:
		i_ReturnValue = i_APCI1710_Write32BitCounterValue(dev,
			(BYTE) CR_AREF(insn->chanspec), (ULONG) data[0]);

		break;

	case APCI1710_INCCPT_ENABLEINDEX:
		i_APCI1710_EnableIndex(dev, (BYTE) CR_AREF(insn->chanspec));
		break;

	case APCI1710_INCCPT_DISABLEINDEX:
		i_ReturnValue = i_APCI1710_DisableIndex(dev,
			(BYTE) CR_AREF(insn->chanspec));
		break;

	case APCI1710_INCCPT_ENABLECOMPARELOGIC:
		i_ReturnValue = i_APCI1710_EnableCompareLogic(dev,
			(BYTE) CR_AREF(insn->chanspec));
		break;

	case APCI1710_INCCPT_DISABLECOMPARELOGIC:
		i_ReturnValue = i_APCI1710_DisableCompareLogic(dev,
			(BYTE) CR_AREF(insn->chanspec));
		break;

	case APCI1710_INCCPT_ENABLEFREQUENCYMEASUREMENT:
		i_ReturnValue = i_APCI1710_EnableFrequencyMeasurement(dev,
			(BYTE) CR_AREF(insn->chanspec), (BYTE) data[0]);
		break;

	case APCI1710_INCCPT_DISABLEFREQUENCYMEASUREMENT:
		i_ReturnValue = i_APCI1710_DisableFrequencyMeasurement(dev,
			(BYTE) CR_AREF(insn->chanspec));
		break;

	default:
		printk("Write Config Parameter Wrong\n");
	}

	if (i_ReturnValue >= 0)
		i_ReturnValue = insn->n;
	return (i_ReturnValue);
}

/*
+----------------------------------------------------------------------------+
| Function Name     : _INT_ i_APCI1710_EnableLatchInterrupt                  |
|                               (BYTE_ b_BoardHandle,                        |
|                                BYTE_ b_ModulNbr)                           |
+----------------------------------------------------------------------------+
| Task              : Enable the latch interrupt from selected module        |
|                     (b_ModulNbr). Each software or hardware latch occur a  |
|                     interrupt.                                             |
+----------------------------------------------------------------------------+
| Input Parameters  : BYTE_ b_BoardHandle : Handle of board APCI-1710        |
|                     BYTE_ b_ModulNbr    : Module number to configure       |
|                                           (0 to 3)                         |
+----------------------------------------------------------------------------+
| Output Parameters : -                                                      |
+----------------------------------------------------------------------------+
| Return Value      :  0: No error                                           |
|                     -1: The handle parameter of the board is wrong         |
|                     -2: No counter module found                            |
|                     -3: Counter not initialised see function               |
|                         "i_APCI1710_InitCounter"                           |
|                     -4: Interrupt routine not installed see function       |
|                         "i_APCI1710_SetBoardIntRoutine"                    |
+----------------------------------------------------------------------------+
*/

INT i_APCI1710_EnableLatchInterrupt(struct comedi_device * dev, BYTE b_ModulNbr)
{
	INT i_ReturnValue = 0;

	/**************************/
	/* Test the module number */
	/**************************/

	if (b_ModulNbr < 4) {
	   /*******************************/
		/* Test if counter initialised */
	   /*******************************/

		if (devpriv->s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {

		 /********************/
			/* Enable interrupt */
		 /********************/

			devpriv->s_ModuleInfo[b_ModulNbr].
				s_SiemensCounterInfo.
				s_ModeRegister.
				s_ByteModeRegister.
				b_ModeRegister2 = devpriv->
				s_ModuleInfo[b_ModulNbr].
				s_SiemensCounterInfo.
				s_ModeRegister.
				s_ByteModeRegister.
				b_ModeRegister2 | APCI1710_ENABLE_LATCH_INT;

		 /***************************/
			/* Write the configuration */
		 /***************************/

			outl(devpriv->s_ModuleInfo[b_ModulNbr].
				s_SiemensCounterInfo.
				s_ModeRegister.
				dw_ModeRegister1_2_3_4, devpriv->s_BoardInfos.
				ui_Address + 20 + (64 * b_ModulNbr));
		} else {
	      /****************************************/
			/* Counter not initialised see function */
			/* "i_APCI1710_InitCounter"             */
	      /****************************************/

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   /*************************************************/
		/* The selected module number parameter is wrong */
	   /*************************************************/

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return (i_ReturnValue);
}

/*
+----------------------------------------------------------------------------+
| Function Name     : _INT_ i_APCI1710_DisableLatchInterrupt                 |
|                               (BYTE_ b_BoardHandle,                        |
|                                BYTE_ b_ModulNbr)                           |
+----------------------------------------------------------------------------+
| Task              : Disable the latch interrupt from selected module       |
|                     (b_ModulNbr).                                          |
+----------------------------------------------------------------------------+
| Input Parameters  : BYTE_ b_BoardHandle : Handle of board APCI-1710        |
|                     BYTE_ b_ModulNbr    : Module number to configure       |
|                                           (0 to 3)                         |
+----------------------------------------------------------------------------+
| Output Parameters : -                                                      |
+----------------------------------------------------------------------------+
| Return Value      :  0: No error                                           |
|                     -1: The handle parameter of the board is wrong         |
|                     -2: No counter module found                            |
|                     -3: Counter not initialised see function               |
|                         "i_APCI1710_InitCounter"                           |
|                     -4: Interrupt routine not installed see function       |
|                         "i_APCI1710_SetBoardIntRoutine"                    |
+----------------------------------------------------------------------------+
*/

INT i_APCI1710_DisableLatchInterrupt(struct comedi_device * dev, BYTE b_ModulNbr)
{
	INT i_ReturnValue = 0;

	/**************************/
	/* Test the module number */
	/**************************/

	if (b_ModulNbr < 4) {
	   /*******************************/
		/* Test if counter initialised */
	   /*******************************/

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {

		 /***************************/
			/* Write the configuration */
		 /***************************/

			outl(devpriv->s_ModuleInfo[b_ModulNbr].
				s_SiemensCounterInfo.
				s_ModeRegister.
				dw_ModeRegister1_2_3_4 &
				((APCI1710_DISABLE_LATCH_INT << 8) | 0xFF),
				devpriv->s_BoardInfos.ui_Address + 20 +
				(64 * b_ModulNbr));

			mdelay(1000);

		 /*********************/
			/* Disable interrupt */
		 /*********************/

			devpriv->
				s_ModuleInfo[b_ModulNbr].
				s_SiemensCounterInfo.
				s_ModeRegister.
				s_ByteModeRegister.
				b_ModeRegister2 = devpriv->
				s_ModuleInfo[b_ModulNbr].
				s_SiemensCounterInfo.
				s_ModeRegister.
				s_ByteModeRegister.
				b_ModeRegister2 & APCI1710_DISABLE_LATCH_INT;

		} else {
	      /****************************************/
			/* Counter not initialised see function */
			/* "i_APCI1710_InitCounter"             */
	      /****************************************/

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   /*************************************************/
		/* The selected module number parameter is wrong */
	   /*************************************************/

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return (i_ReturnValue);
}

/*
+----------------------------------------------------------------------------+
| Function Name     : _INT_ i_APCI1710_Write16BitCounterValue                |
|                                               (BYTE_  b_BoardHandle        |
|                                                BYTE_  b_ModulNbr,          |
|                                                BYTE_  b_SelectedCounter,   |
|                                                UINT_ ui_WriteValue)        |
+----------------------------------------------------------------------------+
| Task              : Write a 16-Bit value (ui_WriteValue) in to the selected|
|                     16-Bit counter (b_SelectedCounter) from selected module|
|                     (b_ModulNbr).                                          |
+----------------------------------------------------------------------------+
| Input Parameters  : BYTE_ b_BoardHandle     : Handle of board APCI-1710    |
|                     BYTE_ b_ModulNbr        : Module number to configure   |
|                                              (0 to 3)                      |
|                     BYTE_ b_SelectedCounter : Selected 16-Bit counter      |
|                                               (0 or 1)                     |
|                     UINT_ ui_WriteValue     : 16-Bit write value           |
+----------------------------------------------------------------------------+
| Output Parameters : -                                                      |
+----------------------------------------------------------------------------+
| Return Value      :  0: No error                                           |
|                     -1: The handle parameter of the board is wrong         |
|                     -2: No counter module found                            |
|                     -3: Counter not initialised see function               |
|                         "i_APCI1710_InitCounter"                           |
|                     -4: The selected 16-Bit counter parameter is wrong     |
+----------------------------------------------------------------------------+
*/

INT i_APCI1710_Write16BitCounterValue(struct comedi_device * dev,
	BYTE b_ModulNbr, BYTE b_SelectedCounter, UINT ui_WriteValue)
{
	INT i_ReturnValue = 0;

	/**************************/
	/* Test the module number */
	/**************************/

	if (b_ModulNbr < 4) {
	   /*******************************/
		/* Test if counter initialised */
	   /*******************************/

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {
	      /******************************/
			/* Test the counter selection */
	      /******************************/

			if (b_SelectedCounter < 2) {
		 /*******************/
				/* Write the value */
		 /*******************/

				outl((ULONG) ((ULONG) (ui_WriteValue) << (16 *
							b_SelectedCounter)),
					devpriv->s_BoardInfos.ui_Address + 8 +
					(b_SelectedCounter * 4) +
					(64 * b_ModulNbr));
			} else {
		 /**************************************************/
				/* The selected 16-Bit counter parameter is wrong */
		 /**************************************************/

				DPRINTK("The selected 16-Bit counter parameter is wrong\n");
				i_ReturnValue = -4;
			}
		} else {
	      /****************************************/
			/* Counter not initialised see function */
			/* "i_APCI1710_InitCounter"             */
	      /****************************************/

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   /*************************************************/
		/* The selected module number parameter is wrong */
	   /*************************************************/

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return (i_ReturnValue);
}

/*
+----------------------------------------------------------------------------+
| Function Name     : _INT_ i_APCI1710_Write32BitCounterValue                |
|                                               (BYTE_   b_BoardHandle       |
|                                                BYTE_   b_ModulNbr,         |
|                                                ULONG_ ul_WriteValue)       |
+----------------------------------------------------------------------------+
| Task              : Write a 32-Bit value (ui_WriteValue) in to the selected|
|                     module (b_ModulNbr).                                   |
+----------------------------------------------------------------------------+
| Input Parameters  : BYTE_ b_BoardHandle     : Handle of board APCI-1710    |
|                     BYTE_ b_ModulNbr        : Module number to configure   |
|                                              (0 to 3)                      |
|                     ULONG_ ul_WriteValue    : 32-Bit write value           |
+----------------------------------------------------------------------------+
| Output Parameters : -                                                      |
+----------------------------------------------------------------------------+
| Return Value      :  0: No error                                           |
|                     -1: The handle parameter of the board is wrong         |
|                     -2: No counter module found                            |
|                     -3: Counter not initialised see function               |
|                         "i_APCI1710_InitCounter"                           |
+----------------------------------------------------------------------------+
*/

INT i_APCI1710_Write32BitCounterValue(struct comedi_device * dev,
	BYTE b_ModulNbr, ULONG ul_WriteValue)
{
	INT i_ReturnValue = 0;

	/**************************/
	/* Test the module number */
	/**************************/

	if (b_ModulNbr < 4) {
	   /*******************************/
		/* Test if counter initialised */
	   /*******************************/

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {
	      /*******************/
			/* Write the value */
	      /*******************/

			outl(ul_WriteValue, devpriv->s_BoardInfos.
				ui_Address + 4 + (64 * b_ModulNbr));
		} else {
	      /****************************************/
			/* Counter not initialised see function */
			/* "i_APCI1710_InitCounter"             */
	      /****************************************/

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   /*************************************************/
		/* The selected module number parameter is wrong */
	   /*************************************************/

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return (i_ReturnValue);
}

/*
+----------------------------------------------------------------------------+
| Function Name     : _INT_ i_APCI1710_EnableIndex (BYTE_  b_BoardHandle,    |
|                                                   BYTE_  b_ModulNbr)       |
+----------------------------------------------------------------------------+
| Task              : Enable the INDEX actions                               |
+----------------------------------------------------------------------------+
| Input Parameters  : BYTE_ b_BoardHandle     : Handle of board APCI-1710    |
|                     BYTE_ b_ModulNbr        : Module number to configure   |
|                                               (0 to 3)                     |
+----------------------------------------------------------------------------+
| Output Parameters : -                                                      |
+----------------------------------------------------------------------------+
| Return Value      :  0: No error                                           |
|                     -1: The handle parameter of the board is wrong         |
|                     -2: No counter module found                            |
|                     -3: Counter not initialised see function               |
|                         "i_APCI1710_InitCounter"                           |
|                     -4: Index not initialised see function                 |
|                         "i_APCI1710_InitIndex"                             |
+----------------------------------------------------------------------------+
*/

INT i_APCI1710_EnableIndex(struct comedi_device * dev, BYTE b_ModulNbr)
{
	INT i_ReturnValue = 0;
	ULONG ul_InterruptLatchReg;

	/**************************/
	/* Test the module number */
	/**************************/

	if (b_ModulNbr < 4) {
	   /*******************************/
		/* Test if counter initialised */
	   /*******************************/

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {
	      /*****************************/
			/* Test if index initialised */
	      /*****************************/

			if (devpriv->
				s_ModuleInfo[b_ModulNbr].
				s_SiemensCounterInfo.s_InitFlag.b_IndexInit) {
				devpriv->
					s_ModuleInfo[b_ModulNbr].
					s_SiemensCounterInfo.
					s_ModeRegister.
					s_ByteModeRegister.
					b_ModeRegister2 = devpriv->
					s_ModuleInfo[b_ModulNbr].
					s_SiemensCounterInfo.
					s_ModeRegister.
					s_ByteModeRegister.
					b_ModeRegister2 | APCI1710_ENABLE_INDEX;

				ul_InterruptLatchReg =
					inl(devpriv->s_BoardInfos.ui_Address +
					24 + (64 * b_ModulNbr));

				outl(devpriv->s_ModuleInfo[b_ModulNbr].
					s_SiemensCounterInfo.
					s_ModeRegister.
					dw_ModeRegister1_2_3_4,
					devpriv->s_BoardInfos.ui_Address + 20 +
					(64 * b_ModulNbr));
			} else {
		 /*************************************************************/
				/* Index not initialised see function "i_APCI1710_InitIndex" */
		 /*************************************************************/

				DPRINTK("Index not initialised \n");
				i_ReturnValue = -4;
			}
		} else {
	      /****************************************/
			/* Counter not initialised see function */
			/* "i_APCI1710_InitCounter"             */
	      /****************************************/

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   /*************************************************/
		/* The selected module number parameter is wrong */
	   /*************************************************/

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return (i_ReturnValue);
}

/*
+----------------------------------------------------------------------------+
| Function Name     : _INT_ i_APCI1710_DisableIndex (BYTE_  b_BoardHandle,   |
|                                                    BYTE_  b_ModulNbr)      |
+----------------------------------------------------------------------------+
| Task              : Disable the INDEX actions                              |
+----------------------------------------------------------------------------+
| Input Parameters  : BYTE_ b_BoardHandle     : Handle of board APCI-1710    |
|                     BYTE_ b_ModulNbr        : Module number to configure   |
|                                               (0 to 3)                     |
+----------------------------------------------------------------------------+
| Output Parameters : -                                                      |
+----------------------------------------------------------------------------+
| Return Value      :  0: No error                                           |
|                     -1: The handle parameter of the board is wrong         |
|                     -2: No counter module found                            |
|                     -3: Counter not initialised see function               |
|                         "i_APCI1710_InitCounter"                           |
|                     -4: Index not initialised see function                 |
|                         "i_APCI1710_InitIndex"                             |
+----------------------------------------------------------------------------+
*/

INT i_APCI1710_DisableIndex(struct comedi_device * dev, BYTE b_ModulNbr)
{
	INT i_ReturnValue = 0;

	/**************************/
	/* Test the module number */
	/**************************/

	if (b_ModulNbr < 4) {
	   /*******************************/
		/* Test if counter initialised */
	   /*******************************/

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {
	      /*****************************/
			/* Test if index initialised */
	      /*****************************/

			if (devpriv->
				s_ModuleInfo[b_ModulNbr].
				s_SiemensCounterInfo.s_InitFlag.b_IndexInit) {
				devpriv->
					s_ModuleInfo[b_ModulNbr].
					s_SiemensCounterInfo.
					s_ModeRegister.
					s_ByteModeRegister.
					b_ModeRegister2 = devpriv->
					s_ModuleInfo[b_ModulNbr].
					s_SiemensCounterInfo.
					s_ModeRegister.
					s_ByteModeRegister.
					b_ModeRegister2 &
					APCI1710_DISABLE_INDEX;

				outl(devpriv->s_ModuleInfo[b_ModulNbr].
					s_SiemensCounterInfo.
					s_ModeRegister.
					dw_ModeRegister1_2_3_4,
					devpriv->s_BoardInfos.ui_Address + 20 +
					(64 * b_ModulNbr));
			} else {
		 /*************************************************************/
				/* Index not initialised see function "i_APCI1710_InitIndex" */
		 /*************************************************************/

				DPRINTK("Index not initialised  \n");
				i_ReturnValue = -4;
			}
		} else {
	      /****************************************/
			/* Counter not initialised see function */
			/* "i_APCI1710_InitCounter"             */
	      /****************************************/

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   /*************************************************/
		/* The selected module number parameter is wrong */
	   /*************************************************/

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return (i_ReturnValue);
}

/*
+----------------------------------------------------------------------------+
| Function Name     : _INT_ i_APCI1710_EnableCompareLogic                    |
|                               (BYTE_   b_BoardHandle,                      |
|                                BYTE_   b_ModulNbr)                         |
+----------------------------------------------------------------------------+
| Task              : Enable the 32-Bit compare logic. At that moment that   |
|                     the incremental counter arrive to the compare value a  |
|                     interrupt is generated.                                |
+----------------------------------------------------------------------------+
| Input Parameters  : BYTE_  b_BoardHandle    : Handle of board APCI-1710    |
|                     BYTE_  b_ModulNbr       : Module number to configure   |
|                                               (0 to 3)                     |
+----------------------------------------------------------------------------+
| Output Parameters : -
+----------------------------------------------------------------------------+
| Return Value      :  0: No error                                           |
|                     -1: The handle parameter of the board is wrong         |
|                     -2: No counter module found                            |
|                     -3: Counter not initialised see function               |
|                         "i_APCI1710_InitCounter"                           |
|                     -4: Compare logic not initialised.                     |
|                         See function "i_APCI1710_InitCompareLogic"         |
|                     -5: Interrupt function not initialised.                |
|                         See function "i_APCI1710_SetBoardIntRoutineX"      |
+----------------------------------------------------------------------------+
*/

INT i_APCI1710_EnableCompareLogic(struct comedi_device * dev, BYTE b_ModulNbr)
{
	INT i_ReturnValue = 0;

	/**************************/
	/* Test the module number */
	/**************************/

	if (b_ModulNbr < 4) {
	   /*******************************/
		/* Test if counter initialised */
	   /*******************************/

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {
	      /*************************************/
			/* Test if compare logic initialised */
	      /*************************************/

			if (devpriv->
				s_ModuleInfo[b_ModulNbr].
				s_SiemensCounterInfo.
				s_InitFlag.b_CompareLogicInit == 1) {
				devpriv->
					s_ModuleInfo[b_ModulNbr].
					s_SiemensCounterInfo.
					s_ModeRegister.
					s_ByteModeRegister.
					b_ModeRegister3 = devpriv->
					s_ModuleInfo[b_ModulNbr].
					s_SiemensCounterInfo.
					s_ModeRegister.
					s_ByteModeRegister.
					b_ModeRegister3 |
					APCI1710_ENABLE_COMPARE_INT;

		    /***************************/
				/* Write the configuration */
		    /***************************/

				outl(devpriv->s_ModuleInfo[b_ModulNbr].
					s_SiemensCounterInfo.
					s_ModeRegister.
					dw_ModeRegister1_2_3_4,
					devpriv->s_BoardInfos.ui_Address + 20 +
					(64 * b_ModulNbr));
			} else {
		 /*********************************/
				/* Compare logic not initialised */
		 /*********************************/

				DPRINTK("Compare logic not initialised\n");
				i_ReturnValue = -4;
			}
		} else {
	      /****************************************/
			/* Counter not initialised see function */
			/* "i_APCI1710_InitCounter"             */
	      /****************************************/

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   /*************************************************/
		/* The selected module number parameter is wrong */
	   /*************************************************/

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return (i_ReturnValue);
}

/*
+----------------------------------------------------------------------------+
| Function Name     : _INT_ i_APCI1710_DisableCompareLogic                   |
|                               (BYTE_   b_BoardHandle,                      |
|                                BYTE_   b_ModulNbr)                         |
+----------------------------------------------------------------------------+
| Task              : Disable the 32-Bit compare logic.
+----------------------------------------------------------------------------+
| Input Parameters  : BYTE_  b_BoardHandle    : Handle of board APCI-1710    |
|                     BYTE_  b_ModulNbr       : Module number to configure   |
|                                               (0 to 3)                     |
+----------------------------------------------------------------------------+
| Output Parameters : -
+----------------------------------------------------------------------------+
| Return Value      :  0: No error                                           |
|                     -1: The handle parameter of the board is wrong         |
|                     -2: No counter module found                            |
|                     -3: Counter not initialised see function               |
|                         "i_APCI1710_InitCounter"                           |
|                     -4: Compare logic not initialised.                     |
|                         See function "i_APCI1710_InitCompareLogic"         |
+----------------------------------------------------------------------------+
*/

INT i_APCI1710_DisableCompareLogic(struct comedi_device * dev, BYTE b_ModulNbr)
{
	INT i_ReturnValue = 0;

	/**************************/
	/* Test the module number */
	/**************************/

	if (b_ModulNbr < 4) {
	   /*******************************/
		/* Test if counter initialised */
	   /*******************************/

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {
	      /*************************************/
			/* Test if compare logic initialised */
	      /*************************************/

			if (devpriv->
				s_ModuleInfo[b_ModulNbr].
				s_SiemensCounterInfo.
				s_InitFlag.b_CompareLogicInit == 1) {
				devpriv->
					s_ModuleInfo[b_ModulNbr].
					s_SiemensCounterInfo.
					s_ModeRegister.
					s_ByteModeRegister.
					b_ModeRegister3 = devpriv->
					s_ModuleInfo[b_ModulNbr].
					s_SiemensCounterInfo.
					s_ModeRegister.
					s_ByteModeRegister.
					b_ModeRegister3 &
					APCI1710_DISABLE_COMPARE_INT;

		 /***************************/
				/* Write the configuration */
		 /***************************/

				outl(devpriv->s_ModuleInfo[b_ModulNbr].
					s_SiemensCounterInfo.
					s_ModeRegister.
					dw_ModeRegister1_2_3_4,
					devpriv->s_BoardInfos.ui_Address + 20 +
					(64 * b_ModulNbr));
			} else {
		 /*********************************/
				/* Compare logic not initialised */
		 /*********************************/

				DPRINTK("Compare logic not initialised\n");
				i_ReturnValue = -4;
			}
		} else {
	      /****************************************/
			/* Counter not initialised see function */
			/* "i_APCI1710_InitCounter"             */
	      /****************************************/

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   /*************************************************/
		/* The selected module number parameter is wrong */
	   /*************************************************/

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return (i_ReturnValue);
}

	/*
	   +----------------------------------------------------------------------------+
	   | Function Name     : _INT_ i_APCI1710_EnableFrequencyMeasurement            |
	   |                            (BYTE_   b_BoardHandle,                      |
	   |                             BYTE_   b_ModulNbr,                         |
	   |                             BYTE_   b_InterruptEnable)                  |
	   +----------------------------------------------------------------------------+
	   | Task              : Enables the frequency measurement function             |
	   +----------------------------------------------------------------------------+
	   | Input Parameters  : BYTE_  b_BoardHandle    : Handle of board APCI-1710    |
	   |                  BYTE_  b_ModulNbr       : Number of the module to be   |
	   |                                            configured (0 to 3)          |
	   |                  BYTE_  b_InterruptEnable: Enable or disable the        |
	   |                                            interrupt.                   |
	   |                                            APCI1710_ENABLE:             |
	   |                                            Enable the interrupt         |
	   |                                            APCI1710_DISABLE:            |
	   |                                            Disable the interrupt        |
	   +----------------------------------------------------------------------------+
	   | Output Parameters : -                                                      |
	   +----------------------------------------------------------------------------+
	   | Return Value      :  0: No error                                           |
	   |                     -1: The handle parameter of the board is wrong         |
	   |                     -2: The selected module number is wrong                |
	   |                     -3: Counter not initialised see function               |
	   |                      "i_APCI1710_InitCounter"                           |
	   |                     -4: Frequency measurement logic not initialised.       |
	   |                      See function "i_APCI1710_InitFrequencyMeasurement" |
	   |                     -5: Interrupt parameter is wrong                       |
	   |                     -6: Interrupt function not initialised.                |
	   +----------------------------------------------------------------------------+
	 */

INT i_APCI1710_EnableFrequencyMeasurement(struct comedi_device * dev,
	BYTE b_ModulNbr, BYTE b_InterruptEnable)
{
	INT i_ReturnValue = 0;

	/**************************/
	/* Test the module number */
	/**************************/

	if (b_ModulNbr < 4) {
	   /*******************************/
		/* Test if counter initialised */
	   /*******************************/

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {
	      /********************************************/
			/* Test if frequency mesurement initialised */
	      /********************************************/

			if (devpriv->
				s_ModuleInfo[b_ModulNbr].
				s_SiemensCounterInfo.
				s_InitFlag.b_FrequencyMeasurementInit == 1) {
		 /***************************/
				/* Test the interrupt mode */
		 /***************************/

				if ((b_InterruptEnable == APCI1710_DISABLE) ||
					(b_InterruptEnable == APCI1710_ENABLE))
				{

		       /************************************/
					/* Enable the frequency measurement */
		       /************************************/

					devpriv->
						s_ModuleInfo[b_ModulNbr].
						s_SiemensCounterInfo.
						s_ModeRegister.
						s_ByteModeRegister.
						b_ModeRegister3 = devpriv->
						s_ModuleInfo[b_ModulNbr].
						s_SiemensCounterInfo.
						s_ModeRegister.
						s_ByteModeRegister.
						b_ModeRegister3 |
						APCI1710_ENABLE_FREQUENCY;

		       /*********************************************/
					/* Disable or enable the frequency interrupt */
		       /*********************************************/

					devpriv->
						s_ModuleInfo[b_ModulNbr].
						s_SiemensCounterInfo.
						s_ModeRegister.
						s_ByteModeRegister.
						b_ModeRegister3 = (devpriv->
						s_ModuleInfo[b_ModulNbr].
						s_SiemensCounterInfo.
						s_ModeRegister.
						s_ByteModeRegister.
						b_ModeRegister3 &
						APCI1710_DISABLE_FREQUENCY_INT)
						| (b_InterruptEnable << 3);

		       /***************************/
					/* Write the configuration */
		       /***************************/

					outl(devpriv->s_ModuleInfo[b_ModulNbr].
						s_SiemensCounterInfo.
						s_ModeRegister.
						dw_ModeRegister1_2_3_4,
						devpriv->s_BoardInfos.
						ui_Address + 20 +
						(64 * b_ModulNbr));

					devpriv->
						s_ModuleInfo[b_ModulNbr].
						s_SiemensCounterInfo.
						s_InitFlag.
						b_FrequencyMeasurementEnable =
						1;
				} else {
		    /********************************/
					/* Interrupt parameter is wrong */
		    /********************************/

					DPRINTK("Interrupt parameter is wrong\n");
					i_ReturnValue = -5;
				}
			} else {
		 /***********************************************/
				/* Frequency measurement logic not initialised */
		 /***********************************************/

				DPRINTK("Frequency measurement logic not initialised\n");
				i_ReturnValue = -4;
			}
		} else {
	      /****************************************/
			/* Counter not initialised see function */
			/* "i_APCI1710_InitCounter"             */
	      /****************************************/

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   /*************************************************/
		/* The selected module number parameter is wrong */
	   /*************************************************/

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return (i_ReturnValue);
}

	/*
	   +----------------------------------------------------------------------------+
	   | Function Name     : _INT_ i_APCI1710_DisableFrequencyMeasurement           |
	   |                            (BYTE_   b_BoardHandle,                      |
	   |                             BYTE_   b_ModulNbr)                         |
	   +----------------------------------------------------------------------------+
	   | Task              : Disables the frequency measurement function             |
	   +----------------------------------------------------------------------------+
	   | Input Parameters  : BYTE_  b_BoardHandle    : Handle of board APCI-1710    |
	   |                  BYTE_  b_ModulNbr       : Number of the module to be   |
	   |                                            configured (0 to 3)          |
	   +----------------------------------------------------------------------------+
	   | Output Parameters : -                                                      |
	   +----------------------------------------------------------------------------+
	   | Return Value      :  0: No error                                           |
	   |                     -1: The handle parameter of the board is wrong         |
	   |                     -2: The selected module number is wrong                |
	   |                     -3: Counter not initialised see function               |
	   |                      "i_APCI1710_InitCounter"                           |
	   |                     -4: Frequency measurement logic not initialised.       |
	   |                      See function "i_APCI1710_InitFrequencyMeasurement" |
	   +----------------------------------------------------------------------------+
	 */

INT i_APCI1710_DisableFrequencyMeasurement(struct comedi_device * dev, BYTE b_ModulNbr)
{
	INT i_ReturnValue = 0;

	/**************************/
	/* Test the module number */
	/**************************/

	if (b_ModulNbr < 4) {
	   /*******************************/
		/* Test if counter initialised */
	   /*******************************/

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {
	      /********************************************/
			/* Test if frequency mesurement initialised */
	      /********************************************/

			if (devpriv->
				s_ModuleInfo[b_ModulNbr].
				s_SiemensCounterInfo.
				s_InitFlag.b_FrequencyMeasurementInit == 1) {
		 /*************************************/
				/* Disable the frequency measurement */
		 /*************************************/

				devpriv->
					s_ModuleInfo[b_ModulNbr].
					s_SiemensCounterInfo.
					s_ModeRegister.
					s_ByteModeRegister.
					b_ModeRegister3 = devpriv->
					s_ModuleInfo[b_ModulNbr].
					s_SiemensCounterInfo.
					s_ModeRegister.
					s_ByteModeRegister.
					b_ModeRegister3 &
					APCI1710_DISABLE_FREQUENCY
					// Begin CG 29/06/01 CG 1100/0231 -> 0701/0232 Frequence measure IRQ must be cleared
					& APCI1710_DISABLE_FREQUENCY_INT;
				// End CG 29/06/01 CG 1100/0231 -> 0701/0232 Frequence measure IRQ must be cleared

		 /***************************/
				/* Write the configuration */
		 /***************************/

				outl(devpriv->s_ModuleInfo[b_ModulNbr].
					s_SiemensCounterInfo.
					s_ModeRegister.
					dw_ModeRegister1_2_3_4,
					devpriv->s_BoardInfos.ui_Address + 20 +
					(64 * b_ModulNbr));

		 /*************************************/
				/* Disable the frequency measurement */
		 /*************************************/

				devpriv->
					s_ModuleInfo[b_ModulNbr].
					s_SiemensCounterInfo.
					s_InitFlag.
					b_FrequencyMeasurementEnable = 0;
			} else {
		 /***********************************************/
				/* Frequency measurement logic not initialised */
		 /***********************************************/

				DPRINTK("Frequency measurement logic not initialised\n");
				i_ReturnValue = -4;
			}
		} else {
	      /****************************************/
			/* Counter not initialised see function */
			/* "i_APCI1710_InitCounter"             */
	      /****************************************/

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   /*************************************************/
		/* The selected module number parameter is wrong */
	   /*************************************************/

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return (i_ReturnValue);
}

/*########################################################################### */

							// INSN READ

/*########################################################################### */

/*
+----------------------------------------------------------------------------+
| Function Name     :INT	i_APCI1710_InsnWriteINCCPT(struct comedi_device *dev,struct comedi_subdevice *s,
struct comedi_insn *insn,unsigned int *data)                   |
+----------------------------------------------------------------------------+
| Task              : Read and Get functions for INC_CPT                                       |
+----------------------------------------------------------------------------+
| Input Parameters  :
+----------------------------------------------------------------------------+
| Output Parameters : -                                                      |
+----------------------------------------------------------------------------+
| Return Value      :
+----------------------------------------------------------------------------+
*/
INT i_APCI1710_InsnReadINCCPT(struct comedi_device * dev, struct comedi_subdevice * s,
	struct comedi_insn * insn, unsigned int * data)
{
	UINT ui_ReadType;
	INT i_ReturnValue = 0;

	ui_ReadType = CR_CHAN(insn->chanspec);

	devpriv->tsk_Current = current;	// Save the current process task structure
	switch (ui_ReadType) {
	case APCI1710_INCCPT_READLATCHREGISTERSTATUS:
		i_ReturnValue = i_APCI1710_ReadLatchRegisterStatus(dev,
			(BYTE) CR_AREF(insn->chanspec),
			(BYTE) CR_RANGE(insn->chanspec), (PBYTE) & data[0]);
		break;

	case APCI1710_INCCPT_READLATCHREGISTERVALUE:
		i_ReturnValue = i_APCI1710_ReadLatchRegisterValue(dev,
			(BYTE) CR_AREF(insn->chanspec),
			(BYTE) CR_RANGE(insn->chanspec), (PULONG) & data[0]);
		printk("Latch Register Value %d\n", data[0]);
		break;

	case APCI1710_INCCPT_READ16BITCOUNTERVALUE:
		i_ReturnValue = i_APCI1710_Read16BitCounterValue(dev,
			(BYTE) CR_AREF(insn->chanspec),
			(BYTE) CR_RANGE(insn->chanspec), (PUINT) & data[0]);
		break;

	case APCI1710_INCCPT_READ32BITCOUNTERVALUE:
		i_ReturnValue = i_APCI1710_Read32BitCounterValue(dev,
			(BYTE) CR_AREF(insn->chanspec), (PULONG) & data[0]);
		break;

	case APCI1710_INCCPT_GETINDEXSTATUS:
		i_ReturnValue = i_APCI1710_GetIndexStatus(dev,
			(BYTE) CR_AREF(insn->chanspec), (PBYTE) & data[0]);
		break;

	case APCI1710_INCCPT_GETREFERENCESTATUS:
		i_ReturnValue = i_APCI1710_GetReferenceStatus(dev,
			(BYTE) CR_AREF(insn->chanspec), (PBYTE) & data[0]);
		break;

	case APCI1710_INCCPT_GETUASSTATUS:
		i_ReturnValue = i_APCI1710_GetUASStatus(dev,
			(BYTE) CR_AREF(insn->chanspec), (PBYTE) & data[0]);
		break;

	case APCI1710_INCCPT_GETCBSTATUS:
		i_ReturnValue = i_APCI1710_GetCBStatus(dev,
			(BYTE) CR_AREF(insn->chanspec), (PBYTE) & data[0]);
		break;

	case APCI1710_INCCPT_GET16BITCBSTATUS:
		i_ReturnValue = i_APCI1710_Get16BitCBStatus(dev,
			(BYTE) CR_AREF(insn->chanspec),
			(PBYTE) & data[0], (PBYTE) & data[1]);
		break;

	case APCI1710_INCCPT_GETUDSTATUS:
		i_ReturnValue = i_APCI1710_GetUDStatus(dev,
			(BYTE) CR_AREF(insn->chanspec), (PBYTE) & data[0]);

		break;

	case APCI1710_INCCPT_GETINTERRUPTUDLATCHEDSTATUS:
		i_ReturnValue = i_APCI1710_GetInterruptUDLatchedStatus(dev,
			(BYTE) CR_AREF(insn->chanspec), (PBYTE) & data[0]);
		break;

	case APCI1710_INCCPT_READFREQUENCYMEASUREMENT:
		i_ReturnValue = i_APCI1710_ReadFrequencyMeasurement(dev,
			(BYTE) CR_AREF(insn->chanspec),
			(PBYTE) & data[0],
			(PBYTE) & data[1], (PULONG) & data[2]);
		break;

	case APCI1710_INCCPT_READINTERRUPT:
		data[0] = devpriv->s_InterruptParameters.
			s_FIFOInterruptParameters[devpriv->
			s_InterruptParameters.ui_Read].b_OldModuleMask;
		data[1] = devpriv->s_InterruptParameters.
			s_FIFOInterruptParameters[devpriv->
			s_InterruptParameters.ui_Read].ul_OldInterruptMask;
		data[2] = devpriv->s_InterruptParameters.
			s_FIFOInterruptParameters[devpriv->
			s_InterruptParameters.ui_Read].ul_OldCounterLatchValue;

		/**************************/
		/* Increment the read FIFO */
		/***************************/

		devpriv->
			s_InterruptParameters.
			ui_Read = (devpriv->s_InterruptParameters.
			ui_Read + 1) % APCI1710_SAVE_INTERRUPT;

		break;

	default:
		printk("ReadType Parameter wrong\n");
	}

	if (i_ReturnValue >= 0)
		i_ReturnValue = insn->n;
	return (i_ReturnValue);

}

/*
+----------------------------------------------------------------------------+
| Function Name     : _INT_ i_APCI1710_ReadLatchRegisterStatus               |
|                                                   (BYTE_   b_BoardHandle,  |
|                                                    BYTE_   b_ModulNbr,     |
|                                                    BYTE_   b_LatchReg,     |
|                                                    PBYTE_ pb_LatchStatus)  |
+----------------------------------------------------------------------------+
| Task              : Read the latch register status from selected module    |
|                     (b_ModulNbr) and selected latch register (b_LatchReg). |
+----------------------------------------------------------------------------+
| Input Parameters  : BYTE_ b_BoardHandle : Handle of board APCI-1710        |
|                     BYTE_ b_ModulNbr    : Module number to configure       |
|                                           (0 to 3)                         |
|                     BYTE_ b_LatchReg    : Selected latch register          |
|                               0 : for the first latch register             |
|                               1 : for the second latch register            |
+----------------------------------------------------------------------------+
| Output Parameters : PBYTE_ pb_LatchStatus :   Latch register status.       |
|                                               0 : No latch occur           |
|                                               1 : A software latch occur   |
|                                               2 : A hardware latch occur   |
|                                               3 : A software and hardware  |
|                                                   latch occur              |
+----------------------------------------------------------------------------+
| Return Value      :  0: No error                                           |
|                     -1: The handle parameter of the board is wrong         |
|                     -2: No counter module found                            |
|                     -3: Counter not initialised see function               |
|                         "i_APCI1710_InitCounter"                           |
|                     -4: The selected latch register parameter is wrong     |
+----------------------------------------------------------------------------+
*/

INT i_APCI1710_ReadLatchRegisterStatus(struct comedi_device * dev,
	BYTE b_ModulNbr, BYTE b_LatchReg, PBYTE pb_LatchStatus)
{
	INT i_ReturnValue = 0;
	DWORD dw_LatchReg;

	/**************************/
	/* Test the module number */
	/**************************/

	if (b_ModulNbr < 4) {
	   /*******************************/
		/* Test if counter initialised */
	   /*******************************/

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {
	      /*************************************/
			/* Test the latch register parameter */
	      /*************************************/

			if (b_LatchReg < 2) {
				dw_LatchReg = inl(devpriv->s_BoardInfos.
					ui_Address + (64 * b_ModulNbr));

				*pb_LatchStatus =
					(BYTE) ((dw_LatchReg >> (b_LatchReg *
							4)) & 0x3);
			} else {
		 /**************************************************/
				/* The selected latch register parameter is wrong */
		 /**************************************************/

				DPRINTK("The selected latch register parameter is wrong\n");
				i_ReturnValue = -4;
			}
		} else {
	      /****************************************/
			/* Counter not initialised see function */
			/* "i_APCI1710_InitCounter"             */
	      /****************************************/

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   /*************************************************/
		/* The selected module number parameter is wrong */
	   /*************************************************/

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return (i_ReturnValue);
}

/*
+----------------------------------------------------------------------------+
| Function Name     : _INT_ i_APCI1710_ReadLatchRegisterValue                |
|                                                   (BYTE_     b_BoardHandle,|
|                                                    BYTE_     b_ModulNbr,   |
|                                                    BYTE_     b_LatchReg,   |
|                                                    PULONG_ pul_LatchValue) |
+----------------------------------------------------------------------------+
| Task              : Read the latch register value from selected module     |
|                     (b_ModulNbr) and selected latch register (b_LatchReg). |
+----------------------------------------------------------------------------+
| Input Parameters  : BYTE_ b_BoardHandle : Handle of board APCI-1710        |
|                     BYTE_ b_ModulNbr    : Module number to configure       |
|                                           (0 to 3)                         |
|                     BYTE_ b_LatchReg    : Selected latch register          |
|                               0 : for the first latch register             |
|                               1 : for the second latch register            |
+----------------------------------------------------------------------------+
| Output Parameters : PULONG_ pul_LatchValue : Latch register value          |
+----------------------------------------------------------------------------+
| Return Value      :  0: No error                                           |
|                     -1: The handle parameter of the board is wrong         |
|                     -2: No counter module found                            |
|                     -3: Counter not initialised see function               |
|                         "i_APCI1710_InitCounter"                           |
|                     -4: The selected latch register parameter is wrong     |
+----------------------------------------------------------------------------+
*/

INT i_APCI1710_ReadLatchRegisterValue(struct comedi_device * dev,
	BYTE b_ModulNbr, BYTE b_LatchReg, PULONG pul_LatchValue)
{
	INT i_ReturnValue = 0;

	/**************************/
	/* Test the module number */
	/**************************/

	if (b_ModulNbr < 4) {
	   /*******************************/
		/* Test if counter initialised */
	   /*******************************/

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {
	      /*************************************/
			/* Test the latch register parameter */
	      /*************************************/

			if (b_LatchReg < 2) {
				*pul_LatchValue = inl(devpriv->s_BoardInfos.
					ui_Address + ((b_LatchReg + 1) * 4) +
					(64 * b_ModulNbr));

			} else {
		 /**************************************************/
				/* The selected latch register parameter is wrong */
		 /**************************************************/

				DPRINTK("The selected latch register parameter is wrong\n");
				i_ReturnValue = -4;
			}
		} else {
	      /****************************************/
			/* Counter not initialised see function */
			/* "i_APCI1710_InitCounter"             */
	      /****************************************/

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   /*************************************************/
		/* The selected module number parameter is wrong */
	   /*************************************************/

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return (i_ReturnValue);
}

/*
+----------------------------------------------------------------------------+
| Function Name     : _INT_ i_APCI1710_Read16BitCounterValue                 |
|                                       (BYTE_     b_BoardHandle,            |
|                                        BYTE_     b_ModulNbr,               |
|                                        BYTE_     b_SelectedCounter,        |
|                                        PUINT_   pui_CounterValue)          |
+----------------------------------------------------------------------------+
| Task              : Latch the selected 16-Bit counter (b_SelectedCounter)  |
|                     from selected module (b_ModulNbr) in to the first      |
|                     latch register and return the latched value.           |
+----------------------------------------------------------------------------+
| Input Parameters  : BYTE_ b_BoardHandle     : Handle of board APCI-1710    |
|                     BYTE_ b_ModulNbr        : Module number to configure   |
|                                              (0 to 3)                      |
|                     BYTE_ b_SelectedCounter : Selected 16-Bit counter      |
|                                               (0 or 1)                     |
+----------------------------------------------------------------------------+
| Output Parameters : PUINT_ pui_CounterValue : 16-Bit counter value         |
+----------------------------------------------------------------------------+
| Return Value      :  0: No error                                           |
|                     -1: The handle parameter of the board is wrong         |
|                     -2: No counter module found                            |
|                     -3: Counter not initialised see function               |
|                         "i_APCI1710_InitCounter"                           |
|                     -4: The selected 16-Bit counter parameter is wrong     |
+----------------------------------------------------------------------------+
*/

INT i_APCI1710_Read16BitCounterValue(struct comedi_device * dev,
	BYTE b_ModulNbr, BYTE b_SelectedCounter, PUINT pui_CounterValue)
{
	INT i_ReturnValue = 0;
	DWORD dw_LathchValue = 0;

	/**************************/
	/* Test the module number */
	/**************************/

	if (b_ModulNbr < 4) {
	   /*******************************/
		/* Test if counter initialised */
	   /*******************************/

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {
	      /******************************/
			/* Test the counter selection */
	      /******************************/

			if (b_SelectedCounter < 2) {
		 /*********************/
				/* Latch the counter */
		 /*********************/

				outl(1, devpriv->s_BoardInfos.
					ui_Address + (64 * b_ModulNbr));

		 /************************/
				/* Read the latch value */
		 /************************/

				dw_LathchValue = inl(devpriv->s_BoardInfos.
					ui_Address + 4 + (64 * b_ModulNbr));

				*pui_CounterValue =
					(UINT) ((dw_LathchValue >> (16 *
							b_SelectedCounter)) &
					0xFFFFU);
			} else {
		 /**************************************************/
				/* The selected 16-Bit counter parameter is wrong */
		 /**************************************************/

				DPRINTK("The selected 16-Bit counter parameter is wrong\n");
				i_ReturnValue = -4;
			}
		} else {
	      /****************************************/
			/* Counter not initialised see function */
			/* "i_APCI1710_InitCounter"             */
	      /****************************************/

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   /*************************************************/
		/* The selected module number parameter is wrong */
	   /*************************************************/

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return (i_ReturnValue);
}

/*
+----------------------------------------------------------------------------+
| Function Name     : _INT_ i_APCI1710_Read32BitCounterValue                 |
|                                       (BYTE_     b_BoardHandle,            |
|                                        BYTE_     b_ModulNbr,               |
|                                        PULONG_ pul_CounterValue)           |
+----------------------------------------------------------------------------+
| Task              : Latch the 32-Bit counter from selected module          |
|                     (b_ModulNbr) in to the first latch register and return |
|                     the latched value.                                     |
+----------------------------------------------------------------------------+
| Input Parameters  : BYTE_ b_BoardHandle     : Handle of board APCI-1710    |
|                     BYTE_ b_ModulNbr        : Module number to configure   |
|                                              (0 to 3)                      |
+----------------------------------------------------------------------------+
| Output Parameters : PULONG_  pul_CounterValue : 32-Bit counter value       |
+----------------------------------------------------------------------------+
| Return Value      :  0: No error                                           |
|                     -1: The handle parameter of the board is wrong         |
|                     -2: No counter module found                            |
|                     -3: Counter not initialised see function               |
|                         "i_APCI1710_InitCounter"                           |
+----------------------------------------------------------------------------+
*/

INT i_APCI1710_Read32BitCounterValue(struct comedi_device * dev,
	BYTE b_ModulNbr, PULONG pul_CounterValue)
{
	INT i_ReturnValue = 0;

	/**************************/
	/* Test the module number */
	/**************************/

	if (b_ModulNbr < 4) {
	   /*******************************/
		/* Test if counter initialised */
	   /*******************************/

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {
	      /*********************/
			/* Tatch the counter */
	      /*********************/

			outl(1, devpriv->s_BoardInfos.
				ui_Address + (64 * b_ModulNbr));

	      /************************/
			/* Read the latch value */
	      /************************/

			*pul_CounterValue = inl(devpriv->s_BoardInfos.
				ui_Address + 4 + (64 * b_ModulNbr));
		} else {
	      /****************************************/
			/* Counter not initialised see function */
			/* "i_APCI1710_InitCounter"             */
	      /****************************************/

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   /*************************************************/
		/* The selected module number parameter is wrong */
	   /*************************************************/

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return (i_ReturnValue);
}

/*
+----------------------------------------------------------------------------+
| Function Name     : _INT_ i_APCI1710_GetIndexStatus (BYTE_   b_BoardHandle,|
|                                                      BYTE_   b_ModulNbr,   |
|                                                      PBYTE_ pb_IndexStatus)|
+----------------------------------------------------------------------------+
| Task              : Return the index status                                |
+----------------------------------------------------------------------------+
| Input Parameters  : BYTE_ b_BoardHandle     : Handle of board APCI-1710    |
|                     BYTE_ b_ModulNbr        : Module number to configure   |
|                                               (0 to 3)                     |
+----------------------------------------------------------------------------+
| Output Parameters : PBYTE_ pb_IndexStatus   : 0 : No INDEX occur           |
|                                               1 : A INDEX occur            |
+----------------------------------------------------------------------------+
| Return Value      :  0: No error                                           |
|                     -1: The handle parameter of the board is wrong         |
|                     -2: No counter module found                            |
|                     -3: Counter not initialised see function               |
|                         "i_APCI1710_InitCounter"                           |
|                     -4: Index not initialised see function                 |
|                         "i_APCI1710_InitIndex"                             |
+----------------------------------------------------------------------------+
*/

INT i_APCI1710_GetIndexStatus(struct comedi_device * dev,
	BYTE b_ModulNbr, PBYTE pb_IndexStatus)
{
	INT i_ReturnValue = 0;
	DWORD dw_StatusReg = 0;

	/**************************/
	/* Test the module number */
	/**************************/

	if (b_ModulNbr < 4) {
	   /*******************************/
		/* Test if counter initialised */
	   /*******************************/

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {
	      /*****************************/
			/* Test if index initialised */
	      /*****************************/

			if (devpriv->
				s_ModuleInfo[b_ModulNbr].
				s_SiemensCounterInfo.s_InitFlag.b_IndexInit) {
				dw_StatusReg = inl(devpriv->s_BoardInfos.
					ui_Address + 12 + (64 * b_ModulNbr));

				*pb_IndexStatus = (BYTE) (dw_StatusReg & 1);
			} else {
		 /*************************************************************/
				/* Index not initialised see function "i_APCI1710_InitIndex" */
		 /*************************************************************/

				DPRINTK("Index not initialised\n");
				i_ReturnValue = -4;
			}
		} else {
	      /****************************************/
			/* Counter not initialised see function */
			/* "i_APCI1710_InitCounter"             */
	      /****************************************/

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   /*************************************************/
		/* The selected module number parameter is wrong */
	   /*************************************************/

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return (i_ReturnValue);
}

/*
+----------------------------------------------------------------------------+
| Function Name     : _INT_ i_APCI1710_GetReferenceStatus                    |
|                                                (BYTE_   b_BoardHandle,     |
|                                                 BYTE_   b_ModulNbr,        |
|                                                 PBYTE_ pb_ReferenceStatus) |
+----------------------------------------------------------------------------+
| Task              : Return the reference status                            |
+----------------------------------------------------------------------------+
| Input Parameters  : BYTE_ b_BoardHandle     : Handle of board APCI-1710    |
|                     BYTE_ b_ModulNbr        : Module number to configure   |
|                                               (0 to 3)                     |
+----------------------------------------------------------------------------+
| Output Parameters : PBYTE_ pb_ReferenceStatus   : 0 : No REFERENCE occur   |
|                                                   1 : A REFERENCE occur    |
+----------------------------------------------------------------------------+
| Return Value      :  0: No error                                           |
|                     -1: The handle parameter of the board is wrong         |
|                     -2: No counter module found                            |
|                     -3: Counter not initialised see function               |
|                         "i_APCI1710_InitCounter"                           |
|                     -4: Reference not initialised see function             |
|                         "i_APCI1710_InitReference"                         |
+----------------------------------------------------------------------------+
*/

INT i_APCI1710_GetReferenceStatus(struct comedi_device * dev,
	BYTE b_ModulNbr, PBYTE pb_ReferenceStatus)
{
	INT i_ReturnValue = 0;
	DWORD dw_StatusReg = 0;

	/**************************/
	/* Test the module number */
	/**************************/

	if (b_ModulNbr < 4) {
	   /*******************************/
		/* Test if counter initialised */
	   /*******************************/

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {
	      /*********************************/
			/* Test if reference initialised */
	      /*********************************/

			if (devpriv->
				s_ModuleInfo[b_ModulNbr].
				s_SiemensCounterInfo.
				s_InitFlag.b_ReferenceInit) {
				dw_StatusReg = inl(devpriv->s_BoardInfos.
					ui_Address + 24 + (64 * b_ModulNbr));

				*pb_ReferenceStatus =
					(BYTE) (~dw_StatusReg & 1);
			} else {
		 /*********************************************************************/
				/* Reference not initialised see function "i_APCI1710_InitReference" */
		 /*********************************************************************/

				DPRINTK("Reference not initialised\n");
				i_ReturnValue = -4;
			}
		} else {
	      /****************************************/
			/* Counter not initialised see function */
			/* "i_APCI1710_InitCounter"             */
	      /****************************************/

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   /*************************************************/
		/* The selected module number parameter is wrong */
	   /*************************************************/

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return (i_ReturnValue);
}

/*
+----------------------------------------------------------------------------+
| Function Name     : _INT_ i_APCI1710_GetUASStatus                          |
|                               (BYTE_   b_BoardHandle,                      |
|                                BYTE_   b_ModulNbr,                         |
|                                PBYTE_ pb_UASStatus)                        |
+----------------------------------------------------------------------------+
| Task              : Return the error signal (UAS) status                   |
+----------------------------------------------------------------------------+
| Input Parameters  : BYTE_ b_BoardHandle     : Handle of board APCI-1710    |
|                     BYTE_ b_ModulNbr        : Module number to configure   |
|                                               (0 to 3)                     |
+----------------------------------------------------------------------------+
| Output Parameters : PBYTE_ pb_UASStatus      : 0 : UAS is low "0"          |
|                                                1 : UAS is high "1"         |
+----------------------------------------------------------------------------+
| Return Value      :  0: No error                                           |
|                     -1: The handle parameter of the board is wrong         |
|                     -2: No counter module found                            |
|                     -3: Counter not initialised see function               |
|                         "i_APCI1710_InitCounter"                           |
+----------------------------------------------------------------------------+
*/

INT i_APCI1710_GetUASStatus(struct comedi_device * dev,
	BYTE b_ModulNbr, PBYTE pb_UASStatus)
{
	INT i_ReturnValue = 0;
	DWORD dw_StatusReg = 0;

	/**************************/
	/* Test the module number */
	/**************************/

	if (b_ModulNbr < 4) {
	   /*******************************/
		/* Test if counter initialised */
	   /*******************************/

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {
			dw_StatusReg = inl(devpriv->s_BoardInfos.
				ui_Address + 24 + (64 * b_ModulNbr));

			*pb_UASStatus = (BYTE) ((dw_StatusReg >> 1) & 1);
		} else {
	      /****************************************/
			/* Counter not initialised see function */
			/* "i_APCI1710_InitCounter"             */
	      /****************************************/

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   /*************************************************/
		/* The selected module number parameter is wrong */
	   /*************************************************/

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;

	}

	return (i_ReturnValue);
}

/*
+----------------------------------------------------------------------------+
| Function Name     : _INT_ i_APCI1710_GetCBStatus                           |
|                               (BYTE_   b_BoardHandle,                      |
|                                BYTE_   b_ModulNbr,                         |
|                                PBYTE_ pb_CBStatus)                         |
+----------------------------------------------------------------------------+
| Task              : Return the counter overflow status                     |
+----------------------------------------------------------------------------+
| Input Parameters  : BYTE_ b_BoardHandle     : Handle of board APCI-1710    |
|                     BYTE_ b_ModulNbr        : Module number to configure   |
|                                               (0 to 3)                     |
+----------------------------------------------------------------------------+
| Output Parameters : PBYTE_ pb_CBStatus      : 0 : Counter no overflow      |
|                                               1 : Counter overflow         |
+----------------------------------------------------------------------------+
| Return Value      :  0: No error                                           |
|                     -1: The handle parameter of the board is wrong         |
|                     -2: No counter module found                            |
|                     -3: Counter not initialised see function               |
|                         "i_APCI1710_InitCounter"                           |
+----------------------------------------------------------------------------+
*/

INT i_APCI1710_GetCBStatus(struct comedi_device * dev,
	BYTE b_ModulNbr, PBYTE pb_CBStatus)
{
	INT i_ReturnValue = 0;
	DWORD dw_StatusReg = 0;

	/**************************/
	/* Test the module number */
	/**************************/

	if (b_ModulNbr < 4) {
	   /*******************************/
		/* Test if counter initialised */
	   /*******************************/

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {
			dw_StatusReg = inl(devpriv->s_BoardInfos.
				ui_Address + 16 + (64 * b_ModulNbr));

			*pb_CBStatus = (BYTE) (dw_StatusReg & 1);

		} else {
	      /****************************************/
			/* Counter not initialised see function */
			/* "i_APCI1710_InitCounter"             */
	      /****************************************/

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   /*************************************************/
		/* The selected module number parameter is wrong */
	   /*************************************************/

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return (i_ReturnValue);
}

/*
+----------------------------------------------------------------------------+
| Function Name     : _INT_ i_APCI1710_Get16BitCBStatus                      |
|					(BYTE_     b_BoardHandle,            |
|					 BYTE_     b_ModulNbr,               |
|					 PBYTE_ pb_CBStatusCounter0,         |
|					 PBYTE_ pb_CBStatusCounter1)         |
+----------------------------------------------------------------------------+
| Task              : Returns the counter overflow (counter initialised to   |
|		      2*16-bit) status from selected incremental counter     |
|		      module                                                 |
+----------------------------------------------------------------------------+
| Input Parameters  : BYTE_ b_BoardHandle     : Handle of board APCI-1710    |
|                     BYTE_ b_ModulNbr        : Module number to configure   |
|                                               (0 to 3)                     |
+----------------------------------------------------------------------------+
| Output Parameters : PBYTE_ pb_CBStatusCounter0 : 0 : No overflow occur for |
|						       the first 16-bit      |
|						       counter               |
|						   1 : Overflow occur for the|
|						       first 16-bit counter  |
|		      PBYTE_ pb_CBStatusCounter1 : 0 : No overflow occur for |
|						       the second 16-bit     |
|						       counter               |
|						   1 : Overflow occur for the|
|						       second 16-bit counter |
+----------------------------------------------------------------------------+
| Return Value      :  0: No error                                           |
|                     -1: The handle parameter of the board is wrong         |
|                     -2: No counter module found                            |
|                     -3: Counter not initialised see function               |
|                         "i_APCI1710_InitCounter"                           |
|                     -4: Counter not initialised to 2*16-bit mode.          |
|			  See function "i_APCI1710_InitCounter"              |
|                     -5: Firmware revision error                            |
+----------------------------------------------------------------------------+
*/

INT i_APCI1710_Get16BitCBStatus(struct comedi_device * dev,
	BYTE b_ModulNbr, PBYTE pb_CBStatusCounter0, PBYTE pb_CBStatusCounter1)
{
	INT i_ReturnValue = 0;
	DWORD dw_StatusReg = 0;

	/**************************/
	/* Test the module number */
	/**************************/

	if (b_ModulNbr < 4) {
	   /*******************************/
		/* Test if counter initialised */
	   /*******************************/

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {
	      /*************************/
			/* Test if 2*16-Bit mode */
	      /*************************/

			if ((devpriv->s_ModuleInfo[b_ModulNbr].
					s_SiemensCounterInfo.
					s_ModeRegister.
					s_ByteModeRegister.
					b_ModeRegister1 & 0x10) == 0x10) {
		 /*****************************/
				/* Test the Firmware version */
		 /*****************************/

				if ((devpriv->s_BoardInfos.
						dw_MolduleConfiguration
						[b_ModulNbr] & 0xFFFF) >=
					0x3136) {
					dw_StatusReg =
						inl(devpriv->s_BoardInfos.
						ui_Address + 16 +
						(64 * b_ModulNbr));

					*pb_CBStatusCounter1 =
						(BYTE) ((dw_StatusReg >> 0) &
						1);
					*pb_CBStatusCounter0 =
						(BYTE) ((dw_StatusReg >> 1) &
						1);
				}	// if ((ps_APCI1710Variable->s_Board [b_BoardHandle].s_BoardInfos.dw_MolduleConfiguration [b_ModulNbr] & 0xFFFF) >= 0x3136)
				else {
		    /****************************/
					/* Firmware revision error  */
		    /****************************/

					i_ReturnValue = -5;
				}	// if ((ps_APCI1710Variable->s_Board [b_BoardHandle].s_BoardInfos.dw_MolduleConfiguration [b_ModulNbr] & 0xFFFF) >= 0x3136)
			}	// if ((ps_APCI1710Variable->s_Board [b_BoardHandle].s_ModuleInfo [b_ModulNbr].s_SiemensCounterInfo.s_ModeRegister.s_ByteModeRegister.b_ModeRegister1 & 0x10) == 0x10)
			else {
		 /********************************************/
				/* Counter not initialised to 2*16-bit mode */
				/* "i_APCI1710_InitCounter"                 */
		 /********************************************/

				DPRINTK("Counter not initialised\n");
				i_ReturnValue = -4;
			}	// if ((ps_APCI1710Variable->s_Board [b_BoardHandle].s_ModuleInfo [b_ModulNbr].s_SiemensCounterInfo.s_ModeRegister.s_ByteModeRegister.b_ModeRegister1 & 0x10) == 0x10)
		}		// if (ps_APCI1710Variable->s_Board [b_BoardHandle].s_ModuleInfo [b_ModulNbr].s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1)
		else {
	      /****************************************/
			/* Counter not initialised see function */
			/* "i_APCI1710_InitCounter"             */
	      /****************************************/

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}		// if (ps_APCI1710Variable->s_Board [b_BoardHandle].s_ModuleInfo [b_ModulNbr].s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1)
	}			// if (b_ModulNbr < 4)
	else {
	   /*************************************************/
		/* The selected module number parameter is wrong */
	   /*************************************************/

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}			// if (b_ModulNbr < 4)

	return (i_ReturnValue);
}

/*
+----------------------------------------------------------------------------+
| Function Name     : _INT_ i_APCI1710_GetUDStatus                           |
|                               (BYTE_   b_BoardHandle,                      |
|                                BYTE_   b_ModulNbr,                         |
|                                PBYTE_ pb_UDStatus)                         |
+----------------------------------------------------------------------------+
| Task              : Return the counter progress status                     |
+----------------------------------------------------------------------------+
| Input Parameters  : BYTE_ b_BoardHandle     : Handle of board APCI-1710    |
|                     BYTE_ b_ModulNbr        : Module number to configure   |
|                                               (0 to 3)                     |
+----------------------------------------------------------------------------+
| Output Parameters : PBYTE_ pb_UDStatus      : 0 : Counter progress in the  |
|                                                   selected mode down       |
|                                               1 : Counter progress in the  |
|                                                   selected mode up         |
+----------------------------------------------------------------------------+
| Return Value      :  0: No error                                           |
|                     -1: The handle parameter of the board is wrong         |
|                     -2: No counter module found                            |
|                     -3: Counter not initialised see function               |
|                         "i_APCI1710_InitCounter"                           |
+----------------------------------------------------------------------------+
*/

INT i_APCI1710_GetUDStatus(struct comedi_device * dev,
	BYTE b_ModulNbr, PBYTE pb_UDStatus)
{
	INT i_ReturnValue = 0;
	DWORD dw_StatusReg = 0;

	/**************************/
	/* Test the module number */
	/**************************/

	if (b_ModulNbr < 4) {
	   /*******************************/
		/* Test if counter initialised */
	   /*******************************/

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {
			dw_StatusReg = inl(devpriv->s_BoardInfos.
				ui_Address + 24 + (64 * b_ModulNbr));

			*pb_UDStatus = (BYTE) ((dw_StatusReg >> 2) & 1);

		} else {
	      /****************************************/
			/* Counter not initialised see function */
			/* "i_APCI1710_InitCounter"             */
	      /****************************************/

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   /*************************************************/
		/* The selected module number parameter is wrong */
	   /*************************************************/

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return (i_ReturnValue);
}

/*
+----------------------------------------------------------------------------+
| Function Name     : _INT_ i_APCI1710_GetInterruptUDLatchedStatus           |
|                               (BYTE_   b_BoardHandle,                      |
|                                BYTE_   b_ModulNbr,                         |
|                                PBYTE_ pb_UDStatus)                         |
+----------------------------------------------------------------------------+
| Task              : Return the counter progress latched status after a     |
|                     index interrupt occur.                                 |
+----------------------------------------------------------------------------+
| Input Parameters  : BYTE_ b_BoardHandle     : Handle of board APCI-1710    |
|                     BYTE_ b_ModulNbr        : Module number to configure   |
|                                               (0 to 3)                     |
+----------------------------------------------------------------------------+
| Output Parameters : PBYTE_ pb_UDStatus      : 0 : Counter progress in the  |
|                                                   selected mode down       |
|                                               1 : Counter progress in the  |
|                                                   selected mode up         |
|                                               2 : No index interrupt occur |
+----------------------------------------------------------------------------+
| Return Value      :  0: No error                                           |
|                     -1: The handle parameter of the board is wrong         |
|                     -2: No counter module found                            |
|                     -3: Counter not initialised see function               |
|                         "i_APCI1710_InitCounter"                           |
|                     -4: Interrupt function not initialised.                |
|                         See function "i_APCI1710_SetBoardIntRoutineX"      |
+----------------------------------------------------------------------------+
*/

INT i_APCI1710_GetInterruptUDLatchedStatus(struct comedi_device * dev,
	BYTE b_ModulNbr, PBYTE pb_UDStatus)
{
	INT i_ReturnValue = 0;
	DWORD dw_StatusReg = 0;

	/**************************/
	/* Test the module number */
	/**************************/

	if (b_ModulNbr < 4) {
	   /*******************************/
		/* Test if counter initialised */
	   /*******************************/

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {
		 /*********************************/
			/* Test if index interrupt occur */
		 /*********************************/

			if (devpriv->
				s_ModuleInfo[b_ModulNbr].
				s_SiemensCounterInfo.
				s_InitFlag.b_IndexInterruptOccur == 1) {
				devpriv->
					s_ModuleInfo[b_ModulNbr].
					s_SiemensCounterInfo.
					s_InitFlag.b_IndexInterruptOccur = 0;

				dw_StatusReg = inl(devpriv->s_BoardInfos.
					ui_Address + 12 + (64 * b_ModulNbr));

				*pb_UDStatus = (BYTE) ((dw_StatusReg >> 1) & 1);
			} else {
		    /****************************/
				/* No index interrupt occur */
		    /****************************/

				*pb_UDStatus = 2;
			}
		} else {
	      /****************************************/
			/* Counter not initialised see function */
			/* "i_APCI1710_InitCounter"             */
	      /****************************************/

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   /*************************************************/
		/* The selected module number parameter is wrong */
	   /*************************************************/

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return (i_ReturnValue);
}

	/*
	   +----------------------------------------------------------------------------+
	   | Function Name     : _INT_ i_APCI1710_ReadFrequencyMeasurement              |
	   |                            (BYTE_            b_BoardHandle,             |
	   |                             BYTE_            b_ModulNbr,                |
	   |                             PBYTE_          pb_Status,                  |
	   |                             PULONG_        pul_ReadValue)               |
	   +----------------------------------------------------------------------------+
	   | Task              : Returns the status (pb_Status) and the number of       |
	   |                  increments in the set time.                            |
	   |                  See function " i_APCI1710_InitFrequencyMeasurement "   |
	   +----------------------------------------------------------------------------+
	   | Input Parameters  : BYTE_  b_BoardHandle    : Handle of board APCI-1710    |
	   |                  BYTE_  b_ModulNbr       : Number of the module to be   |
	   |                                            configured (0 to 3)          |
	   +----------------------------------------------------------------------------+
	   | Output Parameters : PBYTE_ pb_Status     : Returns the frequency        |
	   |                                            measurement status           |
	   |                                            0 : Counting cycle not       |
	   |                                                started.                 |
	   |                                            1 : Counting cycle started.  |
	   |                                            2 : Counting cycle stopped.  |
	   |                                                The measurement cycle is |
	   |                                                completed.               |
	   |                  PBYTE_ pb_UDStatus      : 0 : Counter progress in the  |
	   |                                                   selected mode down       |
	   |                                               1 : Counter progress in the  |
	   |                                                   selected mode up         |
	   |                  PULONG_ pul_ReadValue   : Return the number of         |
	   |                                            increments in the defined    |
	   |                                            time base.                   |
	   +----------------------------------------------------------------------------+
	   | Return Value      :  0: No error                                           |
	   |                     -1: The handle parameter of the board is wrong         |
	   |                     -2: The selected module number is wrong                |
	   |                     -3: Counter not initialised see function               |
	   |                      "i_APCI1710_InitCounter"                           |
	   |                     -4: Frequency measurement logic not initialised.       |
	   |                      See function "i_APCI1710_InitFrequencyMeasurement" |
	   +----------------------------------------------------------------------------+
	 */

INT i_APCI1710_ReadFrequencyMeasurement(struct comedi_device * dev,
	BYTE b_ModulNbr,
	PBYTE pb_Status, PBYTE pb_UDStatus, PULONG pul_ReadValue)
{
	INT i_ReturnValue = 0;
	UINT ui_16BitValue;
	DWORD dw_StatusReg;

	/**************************/
	/* Test the module number */
	/**************************/

	if (b_ModulNbr < 4) {
	   /*******************************/
		/* Test if counter initialised */
	   /*******************************/

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {
	      /********************************************/
			/* Test if frequency mesurement initialised */
	      /********************************************/

			if (devpriv->
				s_ModuleInfo[b_ModulNbr].
				s_SiemensCounterInfo.
				s_InitFlag.b_FrequencyMeasurementInit == 1) {
		 /******************/
				/* Test if enable */
		 /******************/

				if (devpriv->
					s_ModuleInfo[b_ModulNbr].
					s_SiemensCounterInfo.
					s_InitFlag.
					b_FrequencyMeasurementEnable == 1) {
		    /*******************/
					/* Read the status */
		    /*******************/

					dw_StatusReg =
						inl(devpriv->s_BoardInfos.
						ui_Address + 32 +
						(64 * b_ModulNbr));

		    /**************************/
					/* Test if frequency stop */
		    /**************************/

					if (dw_StatusReg & 1) {
						*pb_Status = 2;
						*pb_UDStatus =
							(BYTE) ((dw_StatusReg >>
								1) & 3);

		       /******************/
						/* Read the value */
		       /******************/

						*pul_ReadValue =
							inl(devpriv->
							s_BoardInfos.
							ui_Address + 28 +
							(64 * b_ModulNbr));

						if (*pb_UDStatus == 0) {
			  /*************************/
							/* Test the counter mode */
			  /*************************/

							if ((devpriv->s_ModuleInfo[b_ModulNbr].s_SiemensCounterInfo.s_ModeRegister.s_ByteModeRegister.b_ModeRegister1 & APCI1710_16BIT_COUNTER) == APCI1710_16BIT_COUNTER) {
			     /****************************************/
								/* Test if 16-bit counter 1 pulse occur */
			     /****************************************/

								if ((*pul_ReadValue & 0xFFFFU) != 0) {
									ui_16BitValue
										=
										(UINT)
										*
										pul_ReadValue
										&
										0xFFFFU;
									*pul_ReadValue
										=
										(*pul_ReadValue
										&
										0xFFFF0000UL)
										|
										(0xFFFFU
										-
										ui_16BitValue);
								}

			     /****************************************/
								/* Test if 16-bit counter 2 pulse occur */
			     /****************************************/

								if ((*pul_ReadValue & 0xFFFF0000UL) != 0) {
									ui_16BitValue
										=
										(UINT)
										(
										(*pul_ReadValue
											>>
											16)
										&
										0xFFFFU);
									*pul_ReadValue
										=
										(*pul_ReadValue
										&
										0xFFFFUL)
										|
										(
										(0xFFFFU - ui_16BitValue) << 16);
								}
							} else {
								if (*pul_ReadValue != 0) {
									*pul_ReadValue
										=
										0xFFFFFFFFUL
										-
										*pul_ReadValue;
								}
							}
						} else {
							if (*pb_UDStatus == 1) {
			     /****************************************/
								/* Test if 16-bit counter 2 pulse occur */
			     /****************************************/

								if ((*pul_ReadValue & 0xFFFF0000UL) != 0) {
									ui_16BitValue
										=
										(UINT)
										(
										(*pul_ReadValue
											>>
											16)
										&
										0xFFFFU);
									*pul_ReadValue
										=
										(*pul_ReadValue
										&
										0xFFFFUL)
										|
										(
										(0xFFFFU - ui_16BitValue) << 16);
								}
							} else {
								if (*pb_UDStatus
									== 2) {
				/****************************************/
									/* Test if 16-bit counter 1 pulse occur */
				/****************************************/

									if ((*pul_ReadValue & 0xFFFFU) != 0) {
										ui_16BitValue
											=
											(UINT)
											*
											pul_ReadValue
											&
											0xFFFFU;
										*pul_ReadValue
											=
											(*pul_ReadValue
											&
											0xFFFF0000UL)
											|
											(0xFFFFU
											-
											ui_16BitValue);
									}
								}
							}
						}
					} else {
						*pb_Status = 1;
						*pb_UDStatus = 0;
					}
				} else {
					*pb_Status = 0;
					*pb_UDStatus = 0;
				}
			} else {
		 /***********************************************/
				/* Frequency measurement logic not initialised */
		 /***********************************************/

				DPRINTK("Frequency measurement logic not initialised\n");
				i_ReturnValue = -4;
			}
		} else {
	      /****************************************/
			/* Counter not initialised see function */
			/* "i_APCI1710_InitCounter"             */
	      /****************************************/

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   /*************************************************/
		/* The selected module number parameter is wrong */
	   /*************************************************/

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return (i_ReturnValue);
}

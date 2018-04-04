/*
 *  Copyright (C) 2004,2005  ADDI-DATA GmbH for the source code of this module.
 *
 *	ADDI-DATA GmbH
 *	Dieselstrasse 3
 *	D-77833 Ottersweier
 *	Tel: +19(0)7223/9493-0
 *	Fax: +49(0)7223/9493-92
 *	http://www.addi-data-com
 *	info@addi-data.com
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/timex.h>
#include <linux/timer.h>
#include <linux/pci.h>
#include <linux/io.h>
#include <linux/kmod.h>
#include <linux/uaccess.h>
#include "../../comedidev.h"
#include "addi_amcc_s5933.h"

#define ERROR	-1
#define SUCCESS	1

/* variable type definition */
typedef unsigned char BYTE, *PBYTE;
typedef short SHORT, *PSHORT;
typedef unsigned short USHORT, *PUSHORT;
typedef unsigned short WORD, *PWORD;
typedef int INT, *PINT;;
typedef unsigned int UINT, *PUINT;
typedef int LONG, *PLONG;		/* 32-bit */
typedef unsigned int ULONG, *PULONG;	/* 32-bit */
typedef unsigned int DWORD, *PDWORD;	/* 32-bit */
typedef unsigned long ULONG_PTR;

typedef const struct comedi_lrange *PCRANGE;

#define LOBYTE(W)	(BYTE)((W) & 0xFF)
#define HIBYTE(W)	(BYTE)(((W) >> 8) & 0xFF)
#define MAKEWORD(H, L)	(USHORT)((L) | ((H) << 8))
#define LOWORD(W)	(USHORT)((W) & 0xFFFF)
#define HIWORD(W)	(USHORT)(((W) >> 16) & 0xFFFF)
#define MAKEDWORD(H, L)	(UINT)((L) | ((H) << 16))

#define ADDI_ENABLE		1
#define ADDI_DISABLE		0
#define APCI1710_SAVE_INTERRUPT	1

#define ADDIDATA_EEPROM		1
#define ADDIDATA_NO_EEPROM	0
#define ADDIDATA_93C76		"93C76"
#define ADDIDATA_S5920		"S5920"
#define ADDIDATA_S5933		"S5933"
#define ADDIDATA_9054		"9054"

/* ADDIDATA Enable Disable */
#define ADDIDATA_ENABLE		1
#define ADDIDATA_DISABLE	0

/* Structures */

/* structure for the boardtype */
typedef struct {
	const char *pc_DriverName;	// driver name
	INT i_VendorId;		//PCI vendor a device ID of card
	INT i_DeviceId;
	INT i_IorangeBase0;
	INT i_IorangeBase1;
	INT i_IorangeBase2;	//  base 2 range
	INT i_IorangeBase3;	//  base 3 range
	INT i_PCIEeprom;	// eeprom present or not
	char *pc_EepromChip;	// type of chip
	INT i_NbrAiChannel;	// num of A/D chans
	INT i_NbrAiChannelDiff;	// num of A/D chans in diff mode
	INT i_AiChannelList;	// len of chanlist
	INT i_NbrAoChannel;	// num of D/A chans
	INT i_AiMaxdata;	// resolution of A/D
	INT i_AoMaxdata;	// resolution of D/A
	PCRANGE pr_AiRangelist;	// rangelist for A/D
	PCRANGE pr_AoRangelist;	// rangelist for D/A

	INT i_NbrDiChannel;	// Number of DI channels
	INT i_NbrDoChannel;	// Number of DO channels
	INT i_DoMaxdata;	// data to set all chanels high

	INT i_NbrTTLChannel;	// Number of TTL channels
	PCRANGE pr_TTLRangelist;	// rangelist for TTL

	INT i_Dma;		// dma present or not
	INT i_Timer;		//   timer subdevice present or not
	BYTE b_AvailableConvertUnit;
	UINT ui_MinAcquisitiontimeNs;	// Minimum Acquisition in Nano secs
	UINT ui_MinDelaytimeNs;	// Minimum Delay in Nano secs

	/* interrupt and reset */
	void (*v_hwdrv_Interrupt)(int irq, void *d);
	int (*i_hwdrv_Reset)(struct comedi_device *dev);

	/* Subdevice functions */

	/* ANALOG INPUT */
	int (*i_hwdrv_InsnConfigAnalogInput)(struct comedi_device *dev,
					     struct comedi_subdevice *s,
					     struct comedi_insn *insn,
					     unsigned int *data);
	int (*i_hwdrv_InsnReadAnalogInput)(struct comedi_device *dev,
					    struct comedi_subdevice *s,
					    struct comedi_insn *insn,
					    unsigned int *data);
	int (*i_hwdrv_InsnWriteAnalogInput)(struct comedi_device *dev,
					    struct comedi_subdevice *s,
					    struct comedi_insn *insn,
					    unsigned int *data);
	int (*i_hwdrv_InsnBitsAnalogInput)(struct comedi_device *dev,
					   struct comedi_subdevice *s,
					   struct comedi_insn *insn,
					   unsigned int *data);
	int (*i_hwdrv_CommandTestAnalogInput)(struct comedi_device *dev,
					      struct comedi_subdevice *s,
					      struct comedi_cmd *cmd);
	int (*i_hwdrv_CommandAnalogInput)(struct comedi_device *dev,
					  struct comedi_subdevice *s);
	int (*i_hwdrv_CancelAnalogInput)(struct comedi_device *dev,
					 struct comedi_subdevice *s);

	/* Analog Output */
	int (*i_hwdrv_InsnConfigAnalogOutput)(struct comedi_device *dev,
					      struct comedi_subdevice *s,
					      struct comedi_insn *insn,
					      unsigned int *data);
	int (*i_hwdrv_InsnWriteAnalogOutput)(struct comedi_device *dev,
					     struct comedi_subdevice *s,
					     struct comedi_insn *insn,
					     unsigned int *data);
	int (*i_hwdrv_InsnBitsAnalogOutput)(struct comedi_device *dev,
					    struct comedi_subdevice *s,
					    struct comedi_insn *insn,
					    unsigned int *data);

	/* Digital Input */
	int (*i_hwdrv_InsnConfigDigitalInput) (struct comedi_device *dev,
					       struct comedi_subdevice *s,
					       struct comedi_insn *insn,
					       unsigned int *data);
	int (*i_hwdrv_InsnReadDigitalInput) (struct comedi_device *dev,
					     struct comedi_subdevice *s,
					     struct comedi_insn *insn,
					     unsigned int *data);
	int (*i_hwdrv_InsnWriteDigitalInput) (struct comedi_device *dev,
					      struct comedi_subdevice *s,
					      struct comedi_insn *insn,
					      unsigned int *data);
	int (*i_hwdrv_InsnBitsDigitalInput) (struct comedi_device *dev,
					     struct comedi_subdevice *s,
					     struct comedi_insn *insn,
					     unsigned int *data);

	/* Digital Output */
	int (*i_hwdrv_InsnConfigDigitalOutput)(struct comedi_device *dev,
					       struct comedi_subdevice *s,
					       struct comedi_insn *insn,
					       unsigned int *data);
	int (*i_hwdrv_InsnWriteDigitalOutput)(struct comedi_device *dev,
					      struct comedi_subdevice *s,
					      struct comedi_insn *insn,
					      unsigned int *data);
	int (*i_hwdrv_InsnBitsDigitalOutput)(struct comedi_device *dev,
					     struct comedi_subdevice *s,
					     struct comedi_insn *insn,
					     unsigned int *data);
	int (*i_hwdrv_InsnReadDigitalOutput)(struct comedi_device *dev,
					     struct comedi_subdevice *s,
					     struct comedi_insn *insn,
					     unsigned int *data);

	/* TIMER */
	int (*i_hwdrv_InsnConfigTimer)(struct comedi_device *dev,
				       struct comedi_subdevice *s,
				       struct comedi_insn *insn, unsigned int *data);
	int (*i_hwdrv_InsnWriteTimer)(struct comedi_device *dev,
				      struct comedi_subdevice *s, struct comedi_insn *insn,
				      unsigned int *data);
	int (*i_hwdrv_InsnReadTimer)(struct comedi_device *dev, struct comedi_subdevice *s,
				     struct comedi_insn *insn, unsigned int *data);
	int (*i_hwdrv_InsnBitsTimer)(struct comedi_device *dev, struct comedi_subdevice *s,
				     struct comedi_insn *insn, unsigned int *data);

	/* TTL IO */
	int (*i_hwdr_ConfigInitTTLIO)(struct comedi_device *dev,
				      struct comedi_subdevice *s, struct comedi_insn *insn,
				      unsigned int *data);
	int (*i_hwdr_ReadTTLIOBits)(struct comedi_device *dev, struct comedi_subdevice *s,
				    struct comedi_insn *insn, unsigned int *data);
	int (*i_hwdr_ReadTTLIOAllPortValue)(struct comedi_device *dev,
					    struct comedi_subdevice *s,
					    struct comedi_insn *insn,
					    unsigned int *data);
	int (*i_hwdr_WriteTTLIOChlOnOff)(struct comedi_device *dev,
					 struct comedi_subdevice *s,
					 struct comedi_insn *insn, unsigned int *data);
} boardtype;

//MODULE INFO STRUCTURE

typedef union {
	/* Incremental counter infos */
	struct {
		union {
			struct {
				BYTE b_ModeRegister1;
				BYTE b_ModeRegister2;
				BYTE b_ModeRegister3;
				BYTE b_ModeRegister4;
			} s_ByteModeRegister;
			DWORD dw_ModeRegister1_2_3_4;
		} s_ModeRegister;

		struct {
			unsigned int b_IndexInit:1;
			unsigned int b_CounterInit:1;
			unsigned int b_ReferenceInit:1;
			unsigned int b_IndexInterruptOccur:1;
			unsigned int b_CompareLogicInit:1;
			unsigned int b_FrequencyMeasurementInit:1;
			unsigned int b_FrequencyMeasurementEnable:1;
		} s_InitFlag;

	} s_SiemensCounterInfo;

	/* SSI infos */
	struct {
		BYTE b_SSIProfile;
		BYTE b_PositionTurnLength;
		BYTE b_TurnCptLength;
		BYTE b_SSIInit;
	} s_SSICounterInfo;

	/* TTL I/O infos */
	struct {
		BYTE b_TTLInit;
		BYTE b_PortConfiguration[4];
	} s_TTLIOInfo;

	/* Digital I/O infos */
	struct {
		BYTE b_DigitalInit;
		BYTE b_ChannelAMode;
		BYTE b_ChannelBMode;
		BYTE b_OutputMemoryEnabled;
		DWORD dw_OutputMemory;
	} s_DigitalIOInfo;

      /*********************/
	/* 82X54 timer infos */
      /*********************/

	struct {
		struct {
			BYTE b_82X54Init;
			BYTE b_InputClockSelection;
			BYTE b_InputClockLevel;
			BYTE b_OutputLevel;
			BYTE b_HardwareGateLevel;
			DWORD dw_ConfigurationWord;
		} s_82X54TimerInfo[3];
		BYTE b_InterruptMask;
	} s_82X54ModuleInfo;

      /*********************/
	/* Chronometer infos */
      /*********************/

	struct {
		BYTE b_ChronoInit;
		BYTE b_InterruptMask;
		BYTE b_PCIInputClock;
		BYTE b_TimingUnit;
		BYTE b_CycleMode;
		double d_TimingInterval;
		DWORD dw_ConfigReg;
	} s_ChronoModuleInfo;

      /***********************/
	/* Pulse encoder infos */
      /***********************/

	struct {
		struct {
			BYTE b_PulseEncoderInit;
		} s_PulseEncoderInfo[4];
		DWORD dw_SetRegister;
		DWORD dw_ControlRegister;
		DWORD dw_StatusRegister;
	} s_PulseEncoderModuleInfo;

	/* Tor conter infos */
	struct {
		struct {
			BYTE b_TorCounterInit;
			BYTE b_TimingUnit;
			BYTE b_InterruptEnable;
			double d_TimingInterval;
			ULONG ul_RealTimingInterval;
		} s_TorCounterInfo[2];
		BYTE b_PCIInputClock;
	} s_TorCounterModuleInfo;

	/* PWM infos */
	struct {
		struct {
			BYTE b_PWMInit;
			BYTE b_TimingUnit;
			BYTE b_InterruptEnable;
			double d_LowTiming;
			double d_HighTiming;
			ULONG ul_RealLowTiming;
			ULONG ul_RealHighTiming;
		} s_PWMInfo[2];
		BYTE b_ClockSelection;
	} s_PWMModuleInfo;

	/* ETM infos */
	struct {
		struct {
			BYTE b_ETMEnable;
			BYTE b_ETMInterrupt;
		} s_ETMInfo[2];
		BYTE b_ETMInit;
		BYTE b_TimingUnit;
		BYTE b_ClockSelection;
		double d_TimingInterval;
		ULONG ul_Timing;
	} s_ETMModuleInfo;

	/* CDA infos */
	struct {
		BYTE b_CDAEnable;
		BYTE b_CDAInterrupt;
		BYTE b_CDAInit;
		BYTE b_FctSelection;
		BYTE b_CDAReadFIFOOverflow;
	} s_CDAModuleInfo;

} str_ModuleInfo;

/* Private structure for the addi_apci3120 driver */
typedef struct {

	INT iobase;
	INT i_IobaseAmcc;	// base+size for AMCC chip
	INT i_IobaseAddon;	//addon base address
	INT i_IobaseReserved;
	ULONG_PTR dw_AiBase;
	struct pcilst_struct *amcc;	// ptr too AMCC data
	BYTE allocated;		// we have blocked card
	BYTE b_ValidDriver;	// driver is ok
	BYTE b_AiContinuous;	// we do unlimited AI
	BYTE b_AiInitialisation;
	UINT ui_AiActualScan;	//how many scans we finished
	UINT ui_AiBufferPtr;	// data buffer ptr in samples
	UINT ui_AiNbrofChannels;	// how many channels is measured
	UINT ui_AiScanLength;	// Length of actual scanlist
	UINT ui_AiActualScanPosition;	// position in actual scan
	PUINT pui_AiChannelList;	// actual chanlist
	UINT ui_AiChannelList[32];	// actual chanlist
	BYTE b_AiChannelConfiguration[32];	// actual chanlist
	UINT ui_AiReadData[32];
	DWORD dw_AiInitialised;
	UINT ui_AiTimer0;	//Timer Constant for Timer0
	UINT ui_AiTimer1;	//Timer constant for Timer1
	UINT ui_AiFlags;
	UINT ui_AiDataLength;
	short *AiData;	// Pointer to sample data
	UINT ui_AiNbrofScans;	// number of scans to do
	USHORT us_UseDma;	// To use Dma or not
	BYTE b_DmaDoubleBuffer;	// we can use double buffering
	UINT ui_DmaActualBuffer;	// which buffer is used now
	//*UPDATE-0.7.57->0.7.68
	//ULONG               ul_DmaBufferVirtual[2];// pointers to begin of DMA buffer
	short *ul_DmaBufferVirtual[2];	// pointers to begin of DMA buffer
	ULONG ul_DmaBufferHw[2];	// hw address of DMA buff
	UINT ui_DmaBufferSize[2];	// size of dma buffer in bytes
	UINT ui_DmaBufferUsesize[2];	// which size we may now used for transfer
	UINT ui_DmaBufferSamples[2];	// size in samples
	UINT ui_DmaBufferPages[2];	// number of pages in buffer
	BYTE b_DigitalOutputRegister;	// Digital Output Register
	BYTE b_OutputMemoryStatus;
	BYTE b_AnalogInputChannelNbr;	// Analog input channel Nbr
	BYTE b_AnalogOutputChannelNbr;	// Analog input Output  Nbr
	BYTE b_TimerSelectMode;	// Contain data written at iobase + 0C
	BYTE b_ModeSelectRegister;	// Contain data written at iobase + 0E
	USHORT us_OutputRegister;	// Contain data written at iobase + 0
	BYTE b_InterruptState;
	BYTE b_TimerInit;	// Specify if InitTimerWatchdog was load
	BYTE b_TimerStarted;	// Specify if timer 2 is running or not
	BYTE b_Timer2Mode;	// Specify the timer 2 mode
	BYTE b_Timer2Interrupt;	//Timer2  interrupt enable or disable
	BYTE b_AiCyclicAcquisition;	// indicate cyclic acquisition
	BYTE b_InterruptMode;	// eoc eos or dma
	BYTE b_EocEosInterrupt;	// Enable disable eoc eos interrupt
	UINT ui_EocEosConversionTime;
	BYTE b_EocEosConversionTimeBase;
	BYTE b_SingelDiff;
	BYTE b_ExttrigEnable;	/* To enable or disable external trigger */

	/* Pointer to the current process */
	struct task_struct *tsk_Current;
	boardtype *ps_BoardInfo;

	/* Hardware board infos for 1710 */
	struct {
		UINT ui_Address;	/* Board address */
		UINT ui_FlashAddress;
		BYTE b_InterruptNbr;	/* Board interrupt number */
		BYTE b_SlotNumber;	/* PCI slot number */
		BYTE b_BoardVersion;
		DWORD dw_MolduleConfiguration[4];	/* Module config */
	} s_BoardInfos;

	/* Interrupt infos */
	struct {
		ULONG ul_InterruptOccur;	/* 0   : No interrupt occur */
						/* > 0 : Interrupt occur */
		UINT ui_Read;	/* Read FIFO */
		UINT ui_Write;	/* Write FIFO */
		struct {
			BYTE b_OldModuleMask;
			ULONG ul_OldInterruptMask;	/* Interrupt mask */
			ULONG ul_OldCounterLatchValue;	/* Interrupt counter value */
		} s_FIFOInterruptParameters[APCI1710_SAVE_INTERRUPT];
	} s_InterruptParameters;

	str_ModuleInfo s_ModuleInfo[4];
	ULONG ul_TTLPortConfiguration[10];

} addi_private;

static unsigned short pci_list_builded;	/* set to 1 when list of card is known */

/* Function declarations */
static int i_ADDI_Attach(struct comedi_device *dev, struct comedi_devconfig *it);
static int i_ADDI_Detach(struct comedi_device *dev);
static int i_ADDI_Reset(struct comedi_device *dev);

static irqreturn_t v_ADDI_Interrupt(int irq, void *d PT_REGS_ARG);
static int i_ADDIDATA_InsnReadEeprom(struct comedi_device *dev, struct comedi_subdevice *s,
				     struct comedi_insn *insn, unsigned int *data);

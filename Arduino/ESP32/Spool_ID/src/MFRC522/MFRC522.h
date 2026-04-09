/**
 * Library to use Arduino MFRC522 module.
 *
 * @authors Dr.Leong, Miguel Balboa, Søren Thing Andersen, Tom Clement, many more! See GitLog.
 *
 * For more information read the README.
 *
 * Please read this file for an overview and then MFRC522.cpp for comments on the specific functions.
 */
#ifndef MFRC522_h
#define MFRC522_h

#include "require_cpp11.h"
#include "deprecated.h"
// Enable integer limits
#define __STDC_LIMIT_MACROS
#include <stdint.h>
#include <Arduino.h>
#include <SPI.h>

#ifndef MFRC522_SPICLOCK
#define MFRC522_SPICLOCK (4000000u)	// MFRC522 accept upto 10MHz, set to 4MHz.
#endif

// Firmware data for self-test
// Reference values based on firmware version
// Hint: if needed, you can remove unused self-test data to save flash memory
//
// Version 0.0 (0x90)
// Philips Semiconductors; Preliminary Specification Revision 2.0 - 01 August 2005; 16.1 self-test
const byte MFRC522_firmware_referenceV0_0[] PROGMEM = {
	0x00, 0x87, 0x98, 0x0f, 0x49, 0xFF, 0x07, 0x19,
	0xBF, 0x22, 0x30, 0x49, 0x59, 0x63, 0xAD, 0xCA,
	0x7F, 0xE3, 0x4E, 0x03, 0x5C, 0x4E, 0x49, 0x50,
	0x47, 0x9A, 0x37, 0x61, 0xE7, 0xE2, 0xC6, 0x2E,
	0x75, 0x5A, 0xED, 0x04, 0x3D, 0x02, 0x4B, 0x78,
	0x32, 0xFF, 0x58, 0x3B, 0x7C, 0xE9, 0x00, 0x94,
	0xB4, 0x4A, 0x59, 0x5B, 0xFD, 0xC9, 0x29, 0xDF,
	0x35, 0x96, 0x98, 0x9E, 0x4F, 0x30, 0x32, 0x8D
};
// Version 1.0 (0x91)
// NXP Semiconductors; Rev. 3.8 - 17 September 2014; 16.1.1 self-test
const byte MFRC522_firmware_referenceV1_0[] PROGMEM = {
	0x00, 0xC6, 0x37, 0xD5, 0x32, 0xB7, 0x57, 0x5C,
	0xC2, 0xD8, 0x7C, 0x4D, 0xD9, 0x70, 0xC7, 0x73,
	0x10, 0xE6, 0xD2, 0xAA, 0x5E, 0xA1, 0x3E, 0x5A,
	0x14, 0xAF, 0x30, 0x61, 0xC9, 0x70, 0xDB, 0x2E,
	0x64, 0x22, 0x72, 0xB5, 0xBD, 0x65, 0xF4, 0xEC,
	0x22, 0xBC, 0xD3, 0x72, 0x35, 0xCD, 0xAA, 0x41,
	0x1F, 0xA7, 0xF3, 0x53, 0x14, 0xDE, 0x7E, 0x02,
	0xD9, 0x0F, 0xB5, 0x5E, 0x25, 0x1D, 0x29, 0x79
};
// Version 2.0 (0x92)
// NXP Semiconductors; Rev. 3.8 - 17 September 2014; 16.1.1 self-test
const byte MFRC522_firmware_referenceV2_0[] PROGMEM = {
	0x00, 0xEB, 0x66, 0xBA, 0x57, 0xBF, 0x23, 0x95,
	0xD0, 0xE3, 0x0D, 0x3D, 0x27, 0x89, 0x5C, 0xDE,
	0x9D, 0x3B, 0xA7, 0x00, 0x21, 0x5B, 0x89, 0x82,
	0x51, 0x3A, 0xEB, 0x02, 0x0C, 0xA5, 0x00, 0x49,
	0x7C, 0x84, 0x4D, 0xB3, 0xCC, 0xD2, 0x1B, 0x81,
	0x5D, 0x48, 0x76, 0xD5, 0x71, 0x61, 0x21, 0xA9,
	0x86, 0x96, 0x83, 0x38, 0xCF, 0x9D, 0x5B, 0x6D,
	0xDC, 0x15, 0xBA, 0x3E, 0x7D, 0x95, 0x3B, 0x2F
};
// Clone
// Fudan Semiconductor FM17522 (0x88)
const byte FM17522_firmware_reference[] PROGMEM = {
	0x00, 0xD6, 0x78, 0x8C, 0xE2, 0xAA, 0x0C, 0x18,
	0x2A, 0xB8, 0x7A, 0x7F, 0xD3, 0x6A, 0xCF, 0x0B,
	0xB1, 0x37, 0x63, 0x4B, 0x69, 0xAE, 0x91, 0xC7,
	0xC3, 0x97, 0xAE, 0x77, 0xF4, 0x37, 0xD7, 0x9B,
	0x7C, 0xF5, 0x3C, 0x11, 0x8F, 0x15, 0xC3, 0xD7,
	0xC1, 0x5B, 0x00, 0x2A, 0xD0, 0x75, 0xDE, 0x9E,
	0x51, 0x64, 0xAB, 0x3E, 0xE9, 0x15, 0xB5, 0xAB,
	0x56, 0x9A, 0x98, 0x82, 0x26, 0xEA, 0x2A, 0x62
};

class MFRC522 {
public:
	// Size of the MFRC522 FIFO
	static constexpr byte FIFO_SIZE = 64;		// The FIFO is 64 bytes.
	// Default value for unused pin
	static constexpr uint8_t UNUSED_PIN = UINT8_MAX;

	// MFRC522 registers. Described in chapter 9 of the datasheet.
	// When using SPI all addresses are shifted one bit left in the "SPI address byte" (section 8.1.2.3)
	enum PCD_Register : byte {
		// Page 0: Command and status
		CommandReg				= 0x01 << 1,
		ComIEnReg				= 0x02 << 1,
		DivIEnReg				= 0x03 << 1,
		ComIrqReg				= 0x04 << 1,
		DivIrqReg				= 0x05 << 1,
		ErrorReg				= 0x06 << 1,
		Status1Reg				= 0x07 << 1,
		Status2Reg				= 0x08 << 1,
		FIFODataReg				= 0x09 << 1,
		FIFOLevelReg			= 0x0A << 1,
		WaterLevelReg			= 0x0B << 1,
		ControlReg				= 0x0C << 1,
		BitFramingReg			= 0x0D << 1,
		CollReg					= 0x0E << 1,
		// Page 1: Command
		ModeReg					= 0x11 << 1,
		TxModeReg				= 0x12 << 1,
		RxModeReg				= 0x13 << 1,
		TxControlReg			= 0x14 << 1,
		TxASKReg				= 0x15 << 1,
		TxSelReg				= 0x16 << 1,
		RxSelReg				= 0x17 << 1,
		RxThresholdReg			= 0x18 << 1,
		DemodReg				= 0x19 << 1,
		MfTxReg					= 0x1C << 1,
		MfRxReg					= 0x1D << 1,
		SerialSpeedReg			= 0x1F << 1,
		// Page 2: Configuration
		CRCResultRegH			= 0x21 << 1,
		CRCResultRegL			= 0x22 << 1,
		ModWidthReg				= 0x24 << 1,
		RFCfgReg				= 0x26 << 1,
		GsNReg					= 0x27 << 1,
		CWGsPReg				= 0x28 << 1,
		ModGsPReg				= 0x29 << 1,
		TModeReg				= 0x2A << 1,
		TPrescalerReg			= 0x2B << 1,
		TReloadRegH				= 0x2C << 1,
		TReloadRegL				= 0x2D << 1,
		TCounterValueRegH		= 0x2E << 1,
		TCounterValueRegL		= 0x2F << 1,
		// Page 3: Test Registers
		TestSel1Reg				= 0x31 << 1,
		TestSel2Reg				= 0x32 << 1,
		TestPinEnReg			= 0x33 << 1,
		TestPinValueReg			= 0x34 << 1,
		TestBusReg				= 0x35 << 1,
		AutoTestReg				= 0x36 << 1,
		VersionReg				= 0x37 << 1,
		AnalogTestReg			= 0x38 << 1,
		TestDAC1Reg				= 0x39 << 1,
		TestDAC2Reg				= 0x3A << 1,
		TestADCReg				= 0x3B << 1
	};

	// MFRC522 commands. Described in chapter 10 of the datasheet.
	enum PCD_Command : byte {
		PCD_Idle				= 0x00,
		PCD_Mem					= 0x01,
		PCD_GenerateRandomID	= 0x02,
		PCD_CalcCRC				= 0x03,
		PCD_Transmit			= 0x04,
		PCD_NoCmdChange			= 0x07,
		PCD_Receive				= 0x08,
		PCD_Transceive 			= 0x0C,
		PCD_MFAuthent 			= 0x0E,
		PCD_SoftReset			= 0x0F
	};

	// MFRC522 RxGain[2:0] masks
	enum PCD_RxGain : byte {
		RxGain_18dB				= 0x00 << 4,
		RxGain_23dB				= 0x01 << 4,
		RxGain_18dB_2			= 0x02 << 4,
		RxGain_23dB_2			= 0x03 << 4,
		RxGain_33dB				= 0x04 << 4,
		RxGain_38dB				= 0x05 << 4,
		RxGain_43dB				= 0x06 << 4,
		RxGain_48dB				= 0x07 << 4,
		RxGain_min				= 0x00 << 4,
		RxGain_avg				= 0x04 << 4,
		RxGain_max				= 0x07 << 4
	};

	// Commands sent to the PICC.
	enum PICC_Command : byte {
		PICC_CMD_REQA			= 0x26,
		PICC_CMD_WUPA			= 0x52,
		PICC_CMD_CT				= 0x88,
		PICC_CMD_SEL_CL1		= 0x93,
		PICC_CMD_SEL_CL2		= 0x95,
		PICC_CMD_SEL_CL3		= 0x97,
		PICC_CMD_HLTA			= 0x50,
		PICC_CMD_RATS           = 0xE0,
		PICC_CMD_MF_AUTH_KEY_A	= 0x60,
		PICC_CMD_MF_AUTH_KEY_B	= 0x61,
		PICC_CMD_MF_READ		= 0x30,
		PICC_CMD_MF_WRITE		= 0xA0,
		PICC_CMD_MF_DECREMENT	= 0xC0,
		PICC_CMD_MF_INCREMENT	= 0xC1,
		PICC_CMD_MF_RESTORE		= 0xC2,
		PICC_CMD_MF_TRANSFER	= 0xB0,
		PICC_CMD_UL_WRITE		= 0xA2
	};

	// MIFARE constants
	enum MIFARE_Misc {
		MF_ACK					= 0xA,
		MF_KEY_SIZE				= 6
	};

	// PICC types we can detect.
	enum PICC_Type : byte {
		PICC_TYPE_UNKNOWN		,
		PICC_TYPE_ISO_14443_4	,
		PICC_TYPE_ISO_18092		,
		PICC_TYPE_MIFARE_MINI	,
		PICC_TYPE_MIFARE_1K		,
		PICC_TYPE_MIFARE_4K		,
		PICC_TYPE_MIFARE_UL		,
		PICC_TYPE_MIFARE_PLUS	,
		PICC_TYPE_MIFARE_DESFIRE,
		PICC_TYPE_TNP3XXX		,
		PICC_TYPE_NOT_COMPLETE	= 0xff
	};

	// Return codes from the functions in this class.
	enum StatusCode : byte {
		STATUS_OK				,
		STATUS_ERROR			,
		STATUS_COLLISION		,
		STATUS_TIMEOUT			,
		STATUS_NO_ROOM			,
		STATUS_INTERNAL_ERROR	,
		STATUS_INVALID			,
		STATUS_CRC_WRONG		,
		STATUS_MIFARE_NACK		= 0xff
	};

	// A struct used for passing the UID of a PICC.
	typedef struct {
		byte		size;
		byte		uidByte[10];
		byte		sak;
	} Uid;

	// A struct used for passing a MIFARE Crypto1 key
	typedef struct {
		byte		keyByte[MF_KEY_SIZE];
	} MIFARE_Key;

	// Member variables
	Uid uid;

	// Functions for setting up the Arduino
	MFRC522();
	MFRC522(byte resetPowerDownPin);
	MFRC522(byte chipSelectPin, byte resetPowerDownPin);

	// Basic interface functions for communicating with the MFRC522
	void PCD_WriteRegister(PCD_Register reg, byte value);
	void PCD_WriteRegister(PCD_Register reg, byte count, byte *values);
	byte PCD_ReadRegister(PCD_Register reg);
	void PCD_ReadRegister(PCD_Register reg, byte count, byte *values, byte rxAlign = 0);
	void PCD_SetRegisterBitMask(PCD_Register reg, byte mask);
	void PCD_ClearRegisterBitMask(PCD_Register reg, byte mask);
	StatusCode PCD_CalculateCRC(byte *data, byte length, byte *result);

	// Functions for manipulating the MFRC522
	void PCD_Init();
	void PCD_Init(byte resetPowerDownPin);
	void PCD_Init(byte chipSelectPin, byte resetPowerDownPin);
	void PCD_Reset();
	void PCD_AntennaOn();
	void PCD_AntennaOff();
	byte PCD_GetAntennaGain();
	void PCD_SetAntennaGain(byte mask);
	bool PCD_PerformSelfTest();

	// Power control functions
	void PCD_SoftPowerDown();
	void PCD_SoftPowerUp();

	// Functions for communicating with PICCs
	StatusCode PCD_TransceiveData(byte *sendData, byte sendLen, byte *backData, byte *backLen, byte *validBits = nullptr, byte rxAlign = 0, bool checkCRC = false);
	StatusCode PCD_CommunicateWithPICC(byte command, byte waitIRq, byte *sendData, byte sendLen, byte *backData = nullptr, byte *backLen = nullptr, byte *validBits = nullptr, byte rxAlign = 0, bool checkCRC = false);
	StatusCode PICC_RequestA(byte *bufferATQA, byte *bufferSize);
	StatusCode PICC_WakeupA(byte *bufferATQA, byte *bufferSize);
	StatusCode PICC_REQA_or_WUPA(byte command, byte *bufferATQA, byte *bufferSize);
	virtual StatusCode PICC_Select(Uid *uid, byte validBits = 0);
	StatusCode PICC_HaltA();

	// Functions for communicating with MIFARE PICCs
	StatusCode PCD_Authenticate(byte command, byte blockAddr, MIFARE_Key *key, Uid *uid);
	void PCD_StopCrypto1();
	StatusCode MIFARE_Read(byte blockAddr, byte *buffer, byte *bufferSize);
	StatusCode MIFARE_Write(byte blockAddr, byte *buffer, byte bufferSize);
	StatusCode MIFARE_Ultralight_Write(byte page, byte *buffer, byte bufferSize);
	StatusCode MIFARE_Decrement(byte blockAddr, int32_t delta);
	StatusCode MIFARE_Increment(byte blockAddr, int32_t delta);
	StatusCode MIFARE_Restore(byte blockAddr);
	StatusCode MIFARE_Transfer(byte blockAddr);
	StatusCode MIFARE_GetValue(byte blockAddr, int32_t *value);
	StatusCode MIFARE_SetValue(byte blockAddr, int32_t value);
	StatusCode PCD_NTAG216_AUTH(byte *passWord, byte pACK[]);

	// Support functions
	StatusCode PCD_MIFARE_Transceive(byte *sendData, byte sendLen, bool acceptTimeout = false);
	static const __FlashStringHelper *GetStatusCodeName(StatusCode code);
	static PICC_Type PICC_GetType(byte sak);
	static const __FlashStringHelper *PICC_GetTypeName(PICC_Type type);

	// Support functions for debugging
	void PCD_DumpVersionToSerial();
	void PICC_DumpToSerial(Uid *uid);
	void PICC_DumpDetailsToSerial(Uid *uid);
	void PICC_DumpMifareClassicToSerial(Uid *uid, PICC_Type piccType, MIFARE_Key *key);
	void PICC_DumpMifareClassicSectorToSerial(Uid *uid, MIFARE_Key *key, byte sector);
	void PICC_DumpMifareUltralightToSerial();

	// Advanced functions for MIFARE
	void MIFARE_SetAccessBits(byte *accessBitBuffer, byte g0, byte g1, byte g2, byte g3);
	bool MIFARE_OpenUidBackdoor(bool logErrors);
	bool MIFARE_SetUid(byte *newUid, byte uidSize, bool logErrors);
	bool MIFARE_UnbrickUidSector(bool logErrors);

	// Convenience functions
	virtual bool PICC_IsNewCardPresent();
	virtual bool PICC_ReadCardSerial();

protected:
	byte _chipSelectPin;
	byte _resetPowerDownPin;
	StatusCode MIFARE_TwoStepHelper(byte command, byte blockAddr, int32_t data);
};

#endif

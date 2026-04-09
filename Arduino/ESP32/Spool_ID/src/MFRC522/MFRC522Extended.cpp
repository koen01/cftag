/*
 * Library extends MFRC522.h to support RATS for ISO-14443-4 PICC.
 * RATS - Request for Answer To Select.
 * @author JPG-Consulting
*/

#include "MFRC522Extended.h"

MFRC522::StatusCode MFRC522Extended::PICC_Select(Uid *uid, byte validBits) {
	bool uidComplete, selectDone, useCascadeTag;
	byte cascadeLevel = 1;
	MFRC522::StatusCode result;
	byte count, index, uidIndex;
	int8_t currentLevelKnownBits;
	byte buffer[9];
	byte bufferUsed, rxAlign, txLastBits;
	byte *responseBuffer;
	byte responseLength;

	if (validBits > 80) return STATUS_INVALID;
	PCD_ClearRegisterBitMask(CollReg, 0x80);

	uidComplete = false;
	while (!uidComplete) {
		switch (cascadeLevel) {
			case 1: buffer[0]=PICC_CMD_SEL_CL1; uidIndex=0; useCascadeTag=validBits&&uid->size>4; break;
			case 2: buffer[0]=PICC_CMD_SEL_CL2; uidIndex=3; useCascadeTag=validBits&&uid->size>7; break;
			case 3: buffer[0]=PICC_CMD_SEL_CL3; uidIndex=6; useCascadeTag=false; break;
			default: return STATUS_INTERNAL_ERROR;
		}
		currentLevelKnownBits = validBits - (8 * uidIndex);
		if (currentLevelKnownBits < 0) currentLevelKnownBits = 0;
		index = 2;
		if (useCascadeTag) buffer[index++] = PICC_CMD_CT;
		byte bytesToCopy = currentLevelKnownBits/8 + (currentLevelKnownBits%8 ? 1 : 0);
		if (bytesToCopy) {
			byte maxBytes = useCascadeTag ? 3 : 4;
			if (bytesToCopy > maxBytes) bytesToCopy = maxBytes;
			for (count=0; count<bytesToCopy; count++) buffer[index++] = uid->uidByte[uidIndex+count];
		}
		if (useCascadeTag) currentLevelKnownBits += 8;

		selectDone = false;
		while (!selectDone) {
			if (currentLevelKnownBits >= 32) {
				buffer[1] = 0x70;
				buffer[6] = buffer[2]^buffer[3]^buffer[4]^buffer[5];
				result = PCD_CalculateCRC(buffer, 7, &buffer[7]);
				if (result != STATUS_OK) return result;
				txLastBits = 0; bufferUsed = 9;
				responseBuffer = &buffer[6]; responseLength = 3;
			} else {
				txLastBits = currentLevelKnownBits % 8;
				count = currentLevelKnownBits / 8;
				index = 2 + count;
				buffer[1] = (index << 4) + txLastBits;
				bufferUsed = index + (txLastBits ? 1 : 0);
				responseBuffer = &buffer[index];
				responseLength = sizeof(buffer) - index;
			}
			rxAlign = txLastBits;
			PCD_WriteRegister(BitFramingReg, (rxAlign<<4)+txLastBits);
			result = PCD_TransceiveData(buffer, bufferUsed, responseBuffer, &responseLength, &txLastBits, rxAlign);
			if (result == STATUS_COLLISION) {
				byte valueOfCollReg = PCD_ReadRegister(CollReg);
				if (valueOfCollReg & 0x20) return STATUS_COLLISION;
				byte collisionPos = valueOfCollReg & 0x1F;
				if (collisionPos == 0) collisionPos = 32;
				if (collisionPos <= (byte)currentLevelKnownBits) return STATUS_INTERNAL_ERROR;
				currentLevelKnownBits = collisionPos;
				count = (currentLevelKnownBits-1) % 8;
				index = 1 + (currentLevelKnownBits/8) + (count ? 1 : 0);
				buffer[index] |= (1 << count);
			} else if (result != STATUS_OK) {
				return result;
			} else {
				if (currentLevelKnownBits >= 32) selectDone = true;
				else currentLevelKnownBits = 32;
			}
		}

		index = (buffer[2]==PICC_CMD_CT) ? 3 : 2;
		bytesToCopy = (buffer[2]==PICC_CMD_CT) ? 3 : 4;
		for (count=0; count<bytesToCopy; count++) uid->uidByte[uidIndex+count] = buffer[index++];

		if (responseLength != 3 || txLastBits != 0) return STATUS_ERROR;
		result = PCD_CalculateCRC(responseBuffer, 1, &buffer[2]);
		if (result != STATUS_OK) return result;
		if ((buffer[2]!=responseBuffer[1]) || (buffer[3]!=responseBuffer[2])) return STATUS_CRC_WRONG;
		if (responseBuffer[0] & 0x04) cascadeLevel++;
		else { uidComplete=true; uid->sak=responseBuffer[0]; }
	}

	uid->size = 3 * cascadeLevel + 1;

	if ((uid->sak & 0x24) == 0x20) {
		Ats ats;
		result = PICC_RequestATS(&ats);
		if (result == STATUS_OK && ats.size > 0 && ats.ta1.transmitted) {
			TagBitRates ds = (ats.ta1.ds & 0x01) ? BITRATE_212KBITS : BITRATE_106KBITS;
			TagBitRates dr = (ats.ta1.dr & 0x01) ? BITRATE_212KBITS : BITRATE_106KBITS;
			PICC_PPS(ds, dr);
		}
	}
	return STATUS_OK;
}

MFRC522::StatusCode MFRC522Extended::PICC_RequestATS(Ats *ats) {
	MFRC522::StatusCode result;
	byte bufferATS[FIFO_SIZE];
	byte bufferSize = FIFO_SIZE;
	memset(bufferATS, 0, FIFO_SIZE);
	bufferATS[0] = PICC_CMD_RATS;
	bufferATS[1] = 0x50;
	result = PCD_CalculateCRC(bufferATS, 2, &bufferATS[2]);
	if (result != STATUS_OK) return result;
	result = PCD_TransceiveData(bufferATS, 4, bufferATS, &bufferSize, NULL, 0, true);
	if (result != STATUS_OK) { PICC_HaltA(); }
	ats->size = bufferATS[0];
	if (ats->size > 0x01) {
		ats->ta1.transmitted = (bool)(bufferATS[1] & 0x40);
		ats->tb1.transmitted = (bool)(bufferATS[1] & 0x20);
		ats->tc1.transmitted = (bool)(bufferATS[1] & 0x10);
		uint8_t fsci = bufferATS[1] & 0x0F;
		const byte fscTable[] = {16,24,32,40,48,64,96,128};
		ats->fsc = (fsci < 8) ? fscTable[fsci] : 32;
		uint8_t idx = 2;
		if (ats->ta1.transmitted) {
			ats->ta1.sameD = (bool)(bufferATS[idx] & 0x80);
			ats->ta1.ds = (TagBitRates)((bufferATS[idx] & 0x70) >> 4);
			ats->ta1.dr = (TagBitRates)(bufferATS[idx] & 0x07);
			idx++;
		} else { ats->ta1.ds=BITRATE_106KBITS; ats->ta1.dr=BITRATE_106KBITS; }
		if (ats->tb1.transmitted) {
			ats->tb1.fwi = (bufferATS[idx] & 0xF0) >> 4;
			ats->tb1.sfgi = bufferATS[idx] & 0x0F;
			idx++;
		} else { ats->tb1.fwi=0; ats->tb1.sfgi=0; }
		if (ats->tc1.transmitted) {
			ats->tc1.supportsCID = (bool)(bufferATS[idx] & 0x02);
			ats->tc1.supportsNAD = (bool)(bufferATS[idx] & 0x01);
		} else { ats->tc1.supportsCID=true; ats->tc1.supportsNAD=false; }
	} else {
		ats->ta1.transmitted=false; ats->tb1.transmitted=false; ats->tc1.transmitted=false;
		ats->fsc=32; ats->ta1.sameD=false; ats->ta1.ds=BITRATE_106KBITS; ats->ta1.dr=BITRATE_106KBITS;
		ats->tb1.fwi=0; ats->tb1.sfgi=0; ats->tc1.supportsCID=true; ats->tc1.supportsNAD=false;
	}
	memcpy(ats->data, bufferATS, bufferSize - 2);
	return result;
}

MFRC522::StatusCode MFRC522Extended::PICC_PPS() {
	StatusCode result;
	byte ppsBuffer[4]; byte ppsBufferSize=4;
	ppsBuffer[0]=0xD0; ppsBuffer[1]=0x00;
	result = PCD_CalculateCRC(ppsBuffer, 2, &ppsBuffer[2]);
	if (result != STATUS_OK) return result;
	result = PCD_TransceiveData(ppsBuffer, 4, ppsBuffer, &ppsBufferSize, NULL, 0, true);
	if (result == STATUS_OK) {
		PCD_WriteRegister(TxModeReg, PCD_ReadRegister(TxModeReg)|0x80);
		PCD_WriteRegister(RxModeReg, PCD_ReadRegister(RxModeReg)|0x80);
	}
	return result;
}

MFRC522::StatusCode MFRC522Extended::PICC_PPS(TagBitRates sendBitRate, TagBitRates receiveBitRate) {
	StatusCode result;
	byte ppsBuffer[5]; byte ppsBufferSize=5;
	ppsBuffer[0]=0xD0; ppsBuffer[1]=0x11;
	ppsBuffer[2]=(((sendBitRate&0x03)<<2)|(receiveBitRate&0x03))&0xE7;
	result = PCD_CalculateCRC(ppsBuffer, 3, &ppsBuffer[3]);
	if (result != STATUS_OK) return result;
	result = PCD_TransceiveData(ppsBuffer, 5, ppsBuffer, &ppsBufferSize, NULL, 0, true);
	if (result==STATUS_OK && ppsBufferSize==3 && ppsBuffer[0]==0xD0) {
		byte txReg=(PCD_ReadRegister(TxModeReg)&0x8F)|((receiveBitRate&0x03)<<4)|0x80;
		byte rxReg=(PCD_ReadRegister(RxModeReg)&0x8F)|((sendBitRate&0x03)<<4)|0x80;
		rxReg&=0xF0;
		PCD_WriteRegister(TxModeReg, txReg);
		PCD_WriteRegister(RxModeReg, rxReg);
		switch(sendBitRate) {
			case BITRATE_212KBITS: PCD_WriteRegister(ModWidthReg, 0x15); break;
			case BITRATE_424KBITS: PCD_WriteRegister(ModWidthReg, 0x0A); break;
			case BITRATE_848KBITS: PCD_WriteRegister(ModWidthReg, 0x05); break;
			default:               PCD_WriteRegister(ModWidthReg, 0x26); break;
		}
		delayMicroseconds(10);
	} else if (result==STATUS_OK) { result=STATUS_ERROR; }
	return result;
}

MFRC522::StatusCode MFRC522Extended::TCL_Transceive(PcbBlock *send, PcbBlock *back) {
	MFRC522::StatusCode result;
	byte inBuffer[FIFO_SIZE]; byte inBufferSize=FIFO_SIZE;
	byte outBuffer[send->inf.size+5]; byte outBufferOffset=1, inBufferOffset=1;
	outBuffer[0]=send->prologue.pcb;
	if (send->prologue.pcb&0x08) { outBuffer[outBufferOffset++]=send->prologue.cid; }
	if (send->prologue.pcb&0x04) { outBuffer[outBufferOffset++]=send->prologue.nad; }
	if (send->inf.size>0) { memcpy(&outBuffer[outBufferOffset], send->inf.data, send->inf.size); outBufferOffset+=send->inf.size; }
	if ((PCD_ReadRegister(TxModeReg)&0x80)!=0x80) {
		result=PCD_CalculateCRC(outBuffer, outBufferOffset, &outBuffer[outBufferOffset]);
		if (result!=STATUS_OK) return result;
		outBufferOffset+=2;
	}
	result=PCD_TransceiveData(outBuffer, outBufferOffset, inBuffer, &inBufferSize);
	if (result!=STATUS_OK) return result;
	back->prologue.pcb=inBuffer[0];
	if (send->prologue.pcb&0x08) { back->prologue.cid=inBuffer[inBufferOffset++]; }
	if (send->prologue.pcb&0x04) { back->prologue.nad=inBuffer[inBufferOffset++]; }
	if ((PCD_ReadRegister(TxModeReg)&0x80)!=0x80) {
		if ((int)(inBufferSize-inBufferOffset)<2) return STATUS_CRC_WRONG;
		byte ctrl[2]; MFRC522::StatusCode s=PCD_CalculateCRC(inBuffer, inBufferSize-2, ctrl);
		if (s!=STATUS_OK) return s;
		if (inBuffer[inBufferSize-2]!=ctrl[0]||inBuffer[inBufferSize-1]!=ctrl[1]) return STATUS_CRC_WRONG;
		inBufferSize-=2;
	}
	if (inBufferSize>inBufferOffset) {
		if ((inBufferSize-inBufferOffset)>back->inf.size) return STATUS_NO_ROOM;
		memcpy(back->inf.data, &inBuffer[inBufferOffset], inBufferSize-inBufferOffset);
		back->inf.size=inBufferSize-inBufferOffset;
	} else { back->inf.size=0; }
	if (((inBuffer[0]&0xC0)==0x80)&&(inBuffer[0]&0x20)) return STATUS_MIFARE_NACK;
	return result;
}

MFRC522::StatusCode MFRC522Extended::TCL_Transceive(TagInfo *tag, byte *sendData, byte sendLen, byte *backData, byte *backLen) {
	MFRC522::StatusCode result;
	PcbBlock out, in; byte outBuffer[FIFO_SIZE]; byte outBufferSize=FIFO_SIZE;
	byte totalBackLen = backLen ? *backLen : 0;
	out.prologue.pcb=0x02;
	if (tag->ats.tc1.supportsCID) { out.prologue.pcb|=0x08; out.prologue.cid=0x00; }
	out.prologue.pcb&=0xFB; out.prologue.nad=0x00;
	if (tag->blockNumber) out.prologue.pcb|=0x01;
	if (sendData&&sendLen>0) { out.inf.size=sendLen; out.inf.data=sendData; }
	else { out.inf.size=0; out.inf.data=NULL; }
	in.inf.data=outBuffer; in.inf.size=outBufferSize;
	result=TCL_Transceive(&out, &in);
	if (result!=STATUS_OK) return result;
	tag->blockNumber=!tag->blockNumber;
	if (backData&&backLen&&*backLen>0) {
		if (*backLen<in.inf.size) return STATUS_NO_ROOM;
		*backLen=in.inf.size; memcpy(backData, in.inf.data, in.inf.size);
	}
	if ((in.prologue.pcb&0x10)==0x00) return result;
	while (in.prologue.pcb&0x10) {
		byte ackData[FIFO_SIZE]; byte ackDataSize=FIFO_SIZE;
		result=TCL_TransceiveRBlock(tag, true, ackData, &ackDataSize);
		if (result!=STATUS_OK) return result;
		if (backData&&backLen&&*backLen>0) {
			if ((*backLen+ackDataSize)>totalBackLen) return STATUS_NO_ROOM;
			memcpy(&backData[*backLen], ackData, ackDataSize); *backLen+=ackDataSize;
		}
	}
	return result;
}

MFRC522::StatusCode MFRC522Extended::TCL_TransceiveRBlock(TagInfo *tag, bool ack, byte *backData, byte *backLen) {
	MFRC522::StatusCode result;
	PcbBlock out, in; byte outBuffer[FIFO_SIZE]; byte outBufferSize=FIFO_SIZE;
	out.prologue.pcb=ack?0xA2:0xB2;
	if (tag->ats.tc1.supportsCID) { out.prologue.pcb|=0x08; out.prologue.cid=0x00; }
	out.prologue.pcb&=0xFB; out.prologue.nad=0x00;
	if (tag->blockNumber) out.prologue.pcb|=0x01;
	out.inf.size=0; out.inf.data=NULL;
	in.inf.data=outBuffer; in.inf.size=outBufferSize;
	result=TCL_Transceive(&out, &in);
	if (result!=STATUS_OK) return result;
	tag->blockNumber=!tag->blockNumber;
	if (backData&&backLen) {
		if (*backLen<in.inf.size) return STATUS_NO_ROOM;
		*backLen=in.inf.size; memcpy(backData, in.inf.data, in.inf.size);
	}
	return result;
}

MFRC522::StatusCode MFRC522Extended::TCL_Deselect(TagInfo *tag) {
	MFRC522::StatusCode result;
	byte outBuffer[4]; byte outBufferSize=1;
	byte inBuffer[FIFO_SIZE]; byte inBufferSize=FIFO_SIZE;
	outBuffer[0]=0xC2;
	if (tag->ats.tc1.supportsCID) { outBuffer[0]|=0x08; outBuffer[1]=0x00; outBufferSize=2; }
	result=PCD_TransceiveData(outBuffer, outBufferSize, inBuffer, &inBufferSize);
	return result;
}

MFRC522::PICC_Type MFRC522Extended::PICC_GetType(TagInfo *tag) {
	byte sak=tag->uid.sak&0x7F;
	switch(sak) {
		case 0x04: return PICC_TYPE_NOT_COMPLETE;
		case 0x09: return PICC_TYPE_MIFARE_MINI;
		case 0x08: return PICC_TYPE_MIFARE_1K;
		case 0x18: return PICC_TYPE_MIFARE_4K;
		case 0x00: return PICC_TYPE_MIFARE_UL;
		case 0x10: case 0x11: return PICC_TYPE_MIFARE_PLUS;
		case 0x01: return PICC_TYPE_TNP3XXX;
		case 0x20: return (tag->atqa==0x0344)?PICC_TYPE_MIFARE_DESFIRE:PICC_TYPE_ISO_14443_4;
		case 0x40: return PICC_TYPE_ISO_18092;
		default:   return PICC_TYPE_UNKNOWN;
	}
}

void MFRC522Extended::PICC_DumpToSerial(TagInfo *tag) {
	MIFARE_Key key;
	PICC_DumpDetailsToSerial(tag);
	PICC_Type piccType=MFRC522::PICC_GetType(tag->uid.sak);
	switch(piccType) {
		case PICC_TYPE_MIFARE_MINI: case PICC_TYPE_MIFARE_1K: case PICC_TYPE_MIFARE_4K:
			for(byte i=0;i<6;i++) key.keyByte[i]=0xFF;
			PICC_DumpMifareClassicToSerial(&tag->uid, piccType, &key); break;
		case PICC_TYPE_MIFARE_UL: PICC_DumpMifareUltralightToSerial(); break;
		case PICC_TYPE_ISO_14443_4: case PICC_TYPE_MIFARE_DESFIRE: PICC_DumpISO14443_4(tag); break;
		default: break;
	}
	Serial.println(); PICC_HaltA();
}

void MFRC522Extended::PICC_DumpDetailsToSerial(TagInfo *tag) {
	Serial.print(F("Card ATQA:")); Serial.println(tag->atqa, HEX);
	Serial.print(F("Card UID:"));
	for(byte i=0;i<tag->uid.size;i++) { Serial.print(F(" ")); if(tag->uid.uidByte[i]<0x10) Serial.print(F("0")); Serial.print(tag->uid.uidByte[i],HEX); }
	Serial.println();
	Serial.print(F("Card SAK: ")); if(tag->uid.sak<0x10) Serial.print(F("0")); Serial.println(tag->uid.sak,HEX);
	Serial.print(F("PICC type: ")); Serial.println(PICC_GetTypeName(PICC_GetType(tag)));
}

void MFRC522Extended::PICC_DumpISO14443_4(TagInfo *tag) {
	if(tag->ats.size>0x00) {
		Serial.print(F("Card ATS:"));
		for(byte i=0;i<tag->ats.size;i++) { Serial.print(F(" ")); if(tag->ats.data[i]<0x10) Serial.print(F("0")); Serial.print(tag->ats.data[i],HEX); }
		Serial.println();
	}
}

bool MFRC522Extended::PICC_IsNewCardPresent() {
	byte bufferATQA[2]; byte bufferSize=sizeof(bufferATQA);
	PCD_WriteRegister(TxModeReg, 0x00); PCD_WriteRegister(RxModeReg, 0x00); PCD_WriteRegister(ModWidthReg, 0x26);
	MFRC522::StatusCode result=PICC_RequestA(bufferATQA, &bufferSize);
	if(result==STATUS_OK||result==STATUS_COLLISION) {
		tag.atqa=((uint16_t)bufferATQA[1]<<8)|bufferATQA[0];
		tag.ats.size=0; tag.ats.fsc=32;
		tag.ats.ta1.transmitted=false; tag.ats.ta1.sameD=false; tag.ats.ta1.ds=BITRATE_106KBITS; tag.ats.ta1.dr=BITRATE_106KBITS;
		tag.ats.tb1.transmitted=false; tag.ats.tb1.fwi=0; tag.ats.tb1.sfgi=0;
		tag.ats.tc1.transmitted=false; tag.ats.tc1.supportsCID=true; tag.ats.tc1.supportsNAD=false;
		memset(tag.ats.data, 0, FIFO_SIZE-2); tag.blockNumber=false;
		return true;
	}
	return false;
}

bool MFRC522Extended::PICC_ReadCardSerial() {
	MFRC522::StatusCode result=PICC_Select(&tag.uid);
	uid.size=tag.uid.size; uid.sak=tag.uid.sak;
	memcpy(uid.uidByte, tag.uid.uidByte, sizeof(tag.uid.uidByte));
	return (result==STATUS_OK);
}

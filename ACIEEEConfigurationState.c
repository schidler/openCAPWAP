/**************************************
 * 
 *  Elena Agostini elena.ago@gmail.com
 * 	IEEE Binding
 * 
 ***************************************/

#include "CWAC.h"
//#include "CWVendorPayloads.h"

#ifdef DMALLOC
#include "../dmalloc-5.5.0/dmalloc.h"
#endif


CWBool CWParseIEEEConfigurationResponseMessage(char *msg,
				      int len,
				      int *seqNumPtr,
				      int WTPIndex);
				      
CWBool CWAssembleIEEEConfigurationRequest(CWProtocolMessage **messagesPtr,
				   int *fragmentsNumPtr,
				   int PMTU,
				   int seqNum,
				   int operation,
				   int radioID,
				   int wlanNum,
				   int WTPIndex);
				   
CWBool ACUpdateInfoWlanInterface(WTPInterfaceInfo * interfaceInfo);

CWBool ACEnterIEEEConfiguration(int WTPIndex, CWProtocolMessage *msgPtr) {
	
	//Elena: Now, just for example, I update only wlan 0 interface on radio 0
	int radioID=0, seqNum=0, wlanID=0;
	CWBool ret=CW_FALSE;
	
	//Se non c'e nessun messaggio si deve inviare la richiesta...
	if(msgPtr == NULL)
	{
		CWLog("IEEE Assembling Configuration Request");

		if(!ACUpdateInfoWlanInterface(&(gWTPs[WTPIndex].WTPProtocolManager).radiosInfo.radiosInfo[radioID].gWTPPhyInfo.interfaces[wlanID]))
			return CW_FALSE;
		
		//Create Configuration Request
		if(!(CWAssembleIEEEConfigurationRequest(&(gWTPs[WTPIndex].messages), 
						 &(gWTPs[WTPIndex].messagesCount), 
						 gWTPs[WTPIndex].pathMTU, 
						 seqNum,
						 CW_OP_ADD_WLAN,
						 0,
						 wlanID,
						 WTPIndex
						 )))  { 
			return CW_FALSE;
		}
		
		if(!CWACSendFragments(WTPIndex)) {
			return CW_FALSE;
		}
		
		CWLog("IEEE Configuration Request Sent");
		
		gWTPs[WTPIndex].currentState = CW_ENTER_IEEEE_CONFIGURATION;
		return CW_TRUE;
	}
	//... altrimenti si valuta la risposta e si prosegue al datacheck
	
	CWLog("IEEE Parsing Configuration Response");
	
	if(!CWParseIEEEConfigurationResponseMessage(msgPtr->msg, msgPtr->offset, &seqNum, WTPIndex))
		return CW_FALSE;
	
	gWTPs[WTPIndex].currentState = CW_ENTER_DATA_CHECK;
	return CW_TRUE;
}

CWBool CWParseIEEEConfigurationResponseMessage(char *msg,
				      int len,
				      int *seqNumPtr,
				      int WTPIndex) {

	CWControlHeaderValues controlVal;
	int i,j, index;
	int offsetTillMessages;
	CWProtocolMessage completeMsg;
	
	CWProtocolResultCode resultCode;
	CWProtocolVendorSpecificValues *vendPtr;
	int radioIDtmp, wlanIDtmp;
	char * bssIDTmp;
				
	if(msg == NULL || seqNumPtr == NULL) 
		return CWErrorRaise(CW_ERROR_WRONG_ARG, NULL);
	
	CWDebugLog("Parsing Configuration Response...");
	
	completeMsg.msg = msg;
	completeMsg.offset = 0;
	
	if(!(CWParseControlHeader(&completeMsg, &controlVal))) 
		/* will be handled by the caller */
		return CW_FALSE;

	/* different type */
	if(controlVal.messageTypeValue != CW_MSG_TYPE_VALUE_WLAN_CONFIGURATION_RESPONSE)
		return CWErrorRaise(CW_ERROR_INVALID_FORMAT, "Message is not Configuration Response (maybe it is Image Data Request)");
	
	*seqNumPtr = controlVal.seqNum;
	/* skip timestamp */
	controlVal.msgElemsLen -= CW_CONTROL_HEADER_OFFSET_FOR_MSG_ELEMS;	
	offsetTillMessages = completeMsg.offset;
	
	/* parse message elements */
	while((completeMsg.offset-offsetTillMessages) < controlVal.msgElemsLen) {
	
		unsigned short int elemType = 0;/* = CWProtocolRetrieve32(&completeMsg); */
		unsigned short int elemLen = 0;	/* = CWProtocolRetrieve16(&completeMsg); */
		
		CWParseFormatMsgElem(&completeMsg,&elemType,&elemLen);		

		//CWLog("Parsing Message Element: %u, elemLen: %u", elemType, elemLen);
									
		switch(elemType) {
			case CW_MSG_ELEMENT_RESULT_CODE_CW_TYPE:
				if(!(CWParseResultCode(&completeMsg, elemLen, &(resultCode))))
					return CW_FALSE;
				if(resultCode != CW_PROTOCOL_SUCCESS)
						CWLog("IEEE WLAN Error");
					else
						CWLog("IEEE WLAN OK");
				break;
			case CW_MSG_ELEMENT_IEEE80211_ASSIGNED_WTP_BSSID_CW_TYPE:
				if(!(CWParseACAssignedWTPBSSID(WTPIndex, &completeMsg, elemLen, &radioIDtmp, &wlanIDtmp, &(bssIDTmp))))
					return CW_FALSE;
					
					for(index=0; index < WTP_MAX_INTERFACES; index++)
					{
						if(gWTPs[WTPIndex].WTPProtocolManager.radiosInfo.radiosInfo[radioIDtmp].gWTPPhyInfo.interfaces[index].wlanID == wlanIDtmp)
						{
							CW_CREATE_ARRAY_CALLOC_ERR(gWTPs[WTPIndex].WTPProtocolManager.radiosInfo.radiosInfo[radioIDtmp].gWTPPhyInfo.interfaces[index].BSSID, ETH_ALEN+1, char, return CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL););
							CW_COPY_MEMORY(bssIDTmp, gWTPs[WTPIndex].WTPProtocolManager.radiosInfo.radiosInfo[radioIDtmp].gWTPPhyInfo.interfaces[index].BSSID, ETH_ALEN);
							break;
						}
					}
					CW_FREE_OBJECT(bssIDTmp);
				
				break;
			/*
			case CW_MSG_ELEMENT_VENDOR_SPEC_PAYLOAD_CW_TYPE:
				CW_CREATE_OBJECT_ERR(vendPtr, CWProtocolVendorSpecificValues, return CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL););
				if (!(CWParseVendorPayload(&completeMsg, elemLen, (CWProtocolVendorSpecificValues *) vendPtr)))
				{
					CW_FREE_OBJECT(vendPtr);
					return CW_FALSE; // will be handled by the caller
				}
				break;		
			*/
			default:
				return CWErrorRaise(CW_ERROR_INVALID_FORMAT, "Unrecognized Message Element");
		}
	}
	
	if(completeMsg.offset != len) return CWErrorRaise(CW_ERROR_INVALID_FORMAT, "Garbage at the End of the Message");
	
	CWDebugLog("Configure Response Parsed");	
	return CW_TRUE;
}

CWBool CWAssembleIEEEConfigurationRequest(CWProtocolMessage **messagesPtr,
				   int *fragmentsNumPtr,
				   int PMTU,
				   int seqNum,
				   int operation,
				   int radioID,
				   int wlanNum,
				   int WTPIndex) {

	CWProtocolMessage *msgElems = NULL;
	const int MsgElemCount=1;
	CWProtocolMessage *msgElemsBinding = NULL;
	int msgElemBindingCount=0;
	int k = -1;
	
	if(messagesPtr == NULL || fragmentsNumPtr == NULL)
		return CWErrorRaise(CW_ERROR_WRONG_ARG, NULL);
	
	CWDebugLog("Assembling Configuration Response with operation %d, WTPIndex: %d, radioID: %d, wlanNum: %d", operation, WTPIndex, radioID, wlanNum);
	CW_CREATE_PROTOCOL_MSG_ARRAY_ERR(msgElems, MsgElemCount, return CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL););
	
	//Add WLAN
	if(operation == CW_OP_ADD_WLAN)
	{		
		if(
			(!(CWAssembleMsgElemACAddWlan(radioID, gWTPs[WTPIndex].WTPProtocolManager.radiosInfo.radiosInfo[radioID].gWTPPhyInfo.interfaces[wlanNum], &(msgElems[++k]))))
		){
			int i;
			for(i = 0; i <= k; i++) { CW_FREE_PROTOCOL_MESSAGE(msgElems[i]);}
			CW_FREE_OBJECT(msgElems);
			/* error will be handled by the caller */
			return CW_FALSE;
		}
	}
	//Del WLAN
	else if(operation == CW_OP_DEL_WLAN)
	{
		if(
			(!(CWAssembleMsgElemACDelWlan(radioID, wlanNum, &(msgElems[++k]))))
		){
			int i;
			for(i = 0; i <= k; i++) { CW_FREE_PROTOCOL_MESSAGE(msgElems[i]);}
			CW_FREE_OBJECT(msgElems);
			/* error will be handled by the caller */
			return CW_FALSE;
		}
	}
	//Update WLAN
	else if(operation == CW_OP_UPDATE_WLAN)
	{
		if(
			(!(CWAssembleMsgElemACUpdateWlan(radioID, gWTPs[WTPIndex].WTPProtocolManager.radiosInfo.radiosInfo[radioID].gWTPPhyInfo.interfaces[wlanNum], &(msgElems[++k]))))
		){
			int i;
			for(i = 0; i <= k; i++) { CW_FREE_PROTOCOL_MESSAGE(msgElems[i]);}
			CW_FREE_OBJECT(msgElems);
			/* error will be handled by the caller */
			return CW_FALSE;
		}
	}
	
	CWLog("Now assembling...");
	if(!(CWAssembleMessage(messagesPtr,
			       fragmentsNumPtr,
			       PMTU,
			       seqNum,
			       CW_MSG_TYPE_VALUE_WLAN_CONFIGURATION_REQUEST,
			       msgElems,
			       MsgElemCount,
			       msgElemsBinding,
			       msgElemBindingCount,
#ifdef CW_NO_DTLS
			       CW_PACKET_PLAIN))) {
#else
			       CW_PACKET_CRYPT))) {
#endif
		return CW_FALSE;
	}
	
	CWDebugLog("Configure Response Assembled");
	return CW_TRUE;
}

CWBool ACUpdateInfoWlanInterface(WTPInterfaceInfo * interfaceInfo) {
	int index;
	//WlanID by AC
	interfaceInfo->wlanID=4;
	
	//Useless for AC. WTP will not send the interface name
	interfaceInfo->ifName = NULL;
	
	//Capability
	//ESS: AC MUST set ESS 1
	interfaceInfo->capability[0]=1;
	//IBSS: AC MUST set IBSS 0
	interfaceInfo->capability[1]=0;
	//CF-Pollable
	interfaceInfo->capability[2]=0;
	//CF-Poll
	interfaceInfo->capability[3]=0;
	//Privacy
	interfaceInfo->capability[4]=0;
	//Short Preamble
	interfaceInfo->capability[5]=0;
	//PBCC
	interfaceInfo->capability[6]=0;
	//Channel Agility
	interfaceInfo->capability[7]=0;
	//Spectrum Management
	interfaceInfo->capability[8]=0;
	//QoS
	interfaceInfo->capability[9]=0;
	//Short Slot Time
	interfaceInfo->capability[10]=0;
	//APSD
	interfaceInfo->capability[11]=0;
	//Reserved: MUST be 0
	interfaceInfo->capability[12]=0;
	//DSSS-OFDM
	interfaceInfo->capability[13]=0;
	//Delayed Block ACK
	interfaceInfo->capability[14]=0;
	//Immediate Block ACK
	interfaceInfo->capability[15]=0;
	
	//Bitwise operation for capability 16-bit version
	interfaceInfo->capabilityBit=0;
	for(index=0; index<WLAN_CAPABILITY_NUM_FIELDS; index++)
		interfaceInfo->capabilityBit |= interfaceInfo->capability[index] << index;
	
	//Key not used
	interfaceInfo->keyIndex=0;
	interfaceInfo->keyStatus=0;
	interfaceInfo->keyLength=0;
	CW_ZERO_MEMORY(interfaceInfo->key, WLAN_KEY_LEN);
	
	//Group TSC: not used
	CW_ZERO_MEMORY(interfaceInfo->groupTSC, WLAN_GROUP_TSC_LEN);
	
	//QoS
	interfaceInfo->qos=0;
	//Auth Type: 0 open system, 1 wep
	interfaceInfo->authType=NL80211_AUTHTYPE_OPEN_SYSTEM;
	//Mac Mode: 0 LocalMAC, 1 Split MAC
	interfaceInfo->MACmode=0;
	//Tunnel Mode: this info is in discovery request from WTP
	interfaceInfo->tunnelMode=0;
	//Suppress SSID: 0 yes, 1 no
	interfaceInfo->suppressSSID=1;
	//SSID
	CW_CREATE_STRING_FROM_STRING_ERR(interfaceInfo->SSID, "wtpSSID", return CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL););
		
	return CW_TRUE;
}
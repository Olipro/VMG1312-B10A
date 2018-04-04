var count = 0;
var firstErrorItem="",firstVoipErrorMsg="",showVoipMsg="";
var hasErrorInBasic=false;
var hasErrorInAdvance = false;
var errorCount=0;



function codecChange()
{

	var codec1=document.getElementById("codecSelect1");
	var codec2=document.getElementById("codecSelect2");
	var codec3=document.getElementById("codecSelect3");
	var codecValue1 = codec1.value;
	var codecValue2 = codec2.value;
	var codecValue3 = codec3.value;
	if(codecValue1==codecValue2)
	{
		alert("Compression Type already existed ");
		codec1.value = recoverCodec1;
		codec2.value = recoverCodec2;
	}else if(codecValue1==codecValue3)
	{
		alert("Compression Type already existed");
		codec1.value = recoverCodec1;
		codec3.value = recoverCodec3;
	}else if(codecValue2==codecValue3 && codecValue2 != 'None')
	{
		alert("Compression Type already existed");
		codec2.value = recoverCodec2;
		codec3.value = recoverCodec3;
	}else
	{
		recoverCodec1 = codecValue1;
		recoverCodec2 = codecValue2;
		recoverCodec3 = codecValue3;
	}
	
	if(document.getElementById("codecSelect2").value != "None")
	{		
		codec3.disabled = false;
	}else
	{
		codec3.value = 'None';
		codec3.disabled = true;
	}
}
function addErrorMsg(msg)
{
	if(showVoipMsg=="")
	{
		firstVoipErrorMsg=msg;
	}
	showVoipMsg+=(++errorCount);
	showVoipMsg+=". ";
	showVoipMsg+=msg;
}
function resetGlobleValue()
{

	firstErrorItem="";
	showVoipMsg="";
	hasErrorInBasic=false;
	hasErrorInAdvance = false;
	errorCount=0;
	firstVoipErrorMsg="";
}
function setFirstErrorEmptyItem(itemName,location)
{
	if(((!hasErrorInBasic) || (!hasErrorInAdvance)) && (firstErrorItem==""))
	{
		firstErrorItem=itemName;
	}
	if(location == "basic")
	{
		hasErrorInBasic=true;
	}else
	{	
		hasErrorInAdvance=true;
	}
}
function emptyAndRuleCheck_account(itemName)
{
	valueResoult = document.getElementById(itemName).value;
	if(valueResoult=="")
	{
		return 1;
	}else if(valueResoult=="ChangeMe" || valueResoult=="changeme")
	{
		return 2;
	}else
	{
		return 0;
	}
}
function emptyCheck_account()
{
	var valueResoult="";
	var emptyCount=0,index=0;
	checkResult = emptyAndRuleCheck_account("sipAccountNumber");
	if(checkResult==1)
	{
		addErrorMsg("[Number] is Empty\n");
		setFirstErrorEmptyItem("sipAccountNumber","basic");
	}else if(checkResult==2)
	{
		addErrorMsg("Please change value for [Number] \n");
		setFirstErrorEmptyItem("sipAccountNumber","basic");
	}
	if(showVoipMsg!="")
	{
		return -1;
	}
	else
	{
		return 0;
	}
}

function callForwardCheck()
{
	var isError = false;
	var conditional1 = document.getElementById("unconditionalEnable");
	var conditional2 = document.getElementById("busyEnable");
	var conditional3 = document.getElementById("noAnswerEnable");
	var conditional4 = document.getElementById("warmHotEnable");
	var fwdNumber1 = document.getElementById("unconditionalNumber").value;
	var fwdNumber2 = document.getElementById("busyNumber").value;
	var fwdNumber3 = document.getElementById("noAnswerNumber").value;	
	var fwdNoAnswerCount = document.getElementById("noAnswerCount").value;
	var warmhotNumber = document.getElementById("warmHotNumber").value;	
	var valueRule=/^\d+$/;
	if(conditional1.checked)
	{
		if(fwdNumber1 != "")
		{
			if(!fwdNumber1.match(valueRule))
			{
				addErrorMsg("[Active Unconditional] [Number] is invalid\n");
				setFirstErrorEmptyItem("unconditionalNumber","advance");
				isError = true;
			}
			
		}else
		{
			addErrorMsg("If you enable Unconditional Forward, [To Number] can't be empty\n");
			setFirstErrorEmptyItem("unconditionalNumber","advance");
			isError = true;
		}
	}
		if(conditional2.checked)
		{
			if(fwdNumber2 != "")
			{
				if(!fwdNumber2.match(valueRule))
				{
					addErrorMsg("[Active Busy Forward] [To Number] is invalid\n");
					setFirstErrorEmptyItem("busyNumber","advance");
					isError = true;
				}
			}else
			{
				addErrorMsg("If you enable Busy Forward, [To Number] can't be empty\n");
				setFirstErrorEmptyItem("busyNumber","advance");
				isError = true;
			}
		}
		if(conditional3.checked)
		{
			if(fwdNumber3 != "")
			{
				if(!fwdNumber3.match(valueRule))
				{
					addErrorMsg("[Active No Answer Forward] [To Number] is invalid\n");
					setFirstErrorEmptyItem("noAnswerNumber","advance");
					isError = true;
				}
			}else
			{
				addErrorMsg("If you enable No Answer Forward, [To Number] can't be empty\n");
				setFirstErrorEmptyItem("noAnswerNumber","advance");
				isError = true;
			}
			if(fwdNoAnswerCount != "")
			{
				if(!fwdNoAnswerCount.match(valueRule))
				{
					addErrorMsg("[No Answer Time] is invalid\n");
					setFirstErrorEmptyItem("noAnswerCount","advance");
					isError = true;
				}
			}else
			{
				addErrorMsg("If you enable No Answer Forward, [No Answer Time] can't be empty\n");
				setFirstErrorEmptyItem("noAnswerCount","advance");
				isError = true;
			}
		}
		
		if(conditional4.checked)
		{
			if(warmhotNumber != "")
			{
				if(!warmhotNumber.match(valueRule))
				{
					addErrorMsg("[Warm Line / Hot Line Number] is invalid\n");
					setFirstErrorEmptyItem("warmHotNumber","advance");
					isError = true;
				}
			}else
			{
				addErrorMsg("If you enable [Hot Line / Warm Line Enable], [Hot Line / Warm Line number] can't be empty\n");
				setFirstErrorEmptyItem("warmHotNumber","advance");
				isError = true;
			}
			
			
		}
	if(isError)
	{
		return -1;
	}else
	{
		return 0;
	}
}



function formCheck_account()
{
	var rtpCheckStartResult=0,callForwardResult=0;
	var phoneSelect1 = document.getElementById("phoneSelect1");
	var phoneSelect2 = document.getElementById("phoneSelect2");
	var missedCallEmailTitle = document.getElementById("missed_CallEmailTitle").value;
	var missedCallEmailEnable = document.getElementById("missedCallEmailEnable");
	formCheckResult = emptyCheck_account();
	
	if(checkGreatThanZero(20,document.getElementById("waitingRejectTimer"),1,0) == -1){
		alert("Call Waiting Reject Time setting error\n");
		return false;
	}
	if(rangeCheck(10,180,20,document.getElementById("noAnswerCount"),1,0) == -1){
		alert("No Answer Timer setting error\n");
		return false;
	}		
	if(rangeCheck(5,300,5,document.getElementById("warmExpireTime"),1,0) == -1){
		alert("Warm Line Timer setting error\n");
		return false;
	}	

	if(rangeCheck(120,86400,3600,document.getElementById("mwiExpireTime"),1,0) == -1){
		alert("MWI Expiration Time setting error\n");
		return false;
	}	
	
	if ((phoneSelect1.checked == false) && (phoneSelect2.checked == false)) {
		alert("At least one phone must be select\n");
		return false;
	}

	if (missedCallEmailEnable.checked == true) {
		if (missedCallEmailTitle == '') {
		   	alert("Missed Call Email Title is an empty string\n");
			return false;
		}
	}
	
	callForwardResult = callForwardCheck();
	if((formCheckResult!=-1) && (callForwardResult!=-1)){

		return true;
		//document.getElementById("SipForm").submit();
	}else
	{
		if(hasErrorInAdvance)
		{
			document.getElementById("detailedInfo").style.display = "";
			document.getElementById("btn_wepmore").innerHTML = "less";
		}
		if(firstErrorItem!=""||hasErrorInBasic==true)
		{
			if(document.getElementById(firstErrorItem).disabled != true)
			{
				document.getElementById(firstErrorItem).focus();
			}
			//showErrMsg(firstVoipErrorMsg);
			
			alert(showVoipMsg);
			resetGlobleValue();
			return false;

		}
	}
	resetGlobleValue();
	return true;
}


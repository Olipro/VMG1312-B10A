/*______________________________________________________________________________
**	@function: doShowMsg
**
**	@description:
**		Show message in the message frame.
**	@param:
**		msgType: 0 means normal message
**				 1 means error message
**				 2 means reset all message
**		msg: Your message
**	@return:
**		none
**	@global:
**____________________________________________________________________________*/
function doShowMsg(msgType, msg)
{
	var msgDoc = parent.msgFrame.document;
	var okMsgArea = msgDoc.getElementById("okMsg");
	var errMsgArea = msgDoc.getElementById("errMsg");
	switch(msgType) {
		case 0:	/* Normal Message */
			/* Clear error message area */
			errMsgArea.innerHTML="";
			okMsgArea.innerHTML=msg;
			break;
		case 1: /* Error Message */
			errMsgArea.innerHTML=msg;
			okMsgArea.innerHTML="";
			break;
		case 2:
		default:
			errMsgArea.innerHTML="";
			okMsgArea.innerHTML="";
			break;		
	}
	return;
}
/*______________________________________________________________________________
**	@function: showMsg
**
**	@description:
**		Show normal message in the message frame.
**	@param:
**		msg: Your message
**	@return:
**		none
**	@global:
**____________________________________________________________________________*/
function showMsg(msg)
{
	doShowMsg(0, msg);
}
/*______________________________________________________________________________
**	@function: showErrMsg
**
**	@description:
**		Show error message in the message frame.
**	@param:
**		msg: Your message
**	@return:
**		none
**	@global:
**____________________________________________________________________________*/
function showErrMsg(msg)
{
	doShowMsg(1, msg);
}
/*______________________________________________________________________________
**	@function: resetMsg
**
**	@description:
**		Reset all messages
**	@param:
**		None
**	@return:
**		none
**	@global:
**____________________________________________________________________________*/
function resetMsg()
{
//	doShowMsg(2, "");
}
/*______________________________________________________________________________
**	@function: showResultMsg
**
**	@description:
**		Show execution result message in message frame.
**	@param:
**		msg: Your message
**	@return:
**		none
**	@global:
**____________________________________________________________________________*/
function showResultMsg(msg)
{
	/* Is this result is success, display success informatiion */
	if (msg == "success") {
		doShowMsg(0, "Apply OK");
	/* Is no execution, display empty */	
	} else if (msg == "noExec") {
		doShowMsg(0, "");
	} else {
		doShowMsg(1, msg);
	}
}
/*______________________________________________________________________________
**	@function: checkKeyIPv4Addr
**
**	@description:
**		Make sure user can only type the correct IPv4 address format.
**	@param:
**		e: the key event
**	@return:
**		none
**	@global:
**____________________________________________________________________________*/
function checkKeyIPv4Addr(e){
	/* IE use e.which to get key code and Firefox use e.keyCode */
	var val = window.event ? e.keyCode : e.which;
	/* IE use e.srcElement.value to get element who trigger the evnet and Firefox use e.target */
	var address = e.target ? e.target.value : e.srcElement.value

	var ipArray = address.split(".");
	var dotNum = ipArray.length-1;
	var lastIpVal = ipArray[ipArray.length-1];

	/* Initial Value, for both IE and Firefox */
	e.returnValue=true;

	/* BackSpace */
	if( val == 8 || val == 9 || val == 35 || val == 36 || val == 37 || val == 39) {
		/* 8 is backspace, 9 is tab, end is 35, home is 36, 37 is left, 39 is right*/
		return e.returnValue;
	}

	/* . case */
	if( val == 190 || val == 110 || val == 46) {
		/* DCMA memo: Can't know why the kecode of . will be 46 */
		/* Check if already have 3 dots */
		if ( dotNum == 3 ) {
			/* EX: 140.113.1.1 */
			e.returnValue=false;
			return e.returnValue;
		}
		
		/* Input ".", check if "." is already the last element */
		if( address.lastIndexOf(".") > 0 ) {
			if( address.lastIndexOf(".") == address.length - 1 ) {
				e.returnValue=false;
				return e.returnValue;
			}
		}
		
		/* Check if . is the first element */
		if( address.length == 0 ) {
			e.returnValue = false;
			return e.returnValue;
		}		
		return e.returnValue;
	}
	
	/* Only allow uer input 0 ~ 9 */
	if( val < 48 || val > 57 ){
		e.returnValue = false;
		return e.returnValue;
	}
	
	/* Process special case first */
	if( (lastIpVal.length == 1)  && (lastIpVal == "0") ) {
		/* To avoid the case of 001 or 020 or any string start with 0 */
		e.returnValue = false;
		return e.returnValue;
	}

	num = parseInt( lastIpVal + (val-48) , 10 );
	if( num > 255 ) {
		e.returnValue=false;
	}

	return e.returnValue;
}
/*______________________________________________________________________________
**	@function: validateIpv4Addr
**
**	@description:
**		Validate if a given string is a validate IPv4 address.
**		Before you submit the form, you can use this function to check if all
**		IPv4 address are validated.
**	@param:
**		ipAddr: IP address
**	@return:
**		true: A legal IPv4 address
**		false: Not a legal IPv4 address
**	@global:
**____________________________________________________________________________*/
function validateIpv4Addr(ipAddr){
	var ipArray = ipAddr.split(".");
	var dotNum = ipArray.length-1;
	var lastIpVal = ipArray[ipArray.length-1];

	/* Check dot number first, IP address should have 3 dots.
	   EX: 111.222.333.444 */
	if( dotNum != 3 || ipAddr.length > 15) {
		return false;
	}

	for( i = 0; i < ipArray.length; i++ ) {		
		if( (ipArray[i].length == 0) || (ipArray[i].length > 3) ) {
			/* Should not be this case */
			return false
		}
		
		num = parseInt( ipArray[i], 10 );
		if( num > 255 ) {
			return false;
		} else if( num < 10 && ipArray[i].length > 1 ) {
				/* The case of 01 or 0002 */
				return false
		}
	}
	return true;
}
/*______________________________________________________________________________
**	@function: checkIPv4FieldAddr
**
**	@description:
**		Verify if the input field vlaue is a correct IPv4 address.
**	@param:
**		field: The input object
**	@return:
**		None
**	@global:
**____________________________________________________________________________*/
function checkIPv4AddrField(field){
	if( (field.value.length != 0) && (validateIpv4Addr(field.value) == false) ) {
		field.focus();
		field.style.backgroundColor = "#FFCCCC";
		showErrMsg("Error: " + field.value + " is not a valid IP address");
	} else {
		field.style.backgroundColor = "";
	}
	return;
}
/*______________________________________________________________________________
**	@function: disableItem
**
**	@description:
**		If you enable someone item, it will disable some items.
**	@parameters:
**		enableType
**		sourceItemName
**		destinationItemName
**	@return:
**		none
**	@global:
**____________________________________________________________________________*/
function disableItem()
{
	
	var i=0;
	var argument = disableItem.arguments;
	var sourceChcek = document.getElementById(argument[1]).checked;
	var destinationCheck;
	switch(argument[0])
	{
		case 1:
			if(sourceChcek == false)
			{
				for(i=2;i<argument.length;i++)
				{
					document.getElementById(argument[i]).disabled = false;
				}
			}else
			{
				for(i=2;i<argument.length;i++)
				{
					document.getElementById(argument[i]).disabled = true;
				}
			}
		break;
		case 2:
			if(sourceChcek == true)
			{
				for(i=2;i<argument.length;i++)
				{
					document.getElementById(argument[i]).disabled = false;
				}
			}else
			{
				for(i=2;i<argument.length;i++)
				{
					document.getElementById(argument[i]).disabled = true;
				}
			}
		break;
		default:
		break;
	}
}
/*______________________________________________________________________________
**	@function: disableItem
**
**	@description:
**		Clean Text Field.
**	@parameters:
**		cleanItem		
**		@return:
**		none
**	@global:
**____________________________________________________________________________*/
function cleanText(cleanItem)
{
	cleanItem.value="";
}
/*______________________________________________________________________________
**	@function: disableItem
**
**	@description:
**		Check item's value is validation,
**	@parameters:
**		rangeMin: value low bound
**		rangeMax: value up bound
**		defaultValue: if "recover" is "1", if the value is not validation, it will recover to "default" value
**		check: which item object
**		recover: if "recover" is "1", if the value is not validation, it will recover to "default" value
**		alertActive: if value is not validation , it will show alert box
**		@return:
**		none
**	@global:
**____________________________________________________________________________*/
function rangeCheck(rangeMin,rangeMax,defaultValue,checkItem,recover,alertActive)
{

	
	var checkValue=0;
	var matchValue="";
	var valueRule=/^\d+$/;
	checkValue = checkItem.value;
	matchValue = checkValue.match(valueRule);
	if(matchValue)
	{
		if(checkValue<rangeMin || checkValue>rangeMax)
		{
			
			if(recover==1)
			{
				checkItem.value = defaultValue;
			}
			if(alertActive)
			{
				alert("Value range error");
			}
			return -1;
		}
	}else
	{
		if(recover==1)
		{
			checkItem.value = defaultValue;
			
		}
		if(alertActive)
		{
			alert("Invalid value");
		}
		return -1;
	}
	return 0;
}
/*______________________________________________________________________________
**	@function: checkGreatThanZero
**
**	@description:
**		Check item's value is validation,
**	@parameters:
**		rangeMin: value low bound
**		rangeMax: value up bound
**		defaultValue: if "recover" is "1", if the value is not validation, it will recover to "default" value
**		check: which item object
**		recover: if "recover" is "1", if the value is not validation, it will recover to "default" value
**		alertActive: if value is not validation , it will show alert box
**		@return:
**		none
**	@global:
**____________________________________________________________________________*/
function checkGreatThanZero(defaultValue,checkItem,recover,alertActive)
{
	var checkValue=0;
	var checknum=0;
	var matchValue="";
	var valueRule=/^\d+$/;
	checkValue = checkItem.value;
	matchValue = checkValue.match(valueRule);
	if(matchValue)
	{
		if(checkValue<=checknum)
		{
			if(recover==1)
			{
				checkItem.value = defaultValue;
			}
			if(alertActive)
			{
				alert("Value need greater than 0");
			}
			return -1;
		}
	}else
	{
		if(recover==1)
		{
			checkItem.value = defaultValue;
		}
		if(alertActive)
		{
			alert("Invalid value");
		}
		return -1;
	}
	return 0;
}


/*______________________________________________________________________________
**	@function: indexSelectSubmit
**
**	@description:
**		for select and submit
**	@parameters:
**
**		@return:
**		none
**	@global:
**____________________________________________________________________________*/
function indexSelectSubmit(submitItemName)
{
	document.getElementById(submitItemName).submit();
}
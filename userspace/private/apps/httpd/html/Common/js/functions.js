// JavaScript Document
function formatThousandths(num) {
  num = num + "";
  var re = /(-?\d+)(\d{3})/;
  while (re.test(num)) {
    num = num.replace(re, "$1,$2");
  }
  return num;
}

//show/hide steps
function showHideSteps(hideThis, showThis){
	var hideThis = document.getElementById(hideThis);
	var showThis =document.getElementById(showThis);
	hideThis.style.display ='none';
	showThis.style.display ='block';
}

var MonthName = ["January","February","March","April","May","June","July","August","September","October","November","December"];

function timeStrFormat(str,iLen,space)   
{   
	if(str.length<iLen)   
	{   
		for(iIndex=0;iIndex<iLen-str.length;iIndex++)   
		{
		  if ( space==true ) {
				str="&nbsp;"+str;
			}
			else {
				str="0"+str;
			}
		}   
		return str;   
	}   
	else {
		return str;
	}
}

function hourStrFormat(hour,APM)   
{
	var str = "";
	if ( hour >= 12 ) {
		if ( APM == true ) {
			str = "PM";
		}
		else if ( hour == 12 ) {
			str = "12";
		} else {
			str += hour-12;
		}
	}
	else {
		if ( APM == true ) {
			str = "AM";
		}
		else {
			str += hour;
		}
	}
	return str;
}

function timestampToFullStr ( timestamp ) {
	var d = new Date();
	var localOffset = d.getTimezoneOffset() * 60000;
	var now = new Date(timestamp*1000+localOffset);

	var days = timeStrFormat(now.getDate().toString(),2,true);
	var years = now.getFullYear();
	var months = now.getMonth();
	var hr = now.getHours();
	var min = timeStrFormat(now.getMinutes().toString(),2,false);
	return MonthName[months]+"&nbsp;"+days+",&nbsp;"+years+"&nbsp;-&nbsp;"+ timeStrFormat(hourStrFormat(hr,false),2,true) + ":" + min + " " + hourStrFormat(hr,true);
}

function timestampToDayStr ( timestamp ) {
	var result="";
	var sec = timestamp%60;
	var min = parseInt(timestamp/60)%60;
	var hr = parseInt(timestamp/3600)%24;
	var days = parseInt(timestamp/86400);	

	if ( days > 0 )	{
		if ( days > 1 ) {
			result += days+" days ";
		}
		else {
			result += days+" day ";
		}
	}

	if ( hr > 0 )	{
		if ( hr > 1 ) {
			result += hr+" hrs ";
		}
		else {
			result += hr+" hr ";
		}
	}

	if ( min > 0 )	{
		if ( min > 1 ) {
			result += min+" mins ";
		}
		else {
			result += min+" min ";
		}
	}

	if ( sec > 0 )	{
		if ( sec > 1 ) {
			result += sec+" secs";
		}
		else {
			result += sec+" sec";
		}
	}
	return result;
}

function BackgroundUrl(url)
{
	var objXMLHTTP = null;
	
	if (window.XMLHttpRequest)  {
		objXMLHTTP=new XMLHttpRequest();
	}// code for IE
	else if (window.ActiveXObject)  {
		objXMLHTTP=new ActiveXObject("Microsoft.XMLHTTP");
	}
	else {
		alert("The browser no support XMLHttp Object");
		return;
	}
	
	if ( objXMLHTTP != null ) {
		objXMLHTTP.open("POST","./"+url,true);
		objXMLHTTP.onreadystatechange = function()
		{
		}
		objXMLHTTP.send(null);
	}
}

function BackgroundUrlSync(url)
{
	var objXMLHTTP = null;
	
	if (window.XMLHttpRequest)  {
		objXMLHTTP=new XMLHttpRequest();
	}// code for IE
	else if (window.ActiveXObject)  {
		objXMLHTTP=new ActiveXObject("Microsoft.XMLHTTP");
	}
	else {
		alert("The browser no support XMLHttp Object");
		return;
	}
	
	if ( objXMLHTTP != null ) {
		objXMLHTTP.open("POST","/"+url,false);
		objXMLHTTP.send(null);
		var text = objXMLHTTP.responseText;
	}
}

function ConfirmOpen ( msg, callbackOk, callbackCancel, w, h, t ) {
	return window.parent.ConfirmOpen ( msg, callbackOk, callbackCancel, w, h, t );
}

//__MSTC__, FuChia, QoS
var gblTimeValue;


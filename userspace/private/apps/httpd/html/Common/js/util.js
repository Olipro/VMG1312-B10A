//__MSTC__, Dennis, for partial string
function getPartialString(str,len)
{
   var tmp = "";
   if(str.length>len)
   {
      tmp = str.substr(0,len);
      tmp += "...";
   }else{
      tmp = str;
   }
   return tmp;
}
function isValidIpSubnetAddress(address) {
   ipParts = address.split('/');
   if (ipParts.length > 2) return false;
   if (ipParts.length == 2) {
      num = parseInt(ipParts[1]);
      if (num <= 0 || num > 32)
         return false;
   }

   if (ipParts[0] == '0.0.0.0' ||
       ipParts[0] == '255.255.255.255' )
      return false;

   addrParts = ipParts[0].split('.');
   if ( addrParts.length != 4 ) return false;
   for (i = 0; i < 4; i++) {
      if (isNaN(addrParts[i]) || addrParts[i] =="")
         return false;
      num = parseInt(addrParts[i]);
      if ( num < 0 || num > 255 )
         return false;
   }
   return true;
}
function isHexaDigit(digit) {
   var hexVals = new Array("0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
                           "A", "B", "C", "D", "E", "F", "a", "b", "c", "d", "e", "f");
   var len = hexVals.length;
   var i = 0;
   var ret = false;

   for ( i = 0; i < len; i++ )
      if ( digit == hexVals[i] ) break;

   if ( i < len )
      ret = true;

   return ret;
}

function isValidKey(val, size) {
   var ret = false;
   var len = val.length;
   var dbSize = size * 2;

   if ( len == size )
      ret = true;
   else if ( len == dbSize ) {
      for ( i = 0; i < dbSize; i++ )
         if ( isHexaDigit(val.charAt(i)) == false )
            break;
      if ( i == dbSize )
         ret = true;
   } else
      ret = false;

   return ret;
}


function isValidHexKey(val, size) {
   var ret = false;
   if (val.length == size) {
      for ( i = 0; i < val.length; i++ ) {
         if ( isHexaDigit(val.charAt(i)) == false ) {
            break;
         }
      }
      if ( i == val.length ) {
         ret = true;
      }
   }

   return ret;
}


function isNameUnsafe(compareChar) {
   var unsafeString = "\"<>%\\^[]`\+\$\,='#&@.: \t(){}|/;";

   if ( unsafeString.indexOf(compareChar) == -1 && compareChar.charCodeAt(0) > 32
        && compareChar.charCodeAt(0) < 123 )
      return false; // found no unsafe chars, return false
   else
      return true;
}   

// Check if a name valid
function isValidName(name) {
   var i = 0;	
   
   for ( i = 0; i < name.length; i++ ) {
      if ( isNameUnsafe(name.charAt(i)) == true )
         return false;
   }

   return true;
}

// same as is isNameUnsafe but allow spaces
function isCharUnsafe(compareChar) {
   var unsafeString = "\"<>%\\^[]`\+\$\,='#&@.:\t";
	
   if ( unsafeString.indexOf(compareChar) == -1 && compareChar.charCodeAt(0) >= 32
        && compareChar.charCodeAt(0) < 123 )
      return false; // found no unsafe chars, return false
   else
      return true;
}   

function isValidNameWSpace(name) {
   var i = 0;	
   
   for ( i = 0; i < name.length; i++ ) {
      if ( isCharUnsafe(name.charAt(i)) == true )
         return false;
   }

   return true;
}
//__MSTC__, Dennis merge from 406 common trunk
function isValidNameNoSpecialCht(value, msg){
   var alphaExp = /^[0-9a-zA-Z]+$/;
   if (value.match(alphaExp)) {
       return true;
   } else {
       return false;
   }
}
function isDescriptionCharUnsafe(compareChar) {
   var unsafeString = "\"'\t`\$#&@%\\|";
   
   if ( unsafeString.indexOf(compareChar) == -1 && compareChar.charCodeAt(0) >= 32
        && compareChar.charCodeAt(0) < 127 )
      return false; // found no unsafe chars, return false
   else
      return true;
} 
function isValidDescription(name) {
   var i = 0;	
   
   for ( i = 0; i < name.length; i++ ) {
      if ( isDescriptionCharUnsafe(name.charAt(i)) == true )
         return false;
   }

   return true;
}
// __MSTC__, Dennis end
function isSameSubNet(lan1Ip, lan1Mask, lan2Ip, lan2Mask) {

   var count = 0;
   
   lan1a = lan1Ip.split('.');
   lan1m = lan1Mask.split('.');
   lan2a = lan2Ip.split('.');
   lan2m = lan2Mask.split('.');

   for (i = 0; i < 4; i++) {
      l1a_n = parseInt(lan1a[i]);
      l1m_n = parseInt(lan1m[i]);
      l2a_n = parseInt(lan2a[i]);
      l2m_n = parseInt(lan2m[i]);
      if ((l1a_n & l1m_n) == (l2a_n & l2m_n))
         count++;
   }
   if (count == 4)
      return true;
   else
      return false;
}

function isNetworkAddress(lanIp, lanMask) {

   var count = 0;
   var i;
   var lan, lanm;

   lan = lanIp.split('.');
   lanm = lanMask.split('.');

   for (i = 0; i < 4; i++) {
      la_n = parseInt(lan[i]);
      lm_n = parseInt(lanm[i]);
      if ((la_n & lm_n) == la_n)
         count++;
   }
   if (count == 4)
      return true;
   else
      return false;
}

function isBroadcastAddress(lanIp, lanMask) {
   var i;
   var count = 0;
   var lan, lanm;

   lan = lanIp.split('.');
   lanm = lanMask.split('.');
   for (i = 0 ; i < 4 ; i++) {
      var broadcastIp;
      broadcastIp = (parseInt(lan[i]) & lanm[i]) + 255 - parseInt(lanm[i]);
      if (broadcastIp == parseInt(lan[i]))
         count++;
   }

   if (count == 4)
      return true;
   else
      return false;
}

function isValidIpAddress(address) {

   ipParts = address.split('/');
   if (ipParts.length > 2) return false;
   if (ipParts.length == 2) {
      num = parseInt(ipParts[1]);
      if (num <= 0 || num > 32)
         return false;
   }

   if (ipParts[0] == '0.0.0.0' ||
       ipParts[0] == '255.255.255.255' )
      return false;

   addrParts = ipParts[0].split('.');
   if ( addrParts.length != 4 ) return false;
   for (i = 0; i < 4; i++) {
      if (isNaN(addrParts[i]) || addrParts[i] =="")
         return false;
      num = parseInt(addrParts[i]);
      if ( num < 0 || num > 255 )
         return false;
   }
   return true;
}

function substr_count (haystack, needle, offset, length)
{
    var pos = 0, cnt = 0;

    haystack += '';
    needle += '';
    if (isNaN(offset)) {offset = 0;}
    if (isNaN(length)) {length = 0;}
    offset--;

    while ((offset = haystack.indexOf(needle, offset+1)) != -1){
        if (length > 0 && (offset+needle.length) > length){
            return false;
        } else{
            cnt++;
        }
    }

    return cnt;
}

function test_ipv6(ip)
{
  // Test for empty address
  if (ip.length<3)
  {
	return ip == "::";
  }

  // Check if part is in IPv4 format
  if (ip.indexOf('.')>0)
  {
        lastcolon = ip.lastIndexOf(':');

        if (!(lastcolon && isValidIpAddress(ip.substr(lastcolon + 1))))
            return false;

        // replace IPv4 part with dummy
        ip = ip.substr(0, lastcolon) + ':0:0';
  } 

  // Check uncompressed
  if (ip.indexOf('::')<0)
  {
    var match = ip.match(/^(?:[a-f0-9]{1,4}:){7}[a-f0-9]{1,4}$/i);
    return match != null;
  }

  // Check colon-count for compressed format
  if (substr_count(ip, ':'))
  {
    var match = ip.match(/^(?::|(?:[a-f0-9]{1,4}:)+):(?:(?:[a-f0-9]{1,4}:)*[a-f0-9]{1,4})?$/i);
    return match != null;
  } 

  // Not a valid IPv6 address
  return false;
}

function isValidIpAddress6(address) {
   ipParts = address.split('/');
   if (ipParts.length > 2) return false;
   if (ipParts.length == 2) {
      num = parseInt(ipParts[1]);
      if (num <= 0 || num > 128)
         return false;
   }

   return test_ipv6(ipParts[0]);
}

// DingRuei, check stateful interface ID
function isValidStatefulIntfId(address) {
   var num;
   addrParts = address.split(':');
   if (addrParts.length < 1 || addrParts.length > 8)
      return false;
   for (i = 0; i < addrParts.length; i++) {
      if(addrParts[i] == "")
         return false;
      if(addrParts[i].length > 4 )
         return false;
      for (var j = 0; j < addrParts[i].length; j++)
      {
         if(((addrParts[i].charAt(j) >= '0') &&
             (addrParts[i].charAt(j) <= '9')) ||
            ((addrParts[i].charAt(j) >= 'a') &&
             (addrParts[i].charAt(j) <= 'f')) ||
            ((addrParts[i].charAt(j) >= 'A') &&
             (addrParts[i].charAt(j) <= 'F')) )
            continue
         return false;
      }
      if ( addrParts[i] != "" )
         num = parseInt(addrParts[i], 16);
      else
         return false;
      if ( num != 0 )
         break;
   }
   return true;
}

function isValidPrefixLength(prefixLen) {
   var num;

   num = parseInt(prefixLen);
   if (num <= 0 || num > 128)
      return false;
   return true;
}

function areSamePrefix(addr1, addr2) {
   var i, j;
   var a = [0, 0, 0, 0, 0, 0, 0, 0];
   var b = [0, 0, 0, 0, 0, 0, 0, 0];

   addr1Parts = addr1.split(':');
   if (addr1Parts.length < 3 || addr1Parts.length > 8)
      return false;
   addr2Parts = addr2.split(':');
   if (addr2Parts.length < 3 || addr2Parts.length > 8)
      return false;
   j = 0;
   for (i = 0; i < addr1Parts.length; i++) {
      if ( addr1Parts[i] == "" ) {
		 if ((i != 0) && (i+1 != addr1Parts.length)) {
			j = j + (8 - addr1Parts.length + 1);
		 }
		 else {
		    j++;
		 }
	  }
	  else {
         a[j] = parseInt(addr1Parts[i], 16);
		 j++;
	  }
   }
   j = 0;
   for (i = 0; i < addr2Parts.length; i++) {
      if ( addr2Parts[i] == "" ) {
		 if ((i != 0) && (i+1 != addr2Parts.length)) {
			j = j + (8 - addr2Parts.length + 1);
		 }
		 else {
		    j++;
		 }
	  }
	  else {
         b[j] = parseInt(addr2Parts[i], 16);
		 j++;
	  }
   }
   //only compare 64 bit prefix
   for (i = 0; i < 4; i++) {
      if (a[i] != b[i]) {
	     return false;
	  }
   }
   return true;
}

function getLeftMostZeroBitPos(num) {
   var i = 0;
   var numArr = [128, 64, 32, 16, 8, 4, 2, 1];

   for ( i = 0; i < numArr.length; i++ )
      if ( (num & numArr[i]) == 0 )
         return i;

   return numArr.length;
}

function getRightMostOneBitPos(num) {
   var i = 0;
   var numArr = [1, 2, 4, 8, 16, 32, 64, 128];

   for ( i = 0; i < numArr.length; i++ )
      if ( ((num & numArr[i]) >> i) == 1 )
         return (numArr.length - i - 1);

   return -1;
}

function isValidSubnetMask(mask) {
   var i = 0, num = 0;
   var zeroBitPos = 0, oneBitPos = 0;
   var zeroBitExisted = false;

   if ( mask == '0.0.0.0' )
      return false;

   maskParts = mask.split('.');
   if ( maskParts.length != 4 ) return false;

   for (i = 0; i < 4; i++) {
      if ( isNaN(maskParts[i]) == true )
         return false;
      num = parseInt(maskParts[i]);
      if ( num < 0 || num > 255 )
         return false;
      if ( zeroBitExisted == true && num != 0 )
         return false;
      zeroBitPos = getLeftMostZeroBitPos(num);
      oneBitPos = getRightMostOneBitPos(num);
      if ( zeroBitPos < oneBitPos )
         return false;
      if ( zeroBitPos < 8 )
         zeroBitExisted = true;
   }

   return true;
}

//__MSTC__, Delon Yu, Support Brick NAT GUI
function isValidPort(port) {
   var fromport = 1;
   var toport = 100;

   portrange = port.split(':');
   if ( portrange.length < 1 || portrange.length > 2 ) {
       return false;
   }

   if ( isNaN(portrange[0]) )
       return false;
   fromport = parseInt(portrange[0]);
   if ( isNaN(fromport) )
       return false;
   
   if ( portrange.length > 1 ) {
       if ( isNaN(portrange[1]) )
          return false;
       toport = parseInt(portrange[1]);
       if ( isNaN(toport) )
          return false;
       if ( toport < fromport )
           return false;      
   }
   
   if ( fromport < 1 || fromport > 65535 || toport < 1 || toport > 65535 )
       return false;
   
   return true;
}

function isValidNatPort(port) {
   var fromport = 0;
   var toport = 100;

   portrange = port.split('-');
   if ( portrange.length < 1 || portrange.length > 2 ) {
       return false;
   }
   if ( isNaN(portrange[0]) )
       return false;
   fromport = parseInt(portrange[0]);

   if ( portrange.length > 1 ) {
       if ( isNaN(portrange[1]) )
          return false;
       toport = parseInt(portrange[1]);
       if ( toport <= fromport )
           return false;
   }

   if ( fromport < 1 || fromport > 65535 || toport < 1 || toport > 65535 )
       return false;

   return true;
}

function isMCastorBCastMac(str)
{
	var str1 = str.split(':');
	var binaryVal = parseInt(str1[0], 16).toString(2);
	var lastDigit = binaryVal.substring(binaryVal.length-1)

	if (lastDigit == '1')
	   return 1;
	else
	   return 0;
}


function isValidMacAddress(address) {
   var c = '';
   var num = 0;
   var i = 0, j = 0;
   var zeros = 0;

   addrParts = address.split(':');
   if ( addrParts.length != 6 ) return false;

   for (i = 0; i < 6; i++) {
      if ( addrParts[i] == '' )
         return false;
      for ( j = 0; j < addrParts[i].length; j++ ) {
         c = addrParts[i].toLowerCase().charAt(j);
         if ( (c >= '0' && c <= '9') ||
              (c >= 'a' && c <= 'f') )
            continue;
         else
            return false;
      }

      num = parseInt(addrParts[i], 16);
      if ( num == NaN || num < 0 || num > 255 )
         return false;
      if ( num == 0 )
         zeros++;
   }
   if (zeros == 6)
      return false;

   return true;
}

//ZyXEL,Adam,support wildcard src mac
function isWildcardMacAddress(address) {
   var c = '';
   var num = 0;
   var i = 0, j = 0;
   var zeros = 0;
   var isWildcard = false;
   var len;

  /*check wildcard character * */
  if(address.length != 17)
  {
  	if(address.charAt(8) == '*'  || 
	   address.charAt(11) == '*' || 
	   address.charAt(14) == '*')
	   {
		isWildcard = true;
		address = address.substr(0,address.length-1); 
	   }
	else
		return false;
  }

   addrParts = address.split(':');
   len = (isWildcard) ? addrParts.length : 6;

   for (i = 0; i < len; i++) {
      if ( addrParts[i] == '' )
         return false;
      for ( j = 0; j < addrParts[i].length; j++ ) {
         c = addrParts[i].toLowerCase().charAt(j);
         if ( (c >= '0' && c <= '9') ||
              (c >= 'a' && c <= 'f') )
            continue;
         else
            return false;
      }

      num = parseInt(addrParts[i], 16);
      if ( num == NaN || num < 0 || num > 255 )
         return false;
      if ( num == 0 )
         zeros++;
   }
   if (zeros == 6)
      return false;

   return true;
}


function isValidMacMask(mask) {
   var c = '';
   var num = 0;
   var i = 0, j = 0;
   var zeros = 0;
   var zeroBitPos = 0, oneBitPos = 0;
   var zeroBitExisted = false;

   maskParts = mask.split(':');
   if ( maskParts.length != 6 ) return false;

   for (i = 0; i < 6; i++) {
      if ( maskParts[i] == '' )
         return false;
      for ( j = 0; j < maskParts[i].length; j++ ) {
         c = maskParts[i].toLowerCase().charAt(j);
         if ( (c >= '0' && c <= '9') ||
              (c >= 'a' && c <= 'f') )
            continue;
         else
            return false;
      }

      num = parseInt(maskParts[i], 16);
      if ( num == NaN || num < 0 || num > 255 )
         return false;
      if ( zeroBitExisted == true && num != 0 )
         return false;
      if ( num == 0 )
         zeros++;
      zeroBitPos = getLeftMostZeroBitPos(num);
      oneBitPos = getRightMostOneBitPos(num);
      if ( zeroBitPos < oneBitPos )
         return false;
      if ( zeroBitPos < 8 )
         zeroBitExisted = true;
   }
   if (zeros == 6)
      return false;

   return true;
}

var hexVals = new Array("0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
              "A", "B", "C", "D", "E", "F");
var unsafeString = "\"<>%\\^[]`\+\$\,'#&";
// deleted these chars from the include list ";", "/", "?", ":", "@", "=", "&" and #
// so that we could analyze actual URLs

function isUnsafe(compareChar)
// this function checks to see if a char is URL unsafe.
// Returns bool result. True = unsafe, False = safe
{
   if ( unsafeString.indexOf(compareChar) == -1 && compareChar.charCodeAt(0) > 32
        && compareChar.charCodeAt(0) < 123 )
      return false; // found no unsafe chars, return false
   else
      return true;
}

function decToHex(num, radix)
// part of the hex-ifying functionality
{
   var hexString = "";
   while ( num >= radix ) {
      temp = num % radix;
      num = Math.floor(num / radix);
      hexString += hexVals[temp];
   }
   hexString += hexVals[num];
   return reversal(hexString);
}

function reversal(s)
// part of the hex-ifying functionality
{
   var len = s.length;
   var trans = "";
   for (i = 0; i < len; i++)
      trans = trans + s.substring(len-i-1, len-i);
   s = trans;
   return s;
}

function convert(val)
// this converts a given char to url hex form
{
   return  "%" + decToHex(val.charCodeAt(0), 16);
}


function encodeUrl(val)
{
   var len     = val.length;
   var i       = 0;
   var newStr  = "";
   var original = val;

   for ( i = 0; i < len; i++ ) {
      if ( val.substring(i,i+1).charCodeAt(0) < 255 ) {
         // hack to eliminate the rest of unicode from this
         if (isUnsafe(val.substring(i,i+1)) == false)
            newStr = newStr + val.substring(i,i+1);
         else
            newStr = newStr + convert(val.substring(i,i+1));
      } else {
         // woopsie! restore.
         alert ("Found a non-ISO-8859-1 character at position: " + (i+1) + ",\nPlease eliminate before continuing.");
         newStr = original;
         // short-circuit the loop and exit
         i = len;
      }
   }

   return newStr;
}

var markStrChars = "\"'";

// Checks to see if a char is used to mark begining and ending of string.
// Returns bool result. True = special, False = not special
function isMarkStrChar(compareChar)
{
   if ( markStrChars.indexOf(compareChar) == -1 )
      return false; // found no marked string chars, return false
   else
      return true;
}

// use backslash in front one of the escape codes to process
// marked string characters.
// Returns new process string
function processMarkStrChars(str) {
   var i = 0;
   var retStr = '';

   for ( i = 0; i < str.length; i++ ) {
      if ( isMarkStrChar(str.charAt(i)) == true )
         retStr += '\\';
      retStr += str.charAt(i);
   }

   return retStr;
}

// Web page manipulation functions

function showhide(element, sh)
{
    var status;
    if (sh == 1) {
        status = "block";
    }
    else {
        status = "none"
    }
    
	if (document.getElementById)
	{
		// standard
		document.getElementById(element).style.display = status;
	}
	else if (document.all)
	{
		// old IE
		document.all[element].style.display = status;
	}
	else if (document.layers)
	{
		// Netscape 4
		document.layers[element].display = status;
	}
}

// Load / submit functions

function getSelect(item)
{
	var idx;
	if (item.options.length > 0) {
	    idx = item.selectedIndex;
	    return item.options[idx].value;
	}
	else {
		return '';
    }
}

function setSelect(item, value)
{
	for (i=0; i<item.options.length; i++) {
        if (item.options[i].value == value) {
        	item.selectedIndex = i;
        	break;
        }
    }
}

function setCheck(item, value)
{
    if ( value == '1' ) {
         item.checked = true;
    } else {
         item.checked = false;
    }
}

function setDisable(item, value)
{
    if ( value == 1 || value == '1' ) {
         item.disabled = true;
    } else {
         item.disabled = false;
    }     
}

function submitText(item)
{
	return '&' + item.name + '=' + item.value;
}

function submitSelect(item)
{
	return '&' + item.name + '=' + getSelect(item);
}


function submitCheck(item)
{
	var val;
	if (item.checked == true) {
		val = 1;
	} 
	else {
		val = 0;
	}
	return '&' + item.name + '=' + val;
}


//__MSTC__, Delon Yu, Support Brick NAT GUI
function processLongNameselect( longname, limit ) {
	if(longname.length > limit ){
		var temp_name = longname.substring(0,limit-3);
		temp_name += "...";
		return temp_name;
	}
	else {
		return longname;
	}
}


//__MSTC__,Lynn
function isHostNameUnsafe(compareChar) {
   var unsafeString = "\"<>%\\^[]`\+\$='#&\t(){}|/;";

   if ( unsafeString.indexOf(compareChar) == -1 && compareChar.charCodeAt(0) >= 32
        && compareChar.charCodeAt(0) < 123 )
      return false; // found no unsafe chars, return false
   else
      return true;
}

function isValidHostName(name) {
   var i = 0;

   for ( i = 0; i < name.length; i++ ) {
      if ( isHostNameUnsafe(name.charAt(i)) == true )
         return false;
   }

   return true;
}

function checkValidHostName(name) {
   var i = 0;
    //check the whole character of string
   if (isValidHostName(name)) {   
	   var temp=name.split(".");
		if(temp.length>1 )
		{
			for(i=0;i<temp.length;i++)	{
			//check the case "XXX..XXX"
			  if(temp[i].length==0) return false;
			}
			  return true;
		}
   }
   return false;
}

//__MSTC__, TengChang, Merge from Common_406
function isValidEmail(emailtoCheck) {
   var regExp = /^[^@^\s]+@[^\.@^\s]+(\.[^\.@^\s]+)+$/;

   if ( emailtoCheck.match(regExp) )
      return true;
   else
      return false;
}

//__ZYXEL__, SinJia, Merged by Paul Ho
//Insert html new line automatically with specific string length
//Parameters:
//    inStr: Input string
//    lineWord: Number of char a line, like 32.
//Return values:
//    outString: Output string with <br>
function strWarp(inStr,lineWord){
		var i = 0;
		var outStr = "";
		var bound = 0;
		for(i = 0;i < inStr.length;i+=lineWord){			
			if(i+lineWord<inStr.length){
				outStr= outStr+inStr.substring(i,i+lineWord)+"<br>";
			}else{
				outStr= outStr+inStr.substring(i,inStr.length);
			}
		}
		return outStr;
}

//__ZYXEL__, SinJia, Merged by Paul Ho
//Check the encode string. 
//If the string contains non-ascii, it will return false
//Parameters:
//    val: The string needs to be check
//Return values:
//    -1: The string is valid
//    otherwise: The index of invalid char in string
function checkUrlEncode(val)
{
   var len     = val.length;
   var i       = 0;
   for ( i = 0; i < len; i++ ) {
      if (!(val.substring(i,i+1).charCodeAt(0) < 255) ) {
         return i;
      }
   }

   return -1;
}

//__ZYXEL__, SinJia, Merged by Paul Ho
//Check is the string is consisted of the same token, like "********"
//Parameters:
//    str: Input string
//    token: Single char for repeated
//    length: Expected str length
function isRepeatedStr(str, token, length){
   var valid = true;
   var i = 0;
   
   if(str.length != length){
      valid = false;
      return valid;
   }

   for(i = 0; i<str.length; i++){
      if(str.charAt(i) != token){
         valid = false;
         break;
      }
   }

   return valid;
}

// __MSTC__, Paul Ho, __OBM__, SinJia
/*
 * getWpaPskKeyFromPassphrase( pass: string, ssid: string)
 *  -> string of 64 hex digits
 *
 * Compute the binary PMK from passphrase and SSID as used in the WPA-PSK
 * wireless encryption standard. The passphrase is usually entered by
 * the user as a string of at most 63 characters; the binary key is usually
 * displayed as a sequence of 64 hex digits.
 */
function getWpaPskKeyFromPassphrase(pass, salt) {
 
  /* pad string to 64 bytes and convert to 16 32-bit words */
  function stringtowords(s, padi) {
    /* return a 80-word array for later use in the SHA1 code */
    var z = new Array(80);
    var j = -1, k = 0;
    var n = s.length;
    for (var i = 0; i < 64; i++) {
      var c = 0;
      if (i < n) {
        c = s.charCodeAt(i);
      } else if (padi) {
        /* add 4-byte PBKDF2 block index and
	   standard padding for the final SHA1 input block */
	if (i == n) c = (padi >>> 24) & 0xff;
	else if (i == n + 1) c = (padi >>> 16) & 0xff;
	else if (i == n + 2) c = (padi >>> 8) & 0xff;
	else if (i == n + 3) c = padi & 0xff;
	else if (i == n + 4) c = 0x80;
      }
      if (k == 0) { j++; z[j] = 0; k = 32; }
      k -= 8;
      z[j] = z[j] | (c << k);
    }
    if (padi) z[15] = 8 * (64 + n + 4);
    return z;
  }
 
  /* compute the intermediate SHA1 state after processing just
     the 64-byte padded HMAC key */
  function initsha(w, padbyte) {
    var pw = (padbyte << 24) | (padbyte << 16) | (padbyte << 8) | padbyte;
    for (var t = 0; t < 16; t++) w[t] ^= pw;
    var s = [ 0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0 ];
    var a = s[0], b = s[1], c = s[2], d = s[3], e = s[4];
    var t;
    for (var k = 16; k < 80; k++) {
      t = w[k-3] ^ w[k-8] ^ w[k-14] ^ w[k-16];
      w[k] = (t<<1) | (t>>>31);
    }
    for (var k = 0; k < 20; k++) {
      t = ((a<<5) | (a>>>27)) + e + w[k] + 0x5A827999 + ((b&c)|((~b)&d));
      e = d; d = c; c = (b<<30) | (b>>>2); b = a; a = t & 0xffffffff;
    }
    for (var k = 20; k < 40; k++) {
      t = ((a<<5) | (a>>>27)) + e + w[k] + 0x6ED9EBA1 + (b^c^d);
      e = d; d = c; c = (b<<30) | (b>>>2); b = a; a = t & 0xffffffff;
    }
    for (var k = 40; k < 60; k++) {
      t = ((a<<5) | (a>>>27)) + e + w[k] + 0x8F1BBCDC + ((b&c)|(b&d)|(c&d));
      e = d; d = c; c = (b<<30) | (b>>>2); b = a; a = t & 0xffffffff;
    }
    for (var k = 60; k < 80; k++) {
      t = ((a<<5) | (a>>>27)) + e + w[k] + 0xCA62C1D6 + (b^c^d);
      e = d; d = c; c = (b<<30) | (b>>>2); b = a; a = t & 0xffffffff;
    }
    s[0] = (s[0] + a) & 0xffffffff;
    s[1] = (s[1] + b) & 0xffffffff;
    s[2] = (s[2] + c) & 0xffffffff;
    s[3] = (s[3] + d) & 0xffffffff;
    s[4] = (s[4] + e) & 0xffffffff;
    return s;
  }
 
  /* compute the intermediate SHA1 state of the inner and outer parts
     of the HMAC algorithm after processing the padded HMAC key */
  var hmac_istate = initsha(stringtowords(pass, 0), 0x36);
  var hmac_ostate = initsha(stringtowords(pass, 0), 0x5c);
 
  /* output is created in blocks of 20 bytes at a time and collected
     in a string as hexadecimal digits */
  var hash = '';
  var i = 0;
  while (hash.length < 64) {
    /* prepare 20-byte (5-word) output vector */
    var u = [ 0, 0, 0, 0, 0 ];
    /* prepare input vector for the first SHA1 update (salt + block number) */
    i++;
    var w = stringtowords(salt, i);
    /* iterate 4096 times an inner and an outer SHA1 operation */
    for (var j = 0; j < 2 * 4096; j++) {
      /* alternate inner and outer SHA1 operations */
      var s = (j & 1) ? hmac_ostate : hmac_istate;
      /* inline the SHA1 update operation */
      var a = s[0], b = s[1], c = s[2], d = s[3], e = s[4];
      var t;
      for (var k = 16; k < 80; k++) {
        t = w[k-3] ^ w[k-8] ^ w[k-14] ^ w[k-16];
        w[k] = (t<<1) | (t>>>31);
      }
      for (var k = 0; k < 20; k++) {
        t = ((a<<5) | (a>>>27)) + e + w[k] + 0x5A827999 + ((b&c)|((~b)&d));
        e = d; d = c; c = (b<<30) | (b>>>2); b = a; a = t & 0xffffffff;
      }
      for (var k = 20; k < 40; k++) {
        t = ((a<<5) | (a>>>27)) + e + w[k] + 0x6ED9EBA1 + (b^c^d);
        e = d; d = c; c = (b<<30) | (b>>>2); b = a; a = t & 0xffffffff;
      }
      for (var k = 40; k < 60; k++) {
        t = ((a<<5) | (a>>>27)) + e + w[k] + 0x8F1BBCDC + ((b&c)|(b&d)|(c&d));
        e = d; d = c; c = (b<<30) | (b>>>2); b = a; a = t & 0xffffffff;
      }
      for (var k = 60; k < 80; k++) {
        t = ((a<<5) | (a>>>27)) + e + w[k] + 0xCA62C1D6 + (b^c^d);
        e = d; d = c; c = (b<<30) | (b>>>2); b = a; a = t & 0xffffffff;
      }
      /* stuff the SHA1 output back into the input vector */
      w[0] = (s[0] + a) & 0xffffffff;
      w[1] = (s[1] + b) & 0xffffffff;
      w[2] = (s[2] + c) & 0xffffffff;
      w[3] = (s[3] + d) & 0xffffffff;
      w[4] = (s[4] + e) & 0xffffffff;
      if (j & 1) {
        /* XOR the result of each complete HMAC-SHA1 operation into u */
	u[0] ^= w[0]; u[1] ^= w[1]; u[2] ^= w[2]; u[3] ^= w[3]; u[4] ^= w[4];
      } else if (j == 0) {
        /* pad the new 20-byte input vector for subsequent SHA1 operations */
	w[5] = 0x80000000;
	for (var k = 6; k < 15; k++) w[k] = 0;
	w[15] = 8 * (64 + 20);
      }
    }
    /* convert output vector u to hex and append to output string */
    for (var j = 0; j < 5; j++)
      for (var k = 0; k < 8; k++) {
        var t = (u[j] >>> (28 - 4 * k)) & 0x0f;
	hash += (t < 10) ? t : String.fromCharCode(87 + t);
      }
  }
 
  /* return the first 32 key bytes as a hexadecimal string */
  return hash.substring(0, 64);
}


function getWepKeyFromPassphrase(pass){
   function array(n) {
     for(i=0;i<n;i++) this[i]=0;
     this.length=n;
   }
    
   function integer(n) { return n%(0xffffffff+1); }
    
   function shr(a,b) {
     a=integer(a);
     b=integer(b);
     if (a-0x80000000>=0) {
       a=a%0x80000000;
       a>>=b;
       a+=0x40000000>>(b-1);
     } else
       a>>=b;
     return a;
   }
    
   function shl1(a) {
     a=a%0x80000000;
     if (a&0x40000000==0x40000000)
     {
       a-=0x40000000;  
       a*=2;
       a+=0x80000000;
     } else
       a*=2;
     return a;
   }
    
   function shl(a,b) {
     a=integer(a);
     b=integer(b);
     for (var i=0;i<b;i++) a=shl1(a);
     return a;
   }
    
   function and(a,b) {
     a=integer(a);
     b=integer(b);
     var t1=(a-0x80000000);
     var t2=(b-0x80000000);
     if (t1>=0) 
       if (t2>=0) 
         return ((t1&t2)+0x80000000);
       else
         return (t1&b);
     else
       if (t2>=0)
         return (a&t2);
       else
         return (a&b);  
   }
    
   function or(a,b) {
     a=integer(a);
     b=integer(b);
     var t1=(a-0x80000000);
     var t2=(b-0x80000000);
     if (t1>=0) 
       if (t2>=0) 
         return ((t1|t2)+0x80000000);
       else
         return ((t1|b)+0x80000000);
     else
       if (t2>=0)
         return ((a|t2)+0x80000000);
       else
         return (a|b);  
   }
    
   function xor(a,b) {
     a=integer(a);
     b=integer(b);
     var t1=(a-0x80000000);
     var t2=(b-0x80000000);
     if (t1>=0) 
       if (t2>=0) 
         return (t1^t2);
       else
         return ((t1^b)+0x80000000);
     else
       if (t2>=0)
         return ((a^t2)+0x80000000);
       else
         return (a^b);  
   }
    
   function not(a) {
     a=integer(a);
     return (0xffffffff-a);
   }
    
       var state = new array(4); 
       var count = new array(2);
   	count[0] = 0;
   	count[1] = 0;                     
       var buffer = new array(64); 
       var transformBuffer = new array(16); 
       var digestBits = new array(16);
    
       var S11 = 7;
       var S12 = 12;
       var S13 = 17;
       var S14 = 22;
       var S21 = 5;
       var S22 = 9;
       var S23 = 14;
       var S24 = 20;
       var S31 = 4;
       var S32 = 11;
       var S33 = 16;
       var S34 = 23;
       var S41 = 6;
       var S42 = 10;
       var S43 = 15;
       var S44 = 21;
    
   function F(x,y,z) {
   	return or(and(x,y),and(not(x),z));
   }
    
   function G(x,y,z) {
   	return or(and(x,z),and(y,not(z)));
   }
    
   function H(x,y,z) {
   	return xor(xor(x,y),z);
   }
    
   function I(x,y,z) {
   	return xor(y ,or(x , not(z)));
   }
    
   function rotateLeft(a,n) {
   	return or(shl(a, n),(shr(a,(32 - n))));
   }
    
   function FF(a,b,c,d,x,s,ac) {
       a = a+F(b, c, d) + x + ac;
   	a = rotateLeft(a, s);
   	a = a+b;
   	return a;
   }
    
   function GG(a,b,c,d,x,s,ac) {
   	a = a+G(b, c, d) +x + ac;
   	a = rotateLeft(a, s);
   	a = a+b;
   	return a;
   }
    
   function HH(a,b,c,d,x,s,ac) {
   	a = a+H(b, c, d) + x + ac;
   	a = rotateLeft(a, s);
   	a = a+b;
   	return a;
   }
    
   function II(a,b,c,d,x,s,ac) {
   	a = a+I(b, c, d) + x + ac;
   	a = rotateLeft(a, s);
   	a = a+b;
   	return a;
   }
    
   function transform(buf,offset) { 
   	var a=0, b=0, c=0, d=0; 
   	var x = transformBuffer;
   	
   	a = state[0];
   	b = state[1];
   	c = state[2];
   	d = state[3];
   	
   	for (i = 0; i < 16; i++) {
   	    x[i] = and(buf[i*4+offset],0xff);
   	    for (j = 1; j < 4; j++) {
   		x[i]+=shl(and(buf[i*4+j+offset] ,0xff), j * 8);
   	    }
   	}
    
   	/* Round 1 */
   	a = FF ( a, b, c, d, x[ 0], S11, 0xd76aa478); /* 1 */
   	d = FF ( d, a, b, c, x[ 1], S12, 0xe8c7b756); /* 2 */
   	c = FF ( c, d, a, b, x[ 2], S13, 0x242070db); /* 3 */
   	b = FF ( b, c, d, a, x[ 3], S14, 0xc1bdceee); /* 4 */
   	a = FF ( a, b, c, d, x[ 4], S11, 0xf57c0faf); /* 5 */
   	d = FF ( d, a, b, c, x[ 5], S12, 0x4787c62a); /* 6 */
   	c = FF ( c, d, a, b, x[ 6], S13, 0xa8304613); /* 7 */
   	b = FF ( b, c, d, a, x[ 7], S14, 0xfd469501); /* 8 */
   	a = FF ( a, b, c, d, x[ 8], S11, 0x698098d8); /* 9 */
   	d = FF ( d, a, b, c, x[ 9], S12, 0x8b44f7af); /* 10 */
   	c = FF ( c, d, a, b, x[10], S13, 0xffff5bb1); /* 11 */
   	b = FF ( b, c, d, a, x[11], S14, 0x895cd7be); /* 12 */
   	a = FF ( a, b, c, d, x[12], S11, 0x6b901122); /* 13 */
   	d = FF ( d, a, b, c, x[13], S12, 0xfd987193); /* 14 */
   	c = FF ( c, d, a, b, x[14], S13, 0xa679438e); /* 15 */
   	b = FF ( b, c, d, a, x[15], S14, 0x49b40821); /* 16 */
    
   	/* Round 2 */
   	a = GG ( a, b, c, d, x[ 1], S21, 0xf61e2562); /* 17 */
   	d = GG ( d, a, b, c, x[ 6], S22, 0xc040b340); /* 18 */
   	c = GG ( c, d, a, b, x[11], S23, 0x265e5a51); /* 19 */
   	b = GG ( b, c, d, a, x[ 0], S24, 0xe9b6c7aa); /* 20 */
   	a = GG ( a, b, c, d, x[ 5], S21, 0xd62f105d); /* 21 */
   	d = GG ( d, a, b, c, x[10], S22,  0x2441453); /* 22 */
   	c = GG ( c, d, a, b, x[15], S23, 0xd8a1e681); /* 23 */
   	b = GG ( b, c, d, a, x[ 4], S24, 0xe7d3fbc8); /* 24 */
   	a = GG ( a, b, c, d, x[ 9], S21, 0x21e1cde6); /* 25 */
   	d = GG ( d, a, b, c, x[14], S22, 0xc33707d6); /* 26 */
   	c = GG ( c, d, a, b, x[ 3], S23, 0xf4d50d87); /* 27 */
   	b = GG ( b, c, d, a, x[ 8], S24, 0x455a14ed); /* 28 */
   	a = GG ( a, b, c, d, x[13], S21, 0xa9e3e905); /* 29 */
   	d = GG ( d, a, b, c, x[ 2], S22, 0xfcefa3f8); /* 30 */
   	c = GG ( c, d, a, b, x[ 7], S23, 0x676f02d9); /* 31 */
   	b = GG ( b, c, d, a, x[12], S24, 0x8d2a4c8a); /* 32 */
    
   	/* Round 3 */
   	a = HH ( a, b, c, d, x[ 5], S31, 0xfffa3942); /* 33 */
   	d = HH ( d, a, b, c, x[ 8], S32, 0x8771f681); /* 34 */
   	c = HH ( c, d, a, b, x[11], S33, 0x6d9d6122); /* 35 */
   	b = HH ( b, c, d, a, x[14], S34, 0xfde5380c); /* 36 */
   	a = HH ( a, b, c, d, x[ 1], S31, 0xa4beea44); /* 37 */
   	d = HH ( d, a, b, c, x[ 4], S32, 0x4bdecfa9); /* 38 */
   	c = HH ( c, d, a, b, x[ 7], S33, 0xf6bb4b60); /* 39 */
   	b = HH ( b, c, d, a, x[10], S34, 0xbebfbc70); /* 40 */
   	a = HH ( a, b, c, d, x[13], S31, 0x289b7ec6); /* 41 */
   	d = HH ( d, a, b, c, x[ 0], S32, 0xeaa127fa); /* 42 */
   	c = HH ( c, d, a, b, x[ 3], S33, 0xd4ef3085); /* 43 */
   	b = HH ( b, c, d, a, x[ 6], S34,  0x4881d05); /* 44 */
   	a = HH ( a, b, c, d, x[ 9], S31, 0xd9d4d039); /* 45 */
   	d = HH ( d, a, b, c, x[12], S32, 0xe6db99e5); /* 46 */
   	c = HH ( c, d, a, b, x[15], S33, 0x1fa27cf8); /* 47 */
   	b = HH ( b, c, d, a, x[ 2], S34, 0xc4ac5665); /* 48 */
    
   	/* Round 4 */
   	a = II ( a, b, c, d, x[ 0], S41, 0xf4292244); /* 49 */
   	d = II ( d, a, b, c, x[ 7], S42, 0x432aff97); /* 50 */
   	c = II ( c, d, a, b, x[14], S43, 0xab9423a7); /* 51 */
   	b = II ( b, c, d, a, x[ 5], S44, 0xfc93a039); /* 52 */
   	a = II ( a, b, c, d, x[12], S41, 0x655b59c3); /* 53 */
   	d = II ( d, a, b, c, x[ 3], S42, 0x8f0ccc92); /* 54 */
   	c = II ( c, d, a, b, x[10], S43, 0xffeff47d); /* 55 */
   	b = II ( b, c, d, a, x[ 1], S44, 0x85845dd1); /* 56 */
   	a = II ( a, b, c, d, x[ 8], S41, 0x6fa87e4f); /* 57 */
   	d = II ( d, a, b, c, x[15], S42, 0xfe2ce6e0); /* 58 */
   	c = II ( c, d, a, b, x[ 6], S43, 0xa3014314); /* 59 */
   	b = II ( b, c, d, a, x[13], S44, 0x4e0811a1); /* 60 */
   	a = II ( a, b, c, d, x[ 4], S41, 0xf7537e82); /* 61 */
   	d = II ( d, a, b, c, x[11], S42, 0xbd3af235); /* 62 */
   	c = II ( c, d, a, b, x[ 2], S43, 0x2ad7d2bb); /* 63 */
   	b = II ( b, c, d, a, x[ 9], S44, 0xeb86d391); /* 64 */
    
   	state[0] +=a;
   	state[1] +=b;
   	state[2] +=c;
   	state[3] +=d;
    
   }
    
   function init() {
   	count[0]=count[1] = 0;
   	state[0] = 0x67452301;
   	state[1] = 0xefcdab89;
   	state[2] = 0x98badcfe;
   	state[3] = 0x10325476;
   	for (i = 0; i < digestBits.length; i++)
   	    digestBits[i] = 0;
   }
    
   function update(b) { 
   	var index,i;
   	
   	index = and(shr(count[0],3) , 0x3f);
   	if (count[0]<0xffffffff-7) 
   	  count[0] += 8;
           else {
   	  count[1]++;
   	  count[0]-=0xffffffff+1;
             count[0]+=8;
           }
   	buffer[index] = and(b,0xff);
   	if (index  >= 63) {
   	    transform(buffer, 0);
   	}
   }
    
   function finish() {
   	var bits = new array(8);
   	var	padding; 
   	var	i=0, index=0, padLen=0;
    
   	for (i = 0; i < 4; i++) {
   	    bits[i] = and(shr(count[0],(i * 8)), 0xff);
   	}
           for (i = 0; i < 4; i++) {
   	    bits[i+4]=and(shr(count[1],(i * 8)), 0xff);
   	}
   	index = and(shr(count[0], 3) ,0x3f);
   	padLen = (index < 56) ? (56 - index) : (120 - index);
   	padding = new array(64); 
   	padding[0] = 0x80;
           for (i=0;i<padLen;i++)
   	  update(padding[i]);
           for (i=0;i<8;i++) 
   	  update(bits[i]);
    
   	for (i = 0; i < 4; i++) {
   	    for (j = 0; j < 4; j++) {
   		digestBits[i*4+j] = and(shr(state[i], (j * 8)) , 0xff);
   	    }
   	} 
   }
    
   function hexa(n) {
   	var hexa_h = "0123456789abcdef";
   	var hexa_c=""; 
   	var hexa_m=n;
    
   	for (hexa_i=0;hexa_i<8;hexa_i++) {
   		hexa_c=hexa_h.charAt(Math.abs(hexa_m)%16)+hexa_c;
   		hexa_m=Math.floor(hexa_m/16);
   	}
   	return hexa_c;
   }
    
    
   var ascii="01234567890123456789012345678901" +
             " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ"+
             "[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~";
    
   function MD5(entree) 
   {
   	var l,s,k,ka,kb,kc,kd;
    
   	init();
   	for (k=0;k<entree.length;k++) {
   		l=entree.charAt(k);
   		update(ascii.lastIndexOf(l));
   	}
   	finish();
   	ka=kb=kc=kd=0;
   	for (i=0;i<4;i++) ka+=shl(digestBits[15-i], (i*8));
   	for (i=4;i<8;i++) kb+=shl(digestBits[15-i], ((i-4)*8));
   	for (i=8;i<12;i++) kc+=shl(digestBits[15-i], ((i-8)*8));
   	for (i=12;i<16;i++) kd+=shl(digestBits[15-i], ((i-12)*8));
   	s=hexa(kd)+hexa(kc)+hexa(kb)+hexa(ka);
   	return s; 
   }
    
   function ToHex(i)
   {
   	h="0123456789abcdef";
   	return h.charAt((i>>4)&0x0f)+h.charAt(i&0x0f);
   }
    
   function MM_findObj(n, d)
   {
   	var p,i,x;
   	  
   	if(!d) d=document;
   	if((p=n.indexOf("?"))>0&&parent.frames.length) {
   		d=parent.frames[n.substring(p+1)].document;
   		n=n.substring(0,p);
   	}
   	if(!(x=d[n])&&d.all) x=d.all[n];
   	for (i=0;!x&&i<d.forms.length;i++) x=d.forms[i][n];
   	for(i=0;!x&&d.layers&&i<d.layers.length;i++) x=MM_findObj(n,d.layers[i].document);
    
   	return x;
   }
    
   var passphrase;
   var key64 = new Array(4);
   var key128;

   //start
   l=pass.length;
	seed="";
	for (i=0;i<64;i++) seed+=pass.charAt(i%l);
	key128=MD5(seed).slice(0,26);
	
	rand=0;
	for (i=0;i<l;i++) rand^=(pass.charCodeAt(i)<<((i%4)*8));

	for (i=0;i<4;i++) {
		s=""
		for (j=0;j<5;j++) {
			rand*=0x343fd;
			rand+=0x269ec3;
			rand&=0xffffffff;
			s+=ToHex(rand>>16);
		}
		key64[i]=s;
	}

   //alert("64_0:"+key64[0]+"64_1:"+key64[1]+"64_2:"+key64[2]+"64_3:"+key64[3]+"128_0:"+key128);

   var ret_value = new Array(5);
   ret_value[0] = key64[0];
   ret_value[1] = key64[1];
   ret_value[2] = key64[2];
   ret_value[3] = key64[3];
   ret_value[4] = key128;

   return ret_value;
 
}
//end, __MSTC__, Paul Ho, end of OBM, 

// __MSTC__, __Telus__, Axel, support Multilingal
function MLG_Makestring(str, var1)
{
	return str.replace("{{1}}", var1);
}

function MLG_Makestring(str, var1, var2)
{
	var tmp = str.replace("{{1}}", var1);
	return tmp.replace("{{2}}", var2);
}

function convertHtmlEntitiesToChar(convertStr)
{
	var newDiv = document.createElement(newDiv);
	newDiv.innerHTML = convertStr;
	return newDiv.innerHTML;
}
// __MSTC__, __Telus__, Axel, support Multilingal
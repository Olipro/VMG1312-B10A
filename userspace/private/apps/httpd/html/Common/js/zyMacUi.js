function disableEnterKey(e){
     var key;
     if(window.event)
          key = window.event.keyCode;     //IE
     else
          key = e.which;     //firefox
     if(key == 13)
          return false;
     else
          return true;
}

var KeyPressIndexStart = "0";
var KeyPressIndexEnd = "0";
function doKeyDownEvent(e){
   var key;
   var srcElement;
   if(window.event){
       //IE
       srcElement = window.event.srcElement;
   }
   else{
       //firefox
       srcElement = e.target;
   }
   
   KeyPressIndexStart = getSelectionStart(srcElement);
   KeyPressIndexEnd = getSelectionEnd(srcElement);
}

var nextfield = "str1";
var prevfield = "str1";
var macStrLength = 2;
function doMacStrKeyUp(e){
   with ( document.forms[0] ) {

      var key;
      var srcElement;
      if(window.event){
          key = window.event.keyCode;     //IE
          srcElement = window.event.srcElement;
      }
      else{
          key = e.which;     //firefox
          srcElement = e.target;
      }

      if(key == 37 || key == 38 || key == 39 || key ==40){
         //left, up, right, down
         
         if(key == 37 && getSelectionStart(srcElement) == 0 && KeyPressIndexEnd == 0 && KeyPressIndexStart == 0){
            var element=document.getElementsByName(prevfield)[0];
            eval(prevfield + '.focus();');

            
            if(srcElement.name != "str1"){
               setSelectionStart(element, element.value.length);
            }
         }else if(key == 39 && getSelectionStart(srcElement) == srcElement.value.length && KeyPressIndexStart ==  srcElement.value.length){
            var element=document.getElementsByName(nextfield)[0];
            eval(nextfield + '.focus();');
         }

         
         return true;
      }else if(key == 13){
         //enter
         
         var element=document.getElementsByName(nextfield)[0];
         eval(nextfield + '.focus();');
         element.select();
         return true;
      }else if(key == 46){
         //delete

         //do nothing
      }else if(key == 35 || key == 36){
         //end, home

         //do nothing
      }else if(srcElement.value.length == macStrLength && getSelectionStart(srcElement) == srcElement.value.length){
         //others, length = 2

         var element=document.getElementsByName(nextfield)[0];
         eval(nextfield + '.focus();');
         element.select();
         return true;
          
     }else if(srcElement.value.length == 0 && key == 8 && KeyPressIndexStart == 0){
         //backspace, length = 0
         
         var element=document.getElementsByName(prevfield)[0];
         eval(prevfield + '.focus();');
         element.select();
         return true;
         
     }else{
         return true;
     }

     
  }
}

function getSelectionStart(o) {
	if (o.createTextRange) {
		var r = document.selection.createRange().duplicate()
		r.moveEnd('character', o.value.length)
		if (r.text == '') return o.value.length
		return o.value.lastIndexOf(r.text)
	} else return o.selectionStart
}

function getSelectionEnd(o) {
	if (o.createTextRange) {
		var r = document.selection.createRange().duplicate()
		r.moveStart('character', -o.value.length)
		return r.text.length
	} else return o.selectionEnd
}

function setSelectionStart(o, pos){
   if(o.createTextRange) {
      var range = o.createTextRange();
      range.move('character', pos);
      range.select();
   }
   else {
      if(o.selectionStart) {
          o.focus();
          o.setSelectionRange(pos, pos);
      }
      else
          o.focus();
   }
}
function disableEnterKey(e){
     var key;
     if(window.event)
          key = window.event.keyCode;     //IE
     else
          key = e.which;     //firefox
     if(key == 13)
          return false;
     else
          return true;
}

var KeyPressIndexStart = "0";
var KeyPressIndexEnd = "0";
function doKeyDownEvent(e){
   var key;
   var srcElement;
   if(window.event){
       //IE
       srcElement = window.event.srcElement;
   }
   else{
       //firefox
       srcElement = e.target;
   }
   
   KeyPressIndexStart = getSelectionStart(srcElement);
   KeyPressIndexEnd = getSelectionEnd(srcElement);
}

var nextfield = "str1";
var prevfield = "str1";
var macStrLength = 2;
function doMacStrKeyUp(e){
   with ( document.forms[0] ) {

      var key;
      var srcElement;
      if(window.event){
          key = window.event.keyCode;     //IE
          srcElement = window.event.srcElement;
      }
      else{
          key = e.which;     //firefox
          srcElement = e.target;
      }

      if(key == 37 || key == 38 || key == 39 || key ==40){
         //left, up, right, down
         
         if(key == 37 && getSelectionStart(srcElement) == 0 && KeyPressIndexEnd == 0 && KeyPressIndexStart == 0){
            var element=document.getElementsByName(prevfield)[0];
            eval(prevfield + '.focus();');

            
            if(srcElement.name != "str1"){
               setSelectionStart(element, element.value.length);
            }
         }else if(key == 39 && getSelectionStart(srcElement) == srcElement.value.length && KeyPressIndexStart ==  srcElement.value.length){
            var element=document.getElementsByName(nextfield)[0];
            eval(nextfield + '.focus();');
         }

         
         return true;
      }else if(key == 13){
         //enter
         
         var element=document.getElementsByName(nextfield)[0];
         eval(nextfield + '.focus();');
         element.select();
         return true;
      }else if(key == 46){
         //delete

         //do nothing
      }else if(key == 35 || key == 36){
         //end, home

         //do nothing
      }else if(srcElement.value.length == macStrLength && getSelectionStart(srcElement) == srcElement.value.length){
         //others, length = 2

         var element=document.getElementsByName(nextfield)[0];
         eval(nextfield + '.focus();');
         element.select();
         return true;
          
     }else if(srcElement.value.length == 0 && key == 8 && KeyPressIndexStart == 0){
         //backspace, length = 0
         
         var element=document.getElementsByName(prevfield)[0];
         eval(prevfield + '.focus();');
         element.select();
         return true;
         
     }else{
         return true;
     }

     
  }
}

function getSelectionStart(o) {
	if (o.createTextRange) {
		var r = document.selection.createRange().duplicate()
		r.moveEnd('character', o.value.length)
		if (r.text == '') return o.value.length
		return o.value.lastIndexOf(r.text)
	} else return o.selectionStart
}

function getSelectionEnd(o) {
	if (o.createTextRange) {
		var r = document.selection.createRange().duplicate()
		r.moveStart('character', -o.value.length)
		return r.text.length
	} else return o.selectionEnd
}

function setSelectionStart(o, pos){
   if(o.createTextRange) {
      var range = o.createTextRange();
      range.move('character', pos);
      range.select();
   }
   else {
      if(o.selectionStart) {
          o.focus();
          o.setSelectionRange(pos, pos);
      }
      else
          o.focus();
   }
}

var defaultURL = "pages/connectionStatus/naviView_partialLoad.html"
var sysVar = {
    'model-name': 'DSL-491HNU-B1B v2'
}

function replaceSysVar(){
    $('[sysVar]').each(function(){
        var key = $(this).attr('sysVar');
        $(this).html(sysVar[key]);
    });
}

function hciAlert(str, param){//param={height: 180,type: 0, title: ''}   type: 0/message; 1/warning; 2/error
    return window.parent.$.hciAlert(str,param);
}

function setPageIndex(menuIndex){//ex:security-parentalControl
window.parent.setPageIndex(menuIndex);
}

function setTabIndex(tabIndex){
window.parent.setTabIndex(tabIndex);
}

function getPageIndex(){
return window.parent.actionItemID;
}

function getTabIndex(){
return window.parent.tabIndex;
}

//close all the dialog
    var dialogGroup = window.parent.$('.dialogContener');
       $.each(dialogGroup, function(i, d){
	   var dbObj = window.parent.$(d)
	   if(dbObj.attr('autoClose')!='false')
	     window.parent.$(d).dialog('close');
    });
    
    
/*
if (window.parent.length>0&&!window.parent.isPressMenu) {
 
    var dialogGroup = window.parent.$('.dialogContener');
    if (dialogGroup.length != 0) 
        dialogGroup.dialog('close');
    
    var pathAry = window.location.href.split('/');
    var targetItem = [];
    
    var jsonData =window.parent.parseObj.getJsonData();
     var jsonItem;
    for (var i = 0; i < pathAry.length; i++) {
        if (pathAry[i] == 'pages') {
            if (pathAry[i + 1] == 'tabFW') {
                targetItem.push(pathAry[i + 3]);
                jsonItem = jsonData[pathAry[i + 3]];
            }
            else {
                targetItem.push(pathAry[i + 1]);
                jsonItem = jsonData[pathAry[i + 1]];
            }
        }
    }

      $.each(jsonData, function(i, d){
           if(i=='defaultPage')
           return true;
           var obj =  window.parent.$('#' + i);
           if (d.switchCls != undefined) {
               var onCls = d.switchCls.on;
               var offCls = d.switchCls.off;
               obj.removeClass(onCls).addClass(offCls);
           }
       });
       
        if(jsonItem.switchCls!=undefined)
        window.parent.$('#' + targetItem[0]).removeClass(jsonItem.switchCls.off).addClass(jsonItem.switchCls.on);
        
        //restore actionItemID
        if (jsonData[targetItem[0]]!=undefined && jsonData[targetItem[0]].submenu == undefined) 
            window.parent.actionItemID = targetItem[0];
        else if(jsonData[targetItem[0]]!=undefined ){
            $.each(jsonData[targetItem[0]].submenu, function(i, d){
               if (window.location.href.indexOf(d.url) != -1) {
                  window.parent.actionItemID = targetItem[0]+'-'+ i;
                  return false;
               } 
            });
        } 
}
*/
 

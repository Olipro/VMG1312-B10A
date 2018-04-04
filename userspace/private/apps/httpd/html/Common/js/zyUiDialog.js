
/*this plugin depends on
 * ui.draggable.js
 * ui.resizable.js
 * ui.dialog.js
 * jquery.bgiframe.js
 * effects.core.js
 * 
 * 090828
 * 090903 refine code, add 'resizeConfirmDB' and 'contenerCss' option
 * 090916 cancel event bubble to parent element
 * 090923 cancel event bubble from overlay to parent element
 * 091224 add css "position: relative" in container
 * 100528 add multi lingo config and addLingoAttr function
 * 100624 set default width and height
 * 100707 add naviTo function
 * 100712 auto modify height in ie6
 * 100804 add cancel Bubble.
 * 101119 add btnAttr in each dialog button
 * 101122 add getBtnByAttr, getBtnByText, getBtnByIndex
 * 110111 add autoClose attribute
 */

(function($){

 $.cancelBubble = function(evt){
        //cancel bubble event
        if (window.event) //for IE       
            window.event.cancelBubble = true;
        else 
            //for Firefox        
            evt.stopPropagation();
    };
	
    function applyDefault(baseDiv, settings){
        var _defaultSettings = {
            bgiframe: true,
            closeOnEscape: true,
            modal: true,
			width: 450,
			height: 300,
            autoOpen: false,
			autoClose: true,
			multiLingo: true,
            confirmDB: false, //no background, unresizable
            resizeConfirmDB: false, //no background, resizable
            close: function(){},
            open: function(e){
                $(e.target).parents('.ui-dialog').click(function(e){
                    $.cancelBubble(e);
                });
                 $('.ui-widget-overlay').click(function(e){
                    $.cancelBubble(e);
                });
            },
            containerCss: {
                'overflow-x': 'hidden',
                'margin-left': '4px',
                'margin-right': '4px', 
                'position':'relative',
				'background':'#fff'
            },
            buttons: {    
                'Cancel': function(e){$.cancelBubble(e);$(this).dialog('close');},
				'OK': function(){}
            },
            naviTo: function(){alert('');}
      
        };
        
        if (baseDiv != null) {
            baseDiv.bind('dialogclose', function(event, ui){
               setTimeout(function(){baseDiv.remove();},300);
               //baseDiv.remove();
            });
        }
        
		if($.browser.msie&&($.browser.version == "6.0")&&!$.support.style )
		{
	    //var ie6Setting = {  'margin-right': '18px' };
		//_defaultSettings.containerCss = $.extend(_defaultSettings.containerCss, ie6Setting);
		}
		
        return $.extend(_defaultSettings, settings);
    }
    
     function createContainer(settings){
            var msgDiv = $('<div class="dialogContener">');
            var _settings = applyDefault(msgDiv, settings);
            
            if(!_settings.resizeConfirmDB&&!_settings.confirmDB)
                 msgDiv.css(_settings.containerCss);
            
			msgDiv.attr('autoClose', _settings.autoClose);
			
            if (_settings.confirmDB) 
                 _settings.resizable = false;
                
                if ($.browser.msie && ($.browser.version == "6.0") && !$.support.style) {
                    var dheight = _settings.height;
                    _settings = $.extend(_settings, {height:dheight + 1});
                }
                
                return {container: msgDiv, setting:_settings };
    }
    
	function addLingoAttr(retMsg){
		var dbParent = retMsg.parent('.ui-dialog');
			dbParent.find('.ui-dialog-buttonpane button').attr('lingo','auto');
			dbParent.find('.ui-dialog-title').attr('lingo','auto');
	}
    
    $.extend({
        zyUiDialog: function(settings){
            var msg = createContainer(settings);
			var retMsg = msg.container.dialog(msg.setting);
            
			 //.naviTo = function(){alert('n')};
             
			if(msg.setting.multiLingo)
			addLingoAttr(retMsg);
			
			return retMsg
        }
    });
    
   
    $.fn.extend({
        zyUiDialog: function(settings){
            var msg = createContainer(settings);
		    var retMsg = msg.container.append($(this)).dialog(msg.setting);
			this.naviTo = function(){alert('n')}
          if(msg.setting.multiLingo)
			addLingoAttr(retMsg);
			
           return retMsg
        }
    });
    
    $.fn.extend({
        naviTo: function(url, options, callback){
            if (!url) 
                return;
            if(options!=undefined)
              $(this).dialog("option", options);
                
            $(this).empty().load(url, callback);
            return this;
        },
		setBtnAttr:function(ary){
		    if(ary != undefined)
			{
			    $(this).parents('.ui-dialog').find('.ui-dialog-buttonpane').children('button').each(function(i,d){
					$(d).attr('btnAttr', ary[i]);
				});
			}
			return this;
		},
        getBtnByAttr: function(attr){
            return  $(this).parents('.ui-dialog').find('.ui-dialog-buttonpane').children('button[btnAttr='+attr+']');
        },
        getBtnByText: function(text){
            return  $(this).parents('.ui-dialog').find('.ui-dialog-buttonpane').children('button:contains('+text+')');
        },
        getBtnByIndex:function(index){
            return  $(this).parents('.ui-dialog').find('.ui-dialog-buttonpane').children('button:nth-child('+ index +')');
        }
    });
})(jQuery);

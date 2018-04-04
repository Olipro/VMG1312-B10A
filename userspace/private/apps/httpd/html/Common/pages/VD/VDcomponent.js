/*
 * Copyright (c) 2010
 * Joze Chang @zyxel.com.tw
 *
 * Dual licensed under the GPL (http://www.gnu.org/licenses/gpl.html)
 * and MIT (http://www.opensource.org/licenses/mit-license.php) licenses.
 * 
 * 2009/10/22: add loaded and updated callback, add async property
 * 
 * 2010/03/23 
 * add tooltip plutin support. depend on jquery.tooltip.min.js (http://docs.jquery.com/Plugins/Tooltip/tooltip)
 * 
 * 2010/04/19
 * add error handle for unknow port
 * 
 * 2010/04/22
 * modify some code 
 * 
 * 2010/04/26
 * Add dataWrapper config for setting data wrapper callback function
 * 
 * 2010/05/25
 * set title to the item, even if it be off.
 * 
 * 2010/06/04 
 * set <br> to endline
 */
;
(function($){

    $.fn.VDcomponent = function(settings){
         var _tipOption = {
                showBody: "<br>",
                fade: 100,
               // opacity: 1,
                track: true,
                delay: 0, 
                showURL: false,
                fixPNG: false
            };
            
        var _defaultSetting = {            
            showSpeed: 300,
            easing: '',
            effect: 'none', //fadeIn, slideDown, show, none
 
            //path setting
            vdHtml: '',
            vdCss: '',   //ex: ['xxx.css', 'ooo.css']
            portDef: '',
            
            loaded: function(){},
            updated: function(){},
            async: false,
            dataWrapper: null
        }
        
        var _settings = $.extend(_defaultSetting, settings);
        
        var removePortCls = function($portObj, speed){
            var index = $portObj.attr('id');
            $.each(portDef[index].status, function(i, d){
                if (d != undefined) 
                    $portObj.removeClass(d);
            })
             $portObj.addClass('off');
        }
        var changePortStatus = function(obj, cls, title){

            if (!obj.hasClass(cls)) {
                removePortCls(obj, 0);
                
                if (!obj.hasClass(cls)&& cls!='') {
                  obj.removeClass('off').hide(0).addClass(cls);
                   if (_settings.effect == 'fadeIn'||_settings.effect == '' ) 
                        obj.fadeIn(_settings.showSpeed, _settings.easing);
                   else if (_settings.effect == 'show' ) 
                        obj.show(_settings.showSpeed, _settings.easing);
                   else if (_settings.effect == 'slideDown') 
                        obj.slideDown(_settings.showSpeed, _settings.easing);
                   else if (_settings.effect == 'none') 
                        obj.show(0);
                }
            }
            
            if (obj.attr('title') != title) 
                obj.attr('title', title);
        };
        var _data;
        this.setValue = function(data){
            if(_settings.dataWrapper!=null)
            data = _settings.dataWrapper(data);
            
            _data= data;
            $.each(data, function(i, date){
                $portObj = _vdObj.find('#' + i + '');
                
                 if ($portObj.length == 0) {
                    alert('undefined port: ' + i);
                    return false;
                }
                
                if (portDef[i] == undefined) {
                    alert('undefined port: ' + i);
                    return false;
                }
                /*
                if (portDef[i].status[data[i].status] == undefined) {
                    alert('port: ' + i + ' has undefined status:' + data[i].status);
                    return false;
                }*/
                
                if (data[i].status == undefined) {
                    removePortCls($portObj, _settings.hideSpeed);
                }
                else {
                    portCls = portDef[i].status[data[i].status];
                    portTitle = data[i].msg == undefined ? $portObj.attr('title') : data[i].msg;
                    changePortStatus($portObj, portCls, portTitle);
                }
                   if( $portObj.tooltip!=undefined)
                   $portObj.tooltip(_tipOption);
            })
            _settings.updated();
        };
        
        this.getValue = function(){return _data};
        
        var attachStylesheet = function(href){
            styleSheet = $('<link href="' + href + '" rel="stylesheet"/>')
            styleSheet.appendTo('head');
            return styleSheet.attr('type', "text/css");
        };
        
        var _vdObj = $(this);
        var portDef = '';
        var frameContents;
        var _handler = function(){
            
            if (_settings.vdHtml == '' || _settings.vdCss == '' || _settings.portDef == '') {
                alert('you shold add vdHtml, vdCss and portDef in config !')
                return;
            }
            
            $.ajaxSettings.async = _settings.async;
            
            $.getJSON(_settings.portDef, function(json){
                portDef = json;
                 _vdObj.load(_settings.vdHtml, function(){
               
                $.each(_settings.vdCss, function(i, d){
                    attachStylesheet(d);
                });
                    
                $.each(portDef, function(i, date){
                    $portObj = _vdObj.find('#' + i + '');
                  if ($portObj.length != 0) {
                      $portObj.attr('title', '');
                      removePortCls($portObj, 0);
                  }
                  else{
                     alert('undefined port: ' + i);
                  }
                })
                
                _settings.loaded();
                $.ajaxSettings.async = true;
            })
            
            });
        }
        return this.each(_handler);
    }
 
})(jQuery);

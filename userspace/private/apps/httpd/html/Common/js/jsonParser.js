/*
 * jsonParser.js
 *
 * Copyright (c) 2010 
 * Joze Chang
 *
 * Dual licensed under the GPL (http://www.gnu.org/licenses/gpl.html)
 * and MIT (http://www.opensource.org/licenses/mit-license.php) licenses.
 *
 * $Date: 2010-02-04 $
 * $Rev: 001 $
 * 
 */

(function($)
{
    $.jsonParser = function(settings){
     
       var defaultSetting = 
       {
           jsonUrl:'menu.json',
           menuId: 'menu'
           
       }
       var thisObj = $('<div></div>');
       var subObj = $('<div></div>');
           
       var jsonObj;
        settings = $.extend(defaultSetting , settings);
    
       var menuJsonData;
    
        function doSubMenu($liObj, item){
            var subMenuMark = $('<span class="subMenuMark" style="position: absolute;"></span>')
            $liObj.append(subMenuMark);
            var havorMark = $('<span class="havorMark" style="position: absolute;"></span>')
            $liObj.append(havorMark);
            
            var $menuContainer = $('<ul></ul>');
           // $menuContainer.css('display', 'none');
            $menuContainer.css('visibility', 'hidden');
           // $menuContainer.css('position', 'absolute');
            $menuContainer.addClass('subItems');
            var $targetID = $liObj.attr('id');
            $menuContainer.attr('targetID', $targetID);
            $menuContainer.append('<li class="submenuTop"></li>');
            $.each(item.submenu, function(i, item){
                var $menuItemA = $('<a></a>');
                $menuItemA.html(item.title);
                
                var $menuItemLi = $('<li></li>');
                $menuItemLi.attr('id', $targetID + '-' + i);
                $menuItemLi.addClass('subItem')
                if (item.switchCls != undefined) 
                    $menuItemLi.addClass(item.switchCls.off)
                
                
                $menuItemLi.append('<span class="arrow">&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</span>');
                $menuItemLi.append($menuItemA);
                
                $menuItemLi.hover(function(){
                    $(this).css('cursor', 'pointer').addClass('hover');
                }, function(){
                    $(this).removeClass('hover');
                }).click(function(){
                   // hideSubMenu($liObj, 0);
                    $liObj.trigger('itemLeave',$liObj);
                    $liObj.trigger('itemClick', $menuItemLi.attr('id'));
    
                });
                
                $menuContainer.append($menuItemLi);
                
            });
            
            $menuContainer.append('<li class="submenuBottom"></li>');
            subObj.append($menuContainer);
            
            
            $menuContainer.mouseenter(function(){
              // showSubMenu($liObj);
                   $liObj.trigger('itemEnter',$liObj);
            }).mouseleave(function(){
              //  hideSubMenu($liObj);
                     $liObj.trigger('itemLeave', $liObj);
            });
        }

            var defaultItem;
            var dIndex=0;
            this.getIndex = function(){
                return dIndex;
            }
            
            
            var completedFlag = false;
             var defaultPage; 
            
        var _handler = function(){
            var parseCounter = 0;
       
         
    $.getJSON("menu.json", function(data){
         var $menuContainer =  $('<ul style=""></ul>');
         $menuContainer.addClass('items');
         menuJsonData = data;
        
         $.each(data, function(i, item){   
    
         if(i=='defaultPage') {defaultPage = item;return; }//set default page
         
         var $menuItemA = $('<a></a>'); 
             $menuItemA.html(item.title);
         var $menuItemLi = $('<li></li>');
             $menuItemLi.attr('id', i);
             $menuItemLi.addClass('menuItem')
             if(item.switchCls!=undefined)
             $menuItemLi.addClass(item.switchCls.off)
 
  
         //has submenu
        if (item.submenu != undefined) {
             doSubMenu($menuItemLi, item);
             $menuItemLi.hover(function(){
         
                  $menuItemLi.trigger('itemEnter', $(this));

             }, function(){
                  $menuItemLi.trigger('itemLeave', $(this));

             });
         }

         if ($.browser.mozilla) 
             $menuItemLi.mouseenter(function(){
                 $(this).siblings('li.menuItem.hover').each(function(){
                           $(this).trigger('hideSubmenu',  $(this));
                 })
             });
                    
         if (item.url != undefined) {
             $menuItemLi.css('cursor', 'pointer').hover(function(){
                 $(this).addClass('hover');
             }, function(){
                 $(this).removeClass('hover');
             }).click(function(){
                 $(this).trigger('itemClick', $menuItemLi.attr('id'));
             });
             
         }
         
         //behavior

         $menuItemLi.append($menuItemA);
         $menuContainer.append($menuItemLi);
         
       });
    
        thisObj.append($menuContainer);
        
      // var lessSize = $menuContainer.children('li').length % scrollSize;
       /*var addNullLiSize = scrollSize - lessSize;
       addNullLiSize = addNullLiSize % scrollSize;
       for (var i = 0; i < addNullLiSize; i++) {
           var $tempLi = $('<li style="height:0px;"></li>');
           $menuContainer.append($tempLi);
       }*/
    
       completedFlag=true;
       $('body').trigger('parseCompleted');
       });
       
        
        };
      
       _handler();
       
       this.getCompletedFlag = function(){return completedFlag;};
       this.getHtmlWrap= function(){return thisObj.children();};
       this.getSubMenu= function(){return subObj.children();};
          this.getDefaultPage= function(){return defaultPage;};
        this.getJsonData= function(){return menuJsonData;};       
             

        return this;
    };
 

})(jQuery);



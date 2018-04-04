cancelBubble = function(evt){
    //cancel bubble event
    if (window.event) //for IE       
        window.event.cancelBubble = true;
    else 
        //for Firefox        
        evt.stopPropagation();
};

;(function($)//use $ to instead JQuery
{
    $.cancelBubble = function(evt){
        //cancel bubble event
        if (window.event) //for IE       
            window.event.cancelBubble = true;
        else 
            //for Firefox        
            evt.stopPropagation();
    };
    
    
    $.fn.disableA = function(){
        var _handler = function(n, v){
            //debugger;
            if ($(this).find('.aTagMask').length > 0) 
                return;
            $(this).removeAttr('href');
            var position = $(this).position();
            $(this).css('color', 'gray');
            var w = $(this).width();
            var h = $(this).height();
            var left = position.left;
            var top = position.top;
            var margin = $(this).css('margin');
            var padding = $(this).css('padding-top') + ' ' + $(this).css('padding-right') + ' ' + $(this).css('padding-bottom') + ' ' + $(this).css('padding-left');
            var mask = $('<div class="aTagMask"></div>').css({
                'padding': padding,
                'margin': margin,
                'position': 'absolute',
                left: left,
                top: top,
                width: w,
                height: h
                //,border: 'solid 1px red'
            });
            $(this).append(mask);
            mask.click(function(){
                return false;
            });
        }
        return this.each(_handler);
    }
    
    $.fn.enableA = function(){
        var _handler = function(n, v){
            $(this).attr('href', '#').css('color', '#000').find('.aTagMask').remove().empty();
        }
        return this.each(_handler);
    }
    
})(jQuery);

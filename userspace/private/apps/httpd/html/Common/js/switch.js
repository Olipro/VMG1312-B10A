$(function(){
    	
    	updateCtl = function(onCtl, offCtl){
    		if (onCtl.hasClass('.on')) {
    			onCtl.removeClass('on').addClass('on_on');
    			offCtl.removeClass('off_on').addClass('off');
    		}
    		else{
    			onCtl.removeClass('off').addClass('off_on');
    			offCtl.removeClass('on_on').addClass('on');
    		}
    	};
        
        //-files haring
        $('#Fon').click(function(){
            //$('#filesharing li:[class]').click(function(){
            if (this.className == 'on')  updateCtl($(this), $('#Foff'));
        });
        $('#Foff').click(function(){
            if (this.className == 'off') updateCtl($(this), $('#Fon'));
        });
		
		//-Media sharing
        $('#Mon').click(function(){
            if (this.className == 'on')  updateCtl($(this), $('#Moff'));
        });
        $('#Moff').click(function(){
            if (this.className == 'off') updateCtl($(this), $('#Mon'));
        });
    	
		//-Print server
        $('#Pon').click(function(){
            if (this.className == 'on')  updateCtl($(this), $('#Poff'));
        });
        $('#Poff').click(function(){
            if (this.className == 'off') updateCtl($(this), $('#Pon'));
        });
	});
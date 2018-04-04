$(function(){
    $("#wepmode").hide();
	$("#secmode").hide();
	$("#wepmore").hide();
	
	optionLength = 4;
    for (var x = 1; x <= optionLength - 1; x++) {
        $('#select_' + x).bind('selectMe', function(e){
            {
               // $(e.target).find('._off').fadeOut('fast');
                $(e.target).find('._on').slideDown('fast');
            }
        });
    }
    $('#select_1').bind('selectMe', function(e){
        $('#wepmode').fadeOut(0);
		$('#secmode').fadeOut(0);
    });
	$('#select_2').bind('selectMe', function(e){
        $('#wepmode').fadeIn('fast');
		$('#secmode').fadeOut(0);
    });
	$('#select_3').bind('selectMe', function(e){
        $('#wepmode').fadeOut(0);
		$('#secmode').fadeIn('fast');
    });
	
	//show/hide WEP more setting
	$('#btn_wepmore').click(function(){
       $('#wepmore').toggle(0);
       $('#wepdesc').toggle(0);
       
       if($(this).html()=='hide more')
       $(this).html('more...');
       else if ($(this).html()=='more...')
       $(this).html('hide more');
    });
	
	//show/hide WPA-PSK more setting
	$('#btn_wpapskmore').click(function(){
        $('#encryp').toggle(0);
		$('#timer').toggle(0);
        
       if($(this).html()=='hide more')
       $(this).html('more...');
       else if ($(this).html()=='more...')
       $(this).html('hide more');
    });
	
	//show/hide WPA-PSK more setting
	$('#btn_wpa2pskmore').click(function(){
        $('#pskcom').toggle(0);
		$('#encryp').toggle(0);
		$('#timer').toggle(0);
        
        
        if ($(this).html() == 'hide more') 
            $(this).html('more...');
        else if ($(this).html() == 'more...') 
            $(this).html('hide more');
    });
	
	//show/hide WPA more setting
	$('#btn_wpamore').click(function(){
		$('#encryp').toggle(0);
		$('#timer').toggle(0);
        
        
        if ($(this).html() == 'hide more') 
            $(this).html('more...');
        else if ($(this).html() == 'more...') 
            $(this).html('hide more');
		});
	
	//show/hide WPA2 more setting
	$('#btn_wpa2more').click(function(){
		$('#encryp').toggle(0);
		$('#timer').toggle(0);
		$('#wpacom').toggle(0);
		$('#wpa2').toggle(0);
        
        
        if ($(this).html() == 'hide more') 
            $(this).html('more...');
        else if ($(this).html() == 'more...') 
            $(this).html('hide more');
    });
	
	$("#selectSecmode").change(function(){
     $('.moreBtn').html('more...');

	  var secType = $(this).val();
	  if (secType == 1){
	    $("#pwd").fadeIn(0);
		$("#authserver").fadeOut(0);
		$("#btn_wpapskmore").fadeIn(0);
		$("#btn_wpa2pskmore").fadeOut(0);
		$('#pskcom').hide(0);
		$('#encryp').hide(0);
		$('#timer').hide(0);
		$('#wpacom').hide(0);
		$('#wpa2').hide(0);
	  }	else if (secType == 2){
	    $("#pwd").fadeIn(0);
		$("#authserver").fadeOut(0);
		$("#btn_wpapskmore").fadeOut(0);
		$("#btn_wpa2pskmore").fadeIn(0);
		$('#pskcom').hide(0);
		$('#encryp').hide(0);
		$('#timer').hide(0);
		$('#wpacom').hide(0);
		$('#wpa2').hide(0);
	  }	else if (secType == 3){
	    $("#pwd").fadeOut(0);
		$("#authserver").fadeIn(0);
		$("#btn_wpamore").fadeIn(0);
		$("#btn_wpa2more").fadeOut(0);
		$('#encryp').hide(0);
		$('#timer').hide(0);
		$('#wpacom').hide(0);
		$('#wpa2').hide(0);
	  }	else if (secType == 4){
	    $("#pwd").fadeOut(0);
		$("#authserver").fadeIn(0);
		$("#btn_wpamore").fadeOut(0);
		$("#btn_wpa2more").fadeIn(0);
		$('#encryp').hide(0);
		$('#timer').hide(0);
		$('#wpacom').hide(0);
		$('#wpa2').hide(0);
	  }
	});
    
    //hide all selections	
    function stopSilde(){
        for (var x = 1; x <= optionLength - 1; x++) {
            if (silder.value == x) 
                continue;
            $('#select_' + x).find('._on').attr('style', 'display:none;');
            $('#select_' + x).find('._off').removeAttr('style');
        }
        $('#error_mssage').fadeOut('fast');
    }
    
    var periousValue = 3;
    var silder;
    $('#select_3').trigger('selectMe');
    silder = $("#silder").slider({
        value: 3,
        animate: true,
        orientation: 'horizontal',
        min: 1,
        max: 3,
        //handle: $('.draw_btn'),
        step: 1,
      //modify 20100114 start
	    change: function(event, ui){
            if (periousValue != ui.value) {
                 stopSilde();
        $('#select_' + ui.value).trigger('selectMe');
                updateSliderHandlePQAAttr(ui.value);
                periousValue = ui.value;
     //modify 20100114 end
            }
        }
    });
	
        function updateSliderHandlePQAAttr(value){
        switch (value) {
            case 1:
                $('#silder .ui-slider-handle').attr('pqaattr', 'No Security');
                break;
            case 2:
                $('#silder .ui-slider-handle').attr('pqaattr', 'Basic');
                break;
            case 3:
                $('#silder .ui-slider-handle').attr('pqaattr', 'More Secure');
                break;
  
        }
    }
    
    updateSliderHandlePQAAttr(periousValue);
    
    $('li.sliderBlock').each(function(i, d){
        $(this).click(function(){
            $("#silder").slider('value', i + 1); 
        })
    })
});


$(function(){
    optionLength = 4;
    for (var x = 1; x <= optionLength - 1; x++) {
        $('#select_' + x).bind('selectMe', function(e){
            {;
                $(e.target).find('._off').fadeOut('fast');
                $(e.target).find('._on').fadeIn('fast');
            }
        });
    }
    $('#select_4').bind('selectMe', function(e){
        $('#error_mssage').fadeIn('fast');
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
    
    var periousValue = 2;
    var silder;
    $('#select_2').trigger('selectMe');
    silder = $("#silder").slider({
        value: 2,
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
                $('#silder .ui-slider-handle').attr('pqaattr', 'None');
                break;
            case 2:
                $('#silder .ui-slider-handle').attr('pqaattr', 'Medium');
                break;
            case 3:
                $('#silder .ui-slider-handle').attr('pqaattr', 'High');
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

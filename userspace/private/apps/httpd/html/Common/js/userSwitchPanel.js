$(function(){    
    
    $(".on_off ul li:first").click(function(){ 
        $(this).addClass("on_on").removeClass("on");
        $(".on_off").siblings('a').removeClass('transparent');
		$(".on_off li:last").addClass("off").removeClass("off_on");
	});

	$(".on_off li:last").click(function(){ 
        $(this).addClass("off_on").removeClass("off");
        $(".on_off").siblings('a').addClass('transparent');
		$(".on_off li:first").addClass("on").removeClass("on_on");
	});
	
	$(".on_off2 li:first").click(function(){ 
		$(this).addClass("on_on").removeClass("on");
        $(".on_off2").siblings('a').removeClass('transparent');
		$(".on_off2 li:last").addClass("off").removeClass("off_on");
	});

	$(".on_off2 li:last").click(function(){ 
		$(this).addClass("off_on").removeClass("off");
        $(".on_off2").siblings('a').addClass('transparent');
		$(".on_off2 li:first").addClass("on").removeClass("on_on");
	});
	
	$(".on_off3 li:first").click(function(){ 
		$(this).addClass("on_on").removeClass("on");
        $(".on_off3").siblings('a').removeClass('transparent');
		$(".on_off3 li:last").addClass("off").removeClass("off_on");
	});

	$(".on_off3 li:last").click(function(){ 
		$(this).addClass("off_on").removeClass("off");
        $(".on_off3").siblings('a').addClass('transparent');
		$(".on_off3 li:first").addClass("on").removeClass("on_on");
	});

	$(".on_off4 li:first").click(function(){ 
		$(this).addClass("on_on").removeClass("on");
        $(".on_off4").siblings('a').removeClass('transparent');
		$(".on_off4 li:last").addClass("off").removeClass("off_on");
	});

	$(".on_off4 li:last").click(function(){ 
		$(this).addClass("off_on").removeClass("off");
        $(".on_off4").siblings('a').addClass('transparent');
		$(".on_off4 li:first").addClass("on").removeClass("on_on");
	});
	
	$(".on_off5 li:first").click(function(){ 
		$(this).addClass("on_on").removeClass("on");
        $(".on_off5").siblings('a').removeClass('transparent');
		$(".on_off5 li:last").addClass("off").removeClass("off_on");
	});

	$(".on_off5 li:last").click(function(){ 
		$(this).addClass("off_on").removeClass("off");
        $(".on_off5").siblings('a').addClass('transparent');
		$(".on_off5 li:first").addClass("on").removeClass("on_on");
	});
    
    $(".on_off6 li:first").click(function(){ 
		$(this).addClass("on_on").removeClass("on");
        $(".on_off6").siblings('a').removeClass('transparent');
		$(".on_off6 li:last").addClass("off").removeClass("off_on");
	});

	$(".on_off6 li:last").click(function(){ 
		$(this).addClass("off_on").removeClass("off");
        $(".on_off6").siblings('a').addClass('transparent');
		$(".on_off6 li:first").addClass("on").removeClass("on_on");
	});
    
    $(".on_off7 li:first").click(function(){ 
		$(this).addClass("on_on").removeClass("on");
        $(".on_off7").siblings('a').removeClass('transparent');
		$(".on_off7 li:last").addClass("off").removeClass("off_on");
	});

	$(".on_off7 li:last").click(function(){ 
		$(this).addClass("off_on").removeClass("off");
        $(".on_off7").siblings('a').addClass('transparent');
		$(".on_off7 li:first").addClass("on").removeClass("on_on");
	});
    
    //on click function
    $('.switchPanel .game').click(function(){
        var db1 = window.parent.$.zyUiDialog({
            width: 550,
            height: 250,
            title: 'Game Engine',
			buttons:{
                '<%ejGetML(MLG_Common_OK)%>': function(){ db1.dialog('close');}
			}
        });
		var link = 'pages/user/gameengine.html';
        if(jQuery.browser.safari && parseInt(jQuery.browser.version)<=525 )
        link='../../'+link;
        db1.load(link);
        db1.dialog('open');
    });
    
    $('.switchPanel .power').click(function(){
        var db1 = window.parent.$.zyUiDialog({
            width: 750,
            height: 450,
            title: 'Power Saving',
			buttons:{
                '<%ejGetML(MLG_Common_Cancel)%>': function(){ db1.dialog('close');},
                '<%ejGetML(MLG_Common_Apply)%>': function(){ }
			}
        });
		var link = 'pages/user/powersaving.html';
        if(jQuery.browser.safari && parseInt(jQuery.browser.version)<=525 )
        link='../../'+link;
        db1.load(link);
        db1.dialog('open');
    });
    
    $('.switchPanel .firewall').click(function(){
        var db1 = window.parent.$.zyUiDialog({
            width: 550,
            height: 250,
            title: 'Firewall',
			buttons:{
                '<%ejGetML(MLG_Common_OK)%>': function(){ db1.dialog('close');}
			}
        });
		var link = 'pages/user/firewall.html';
        if(jQuery.browser.safari && parseInt(jQuery.browser.version)<=525 )
        link='../../'+link;
        db1.load(link);
        db1.dialog('open');
    });
    
    $('.switchPanel .wireless').click(function(){
        var db1 = window.parent.$.zyUiDialog({
            width: 750,
            height: 600,
            title: 'Wireless Security',
			buttons:{
                '<%ejGetML(MLG_Common_Cancel)%>': function(){ db1.dialog('close');},
                '<%ejGetML(MLG_Common_Apply)%>': function(){ }
			}
        });
		var link = 'pages/user/wirelesssecurity.html';
        if(jQuery.browser.safari && parseInt(jQuery.browser.version)<=525 )
        link='../../'+link;
        db1.load(link);
        db1.dialog('open');
    });
    
    $('.switchPanel .contentfilter').click(function(){
        var db1 = window.parent.$.zyUiDialog({
            width: 750,
            height: 450,
            title: 'Content Filter',
			buttons:{
                '<%ejGetML(MLG_Common_Cancel)%>': function(){ db1.dialog('close');},
                '<%ejGetML(MLG_Common_Apply)%>': function(){ }
			}
        });
		var link = 'pages/user/contentfilter.html';
        if(jQuery.browser.safari && parseInt(jQuery.browser.version)<=525 )
        link='../../'+link;
        db1.load(link);
        db1.dialog('open');
    });
    
    $('.switchPanel .bandwidth').click(function(){
        var db1 = window.parent.$.zyUiDialog({
            width: 700,
            height: 550,
            title: 'Bandwidth MGMT',
			buttons:{
                '<%ejGetML(MLG_Common_Cancel)%>': function(){ db1.dialog('close');},
                '<%ejGetML(MLG_Common_Apply)%>': function(){ }
			}
        });
		var link = 'pages/user/bandwidthmgmt.html';
        if(jQuery.browser.safari && parseInt(jQuery.browser.version)<=525 )
        link='../../'+link;
        db1.load(link);
        db1.dialog('open');
    });
    
    $('.switchPanel .dlna').click(function(){
        var db1 = window.parent.$.zyUiDialog({
            width: 550,
            height: 250,
            title: 'Media Server',
			buttons:{
                '<%ejGetML(MLG_Common_OK)%>': function(){ db1.dialog('close');}
			}
        });
		var link = 'pages/user/mediaserver.html';
        if(jQuery.browser.safari && parseInt(jQuery.browser.version)<=525 )
        link='../../'+link;
        db1.load(link);
        db1.dialog('open');
    });

});

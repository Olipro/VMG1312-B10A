function nextWizardState(wizardDB, id, save_id){
	var buttomPana = wizardDB.parents('.ui-dialog').find('.ui-dialog-buttonpane');
	var titlePana = wizardDB.parents('.ui-dialog').find('.ui-dialog-titlebar');
	switch (id) {
		case 'welcome':
			if ( isInetExist() ) {
				wizardDB.find('#welcome').hide(0);
				wizardDB.find('#internetsetting').show(0);
				wizardDB.find('#wirelesssetting').hide(0);
			}
			else {
				wizardDB.find('#welcome').hide(0);
				wizardDB.find('#internetsetting').hide(0);
				wizardDB.find('#wirelesssetting').show(0);
			}
			wizardDB.find('#waiting').hide(0);
			wizardDB.find('#result').hide(0);

			buttomPana.children('Button:nth-child(3)').removeAttr('disabled');
			break;

		case 'internetsetting':
			wizardDB.find('#welcome').hide(0);
			wizardDB.find('#waiting').hide(0);
			wizardDB.find('#result').hide(0);
			if ( checkInternetValue() ) {
				wizardDB.find('#internetsetting').hide(0);
				wizardDB.find('#wirelesssetting').show(0);
				buttomPana.children('Button:nth-child(2)').removeAttr('disabled').html(save_id);
			}
			else {
				wizardDB.find('#internetsetting').show(0);
				wizardDB.find('#wirelesssetting').hide(0);
			}
			break;
		          
		case 'wirelesssetting': 
			wizardDB.find('#welcome').hide(0);
			wizardDB.find('#internetsetting').hide(0);
			wizardDB.find('#wirelesssetting').hide(0);
			wizardDB.find('#waiting').show(0);
			wizardDB.find('#result').hide(0);

			titlePana.children('.ui-dialog-titlebar-close').hide(0);
			buttomPana.children('Button:nth-child(1)').attr('disabled', 'disabled');
			buttomPana.children('Button:nth-child(2)').attr('disabled', 'disabled');
			buttomPana.children('Button:nth-child(3)').attr('disabled', 'disabled');
			saveConfig();
			break;

		case 'waiting':
			wizardDB.find('#welcome').hide(0);
			wizardDB.find('#internetsetting').hide(0);
			wizardDB.find('#wirelesssetting').hide(0);
			wizardDB.find('#waiting').hide(0);
			wizardDB.find('#result').show(0);
			
			titlePana.children('.ui-dialog-titlebar-close').show(0);
			buttomPana.children('Button:nth-child(1)').removeAttr('disabled');
			buttomPana.children('Button:nth-child(2)').hide();
			buttomPana.children('Button:nth-child(3)').hide();
			break;				          

		default:
			wizardDB.dialog('close');
			break; 
	}
}

function backWizardState(db1, id, next_id){
	var buttomPana = db1.parents('.ui-dialog').find('.ui-dialog-buttonpane');
	var titlePana = db1.parents('.ui-dialog').find('.ui-dialog-titlebar');
	switch (db1.find('.state:visible').attr('id')) {
		case 'internetsetting':
			db1.find('#welcome').show(0);
			db1.find("#internetsetting").hide(0);
			db1.find('#wirelesssetting').hide(0);
			db1.find('#waiting').hide(0);
			db1.find('#result').hide(0);

			buttomPana.children('Button:nth-child(3)').attr('disabled', 'disabled');
			break;

		case 'wirelesssetting':
			if ( isInetExist() ) {
				db1.find('#welcome').hide(0);
				db1.find("#internetsetting").show(0);
			}
			else {
				db1.find('#welcome').show(0);
				db1.find('#internetsetting').hide(0);
				buttomPana.children('Button:nth-child(3)').attr('disabled', 'disabled');
			}
			db1.find('#wirelesssetting').hide(0);
			db1.find('#waiting').hide(0);
			db1.find('#result').hide(0);

			buttomPana.children('Button:nth-child(2)').removeAttr('disabled').html(next_id);
			break;
			      
		default:
			db1.dialog('close');
			break; 
	}
}

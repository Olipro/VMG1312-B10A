<!-- #BEGINZONE useredit_up_zone -->
<script language="javascript">
var groupstr = '##GROUP_STR##';
var passexpired = '##PASS_EXPIRED##';
var advancedaccountsecurity = '##ADVANCED_ACCOUNT_SECURITY##';

function isPassFormat(value, msg){
   var alphaExp = /^(?=^.{6,}$)((?=.*[0-9])(?=.*[a-zA-Z]))^[0-9a-zA-Z_#@!=+-.]+$/;
   if (value.match(alphaExp)) {
       return true;
   } else {
       //AlertOpen(msg,2);
       hciAlert(msg, {height: 180,type: 2, title: '##Error_Title##'});
       return false;
   }
}

function isNumberKey(evt)
{
	var charCode = (evt.which) ? evt.which : event.keyCode
    if (charCode > 31 && (charCode < 48 || charCode > 57))
    	return false;
    return true;
}

function writeGroupList() {
	var str = '';
	var group;
	var i = 0;
	if ( groupstr != '' ) {
		group = groupstr.split('|');
		for ( i = 0 ; i < group.length; i ++ ) {
			var groupsel = group[i].split('/');
			if ( groupsel[0] != '' ) {
				if ( groupsel[1] == '1' ) {
					str += "<option value=\""+ groupsel[0] +"\" selected>"+ groupsel[0] +"</option>";
				}
				else {
					str += "<option value=\""+ groupsel[0] +"\">"+ groupsel[0] +"</option>";
				}
			}
		}
		$('#showgroupName').append(str);
	}
}

function btnApply() {
  with ( document.forms[0] ) {
	action.value="edit";

	if(OldPasswd.value != 0 || NewPasswd.value != 0 || VerifyNewPasswd.value != 0)
	{
		
		if(isPassFormat(NewPasswd.value,"Password must require a minimum length of 6 characters(mixed alphabetic and numeric).") == false)
			return;
	
		if(NewPasswd.value == userName.value)
		{
			//AlertOpen("New Password can't be the same with User Name",2);
			hciAlert("##Alert_Str26##", {height: 180,type: 2, title: '##Error_Title##'});
			return;
		}

		// ZyXEL, ShuYing, ignore that the characters in password cannot contain username
		/*if(NewPasswd.value.indexOf(userName.value) != -1)
		{
		    //AlertOpen("The characters in New Password cannot contain User Name",2);
		    hciAlert("The characters in New Password cannot contain User Name.", {height: 180,type: 2, title: 'Error'});
			return;
		}*/
		
		if(NewPasswd.value != VerifyNewPasswd.value)
		{
			//AlertOpen("Verify New Password fail",2);
			hciAlert("##Alert_Str27##", {height: 180,type: 2, title: '##Error_Title##'});
			return;
		}
	}
	
	if ( passexpired == '1' ) {
		if(expPeriod.value > 180)
		{
			//AlertOpen("Max expire period time is 180 days",2);
			hciAlert("##Alert_Str28##", {height: 180,type: 2, title: '##Error_Title##'});
			return;
		}
		else if(expPeriod.value < 30)
		{
			//AlertOpen("Min expire period time is 30 days",2);
			hciAlert("##Alert_Str29##", {height: 180,type: 2, title: '##Error_Title##'});
			return;
		}
	}

	if(retryTime.value > 5)
	{
		//AlertOpen("Max retry times is 5",2);
		hciAlert("##Alert_Str30##", {height: 180,type: 2, title: '##Error_Title##'});
		return;
	}
	else if(retryTime.value < 0)
	{
		//AlertOpen("Min retry times is 0",2);
		hciAlert("##Alert_Str31##", {height: 180,type: 2, title: '##Error_Title##'});
		return;
	}

	if(idleTime.value > 60)
	{
		//AlertOpen("Max idle timeout is 60 minutes",2);
		hciAlert("##Alert_Str32##", {height: 180,type: 2, title: '##Error_Title##'});
		return;
	}
	else if(idleTime.value < 1)
	{
		//AlertOpen("Min idle timeout is 1",2);
		hciAlert("##Alert_Str33##", {height: 180,type: 2, title: '##Error_Title##'});
		return;
	}

	if(lockTime.value > 90)
	{
		//AlertOpen("Max locked time period is 90 minutes",2);
		hciAlert("##Alert_Str34##", {height: 180,type: 2, title: '##Error_Title##'});
		return;
	}
	else if(lockTime.value < 15)
	{
		//AlertOpen("Min locked time period is 15 minutes",2);
		hciAlert("##Alert_Str35##", {height: 180,type: 2, title: '##Error_Title##'});
		return;
	}
	groupName.value =showgroupName.value;
	sessionKey.value = mainFrame.gblsessionKey;
    submit();
    $('.ui-dialog-titlebar-close').trigger('click');
    $.openLoadingMask(1);
    return;
  }
}

var db = $('.popup_frame').parents('.dialogContener'); 
var ApplyBtn = db.parents('.ui-dialog').find('.ui-dialog-buttonpane').children('Button:nth-child(2)');
ApplyBtn.click(function(){ btnApply(); });

$(document).ready(function() {
	writeGroupList();
	if ( advancedaccountsecurity == '0' ) {
		passexpired = '0';
	}

	if ( passexpired != '1' ) {
		$("#passexpiredli").hide();
	}

});
</script>

<form method=POST action="/pages/tabFW/userAccount-cfg.cmd" id="userEdit" target="mainFrame">
<input type="hidden" name="action" id="action">
<input type="hidden" name="sessionKey" id="sessionKey">
<input type="hidden" name="userName" id="userName" value=##userName##>
<input type="hidden" name="groupName" id="groupName">
<div class="popup_frame" >
<div class="data_frame"><ul>
<li class="set1">
    <div class="w_text">
		<ul>
			<li class="left_table"> ##User_Name_Account##   :</li>
			<li class="right_table">
				<input type="text" name="userNameShow" size="15" maxlength="15" value=##userName## disabled/>
			</li>
		</ul>
	</div>
</li>
<li class="set1">
    <div class="w_text">
		<ul>
			<li class="left_table"> ##Old_Password_Account##   :</li>
			<li class="right_table">
				<input name="OldPasswd" type="password" id="OldPasswd"  class="text" size="15" maxlength="20" />
			</li>
		</ul>
	</div>
</li>
<li class="set1">
    <div class="w_text">
		<ul>
			<li class="left_table"> ##New_Password_Account##   :</li>
			<li class="right_table">
				<input name="NewPasswd" type="password" id="NewPasswd"  class="text" size="15" maxlength="20" />
			</li>
		</ul>
	</div>
</li>
<li class="set1">
    <div class="w_text">
		<ul>
			<li class="left_table"> ##Verify_New_Password_Account##   :</li>
			<li class="right_table">
				<input name="VerifyNewPasswd" type="password" id="VerifyNewPasswd"  class="text" size="15" maxlength="20"/>
			</li>
		</ul>
	</div>
</li>
<li class="set1" id="passexpiredli">
	<div class="w_text">
		<ul>
			<li class="left_table">Expire Period  :</li>
			<li class="right_table">
				<input type="text" name="expPeriod" size="3" maxlength="3" value=##expPeriod## onkeypress="return isNumberKey(event)" />&nbsp;(30~180 Days)
			</li>
		</ul>
	</div>
</li>
<li class="set1">
    <div class="w_text">
		<ul>
			<li class="left_table"> ##Retry_Times_Account##  :</li>
			<li class="right_table">
				<input type="text" name="retryTime" size="1" maxlength="1" value=##retryTime## onkeypress="return isNumberKey(event)"/>&nbsp;##Retry_Times_log##
			</li>
		</ul>
	</div>
</li>
<li class="set1">
    <div class="w_text">
		<ul>
			<li class="left_table"> ##Idle_Timeout_Account##  :</li>
			<li class="right_table">
				<input type="text" name="idleTime" size="2" maxlength="2" value=##idleTime## onkeypress="return isNumberKey(event)"/>&nbsp;##Minute_1##
			</li>
		</ul>
	</div>
</li>
<li class="set1">
	<div class="w_text">
		<ul>
			<li class="left_table"> ##Lock_Period_Account##  :</li>
			<li class="right_table">
				<input type="text" name="lockTime" size="2" maxlength="2" value=##lockTime## onkeypress="return isNumberKey(event)"/>&nbsp;##Minute_2##
			</li>
		</ul>
	</div>
</li>
<li class="set1">
	<div class="w_text">
		<ul>
			<li class="left_table"> ##Group_Account##   :
			</li>
			<li class="right_table">
				<select name="showgroupName" id="showgroupName" ##groupdisable## disabled/>
				</select>
			</li>
		</ul>
	</div>
</li>

</ul>
</div>
</div>
</form>
<!-- #ENDZONE useredit_up_zone -->

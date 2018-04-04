var ntpServers = new Array();

ntpServers[0] = 'clock.fmt.he.net';
ntpServers[1] = 'clock.nyc.he.net';
ntpServers[2] = 'clock.sjc.he.net';
ntpServers[3] = 'clock.via.net';
ntpServers[4] = 'ntp1.tummy.com';
ntpServers[5] = 'time.cachenetworks.com';
ntpServers[6] = 'time.nist.gov';
ntpServers[7] = 'pool.ntp.org';

var daylightsaving = new Array();
daylightsaving[0] = 'M3.2.0/2,M11.1.0/2';
daylightsaving[1] = 'M3.5.0/1,M10.5.0/1';
daylightsaving[2] = 'M10.2.0,M3.2.0';
daylightsaving[3] = 'M10.1.0,M4.1.0/3';
daylightsaving[4] = 'M4.4.5,M9.4.4';
daylightsaving[5] = 'M3.4.5,M9.2.0';
daylightsaving[6] = '';
daylightsaving[7] = 'M3.5.0/2,M10.4.0/3';
daylightsaving[8] = 'M3.D21,M9.D21';
daylightsaving[9] = 'M3.5.0/4,M10.4.0/5';
daylightsaving[10] = 'M4.1.0/3,M10.1.0/2';
daylightsaving[11] = 'M4.1.0/3:45,M9.4.0/2:45';
daylightsaving[12] = 'M3.5.0/2,M10.5.0/3';
daylightsaving[13] = 'M4.1.0/2,M9.4.0/2';
daylightsaving[14] = 'M2.3.0,M10.3.0';

var Size = 92;
var timeZones = new Array(Size);
for ( var i = 0; i < Size; i++ )
	timeZones[i] = new Array(3);

timeZones[0][0] = '(GMT-12:00) International Date Line West';
timeZones[0][1] = 'IDLW12';
timeZones[1][0] = '(GMT-11:00) Midway Island, Samoa';
timeZones[1][1] = 'SST11';
timeZones[2][0] = '(GMT-10:00) Hawaii';
timeZones[2][1] = 'HST10';
timeZones[3][0] = '(GMT-09:00) Alaska';
timeZones[3][1] = 'AKST9AKDT';
timeZones[3][2] = 0;
timeZones[4][0] = '(GMT-08:00) Pacific Time, Tijuana';
timeZones[4][1] = 'PST8PDT';
timeZones[4][2] = 0;
timeZones[5][0] = '(GMT-07:00) Arizona, Mazatlan';
timeZones[5][1] = 'MST7';
timeZones[6][0] = '(GMT-07:00) Chihuahua';
timeZones[6][1] = 'MST7MDT';
timeZones[6][2] = 13;
timeZones[7][0] = '(GMT-07:00) Mountain Time';
timeZones[7][1] = 'MST7MDT';
timeZones[7][2] = 0;
timeZones[8][0] = '(GMT-06:00) Central America';
timeZones[8][1] = 'CST6CDT';
timeZones[8][2] = 0;
timeZones[9][0] = '(GMT-06:00) Central Time';
timeZones[9][1] = 'CST6CDT';
timeZones[9][2] = 0;
timeZones[10][0] = '(GMT-06:00) Guadalajara, Mexico City, Monterrey';
timeZones[10][1] = 'CST6CDT';
timeZones[10][2] = 13;
timeZones[11][0] = '(GMT-06:00) Saskatchewan';
timeZones[11][1] = 'CST6';
timeZones[12][0] = '(GMT-05:00) Bogota, Lima, Quito';
timeZones[12][1] = 'COT5';
timeZones[13][0] = '(GMT-05:00) Eastern Time';
timeZones[13][1] = 'EST5EDT';
timeZones[13][2] = 0;
timeZones[14][0] = '(GMT-05:00) Indiana';
timeZones[14][1] = 'EST5EDT';
timeZones[14][2] = 0;
timeZones[15][0] = '(GMT-04:00) Atlantic Time';
timeZones[15][1] = 'AST4ADT';
timeZones[15][2] = 0;
timeZones[16][0] = '(GMT-04:00) Caracas, La Paz';
timeZones[16][1] = 'BOT4';
timeZones[17][0] = '(GMT-04:00) Santiago';
timeZones[17][1] = 'CLT4CLST';
timeZones[17][2] = 2;
timeZones[18][0] = '(GMT-04:00) Georgetown';
timeZones[18][1] = 'ART3';
timeZones[19][0] = '(GMT-03:30) Newfoundland';
timeZones[19][1] = 'NST3:30NDT';
timeZones[19][2] = 0;
timeZones[20][0] = '(GMT-03:00) Brasilia';
timeZones[20][1] = 'BRT3BRST';
timeZones[20][2] = 14;
timeZones[21][0] = '(GMT-03:00) Buenos Aires';
timeZones[21][1] = 'ART3';
timeZones[22][0] = '(GMT-03:00) Greenland';
timeZones[22][1] = 'CGT3';
timeZones[23][0] = '(GMT-02:00) Mid-Atlantic';
timeZones[23][1] = 'MAT2';
timeZones[24][0] = '(GMT-01:00) Azores';
timeZones[24][1] = 'AZOT1AZOST';
timeZones[24][2] = 1;
timeZones[25][0] = '(GMT-01:00) Cape Verde Is.';
timeZones[25][1] = 'CVT1';
timeZones[26][0] = '(GMT-00:00) Casablanca';
timeZones[26][1] = 'WET0WEST';
timeZones[27][0] = '(GMT-00:00) Monrovia';
timeZones[27][1] = 'WET0';
timeZones[28][0] = '(GMT-00:00) Greenwich Mean Time: Edinburgh, London';
timeZones[28][1] = 'GMT0BST';
timeZones[28][2] = 1;
timeZones[29][0] = '(GMT-00:00) Greenwich Mean Time: Dublin';
timeZones[29][1] = 'GMT0IST';
timeZones[29][2] = 1;
timeZones[30][0] = '(GMT-00:00) Lisbon';
timeZones[30][1] = 'WET0WEST';
timeZones[30][2] = 1;
timeZones[31][0] = '(GMT+01:00) Amsterdam, Berlin, Bern, Rome, Stockholm, Vienna';
timeZones[31][1] = 'CET-1CEST';
timeZones[31][2] = 1;
timeZones[32][0] = '(GMT+01:00) Belgrade, Bratislava, Budapest, Ljubljana, Prague';
timeZones[32][1] = 'CET-1CEST';
timeZones[32][2] = 1;
timeZones[33][0] = '(GMT+01:00) Brussels, Copenhagen, Madrid, Paris';
timeZones[33][1] = 'CET-1CEST';
timeZones[33][2] = 1;
timeZones[34][0] = '(GMT+01:00) Sarajevo, Skopje, Warsaw, Zagreb';
timeZones[34][1] = 'CET-1CEST';
timeZones[34][2] = 1;
timeZones[35][0] = '(GMT+01:00) West Central Africa';
timeZones[35][1] = 'WAT-1';
timeZones[36][0] = '(GMT+02:00) Athens, Istanbul, Minsk';
timeZones[36][1] = 'EET-2EEST';
timeZones[36][2] = 1;
timeZones[37][0] = '(GMT+02:00) Bucharest';
timeZones[37][1] = 'EET-2EEST';
timeZones[37][2] = 1;
timeZones[38][0] = '(GMT+02:00) Cairo';
timeZones[38][1] = 'EET-2EEST';
timeZones[38][2] = 4;
timeZones[39][0] = '(GMT+02:00) Harare, Pretoria';
timeZones[39][1] = 'CAT-2';
timeZones[40][0] = '(GMT+02:00) Pretoria';
timeZones[40][1] = 'SAST-2';
timeZones[41][0] = '(GMT+02:00) Helsinki, Kyiv, Riga, Sofia, Tallinn, Vilnius';
timeZones[41][1] = 'EET-2EEST';
timeZones[41][2] = 1;
timeZones[42][0] = '(GMT+02:00) Jerusalem';
timeZones[42][1] = 'IST-2IDT';
timeZones[42][2] = 5;
timeZones[43][0] = '(GMT+03:00) Baghdad';
timeZones[43][1] = 'AST-3';
timeZones[44][0] = '(GMT+03:00) Kuwait, Riyadh';
timeZones[44][1] = 'AST-3';
timeZones[45][0] = '(GMT+03:00) Moscow, St. Petersburg, Volgograd';
timeZones[45][1] = 'MSK-3MSD';
timeZones[45][2] = 7;
timeZones[46][0] = '(GMT+03:00) Nairobi';
timeZones[46][1] = 'EAT-3';
timeZones[47][0] = '(GMT+03:30) Tehran';
timeZones[47][1] = 'IRST-3:30IRDT';
timeZones[47][2] = 8;
timeZones[48][0] = '(GMT+04:00) Abu Dhabi, Muscat';
timeZones[48][1] = 'GST-4';
timeZones[49][0] = '(GMT+04:00) Baku';
timeZones[49][1] = 'AZT-4AZST';
timeZones[49][2] = 9;
timeZones[50][0] = '(GMT+04:00) Yerevan';
timeZones[50][1] = 'AMT-4AMST';
timeZones[50][2] = 9;
timeZones[51][0] = '(GMT+04:00) Tbilisi';
timeZones[51][1] = 'GET-4';
timeZones[52][0] = '(GMT+04:30) Kabul';
timeZones[52][1] = 'AFT-4:30';
timeZones[53][0] = '(GMT+05:00) Yekaterinburg';
timeZones[53][1] = 'YEKT-5YEKST';
timeZones[53][2] = 7;
timeZones[54][0] = '(GMT+05:00) Islamabad, Karachi';
timeZones[54][1] = 'PKT-5';
timeZones[55][0] = '(GMT+05:00) Tashkent';
timeZones[55][1] = 'UZT-5';
timeZones[56][0] = '(GMT+05:30) Chennai, Kolkata, Mumbai, New Delhi';
timeZones[56][1] = 'IST-5:30';
timeZones[57][0] = '(GMT+05:45) Kathmandu';
timeZones[57][1] = 'NPT-5:45';
timeZones[58][0] = '(GMT+06:00) Almaty, Astana';
timeZones[58][1] = 'ALMT-6';
timeZones[59][0] = '(GMT+06:00) Novosibirsk';
timeZones[59][1] = 'NOVT-6NOVST';
timeZones[59][2] = 12;
timeZones[60][0] = '(GMT+06:00) Dhaka';
timeZones[60][1] = 'BST-6';
timeZones[61][0] = '(GMT+06:00) Sri Jayawardenapura';
timeZones[61][1] = 'LKT-6';
timeZones[62][0] = '(GMT+06:30) Yangoon';
timeZones[62][1] = 'MMT-6:30';
timeZones[63][0] = '(GMT+07:00) Bangkok, Hanoi';
timeZones[63][1] = 'ICT-7';
timeZones[64][0] = '(GMT+07:00) Jakarta';
timeZones[64][1] = 'WIB-7';
timeZones[65][0] = '(GMT+07:00) Krasnoyarsk';
timeZones[65][1] = 'KRAT-7KRAST';
timeZones[65][2] = 7;
timeZones[66][0] = '(GMT+08:00) Hong Kong';
timeZones[66][1] = 'HKT-8';
timeZones[67][0] = '(GMT+08:00) Beijing, Chongquing, Urumqi';
timeZones[67][1] = 'CST-8';
timeZones[68][0] = '(GMT+08:00) Irkutsk';
timeZones[68][1] = 'IRKT-8IRST';
timeZones[68][2] = 12;
timeZones[69][0] = '(GMT+08:00) Ulaan Bataar';
timeZones[69][1] = 'LUAT-8';
timeZones[70][0] = '(GMT+08:00) Kuala Lumpur';
timeZones[70][1] = 'MYT-8';
timeZones[71][0] = '(GMT+08:00) Singapore';
timeZones[71][1] = 'SGT-8';
timeZones[72][0] = '(GMT+08:00) Perth';
timeZones[72][1] = 'WST-8';
timeZones[73][0] = '(GMT+08:00) Taipei';
timeZones[73][1] = 'CST-8';
timeZones[74][0] = '(GMT+09:00) Osaka, Sapporo, Tokyo';
timeZones[74][1] = 'JST-9';
timeZones[75][0] = '(GMT+09:00) Seoul';
timeZones[75][1] = 'KST-9';
timeZones[76][0] = '(GMT+09:00) Yakutsk';
timeZones[76][1] = 'YAKT-9YAKST';
timeZones[76][2] = 7;
timeZones[77][0] = '(GMT+09:30) Adelaide';
timeZones[77][1] = 'CST-9:30CDT';
timeZones[77][2] = 3;
timeZones[78][0] = '(GMT+09:30) Darwin';
timeZones[78][1] = 'CST-9:30';
timeZones[79][0] = '(GMT+10:00) Brisbane';
timeZones[79][1] = 'EST-10';
timeZones[80][0] = '(GMT+10:00) Canberra, Melbourne, Sydney';
timeZones[80][1] = 'EST-10EDT';
timeZones[80][2] = 3;
timeZones[81][0] = '(GMT+10:00) Guam';
timeZones[81][1] = 'ChST-10';
timeZones[82][0] = '(GMT+10:00) Port Moresby';
timeZones[82][1] = 'PGT-10';
timeZones[83][0] = '(GMT+10:00) Hobart';
timeZones[83][1] = 'EST-10EDT';
timeZones[83][2] = 10;
timeZones[84][0] = '(GMT+10:00) Vladivostok';
timeZones[84][1] = 'VLAT-10VLAST';
timeZones[84][2] = 7;
timeZones[85][0] = '(GMT+11:00) Magadan';
timeZones[85][1] = 'MAGT-11MAGST-11';
timeZones[85][2] = 7;
timeZones[86][0] = '(GMT+11:00) Solomon Is.';
timeZones[86][1] = 'SBT-11';
timeZones[87][0] = '(GMT+11:00) New Caledonia';
timeZones[87][1] = 'NCT-11';
timeZones[88][0] = '(GMT+12:00) Auckland, Wellington';
timeZones[88][1] = 'NZST-12NZDT';
timeZones[88][2] = 11;
timeZones[89][0] = '(GMT+12:00) Kamchatka';
timeZones[89][1] = 'PETT-12PETST';
timeZones[89][2] = 1;
timeZones[90][0] = '(GMT+12:00) Marshall Is.';
timeZones[90][1] = 'MHT-12';
timeZones[91][0] = '(GMT+12:00) Fiji';
timeZones[91][1] = 'FJT-12FJST';
timeZones[91][2] = 7;

function getLocalTimeZone(idx) {
	var ret =  timeZones[idx][1];//NPT-5:45DNPT
	
	var start = timeZones[idx][1].length;
	var end = 0;
	
	for ( i = 0 ; i < timeZones[idx][1].length ; i ++ ) {
		if ( timeZones[idx][1].charCodeAt(i) == 45 ) { //-
			start = start > i ? i : start;
		}
		else if ( timeZones[idx][1].charCodeAt(i) >= 48 && timeZones[idx][1].charCodeAt(i) <= 57 ) {
			start = start > i ? i : start;
		}
		else if ( timeZones[idx][1].charCodeAt(i) == 58 ) { //:
		
		}
		else if ( timeZones[idx][1].charCodeAt(i) < 48 || timeZones[idx][1].charCodeAt(i) > 57 ) {
			if ( start != timeZones[idx][1].length ) {
				end = i;
				break;
			}
		}
	}
	if ( i == timeZones[idx][1].length && end == 0 ) {
		end = i;
	}

	ret = timeZones[idx][1].substr(0, end);//NPT-5:45
	
	return ret
}

function getTimeZoneName(idx) {
	var str = timeZones[idx][0];
	var ret = '';
	
	ret = str.substr(12);
	
	return ret;
}

function getTimeZoneNameIndex(timeZoneName) {
	var i = 0, ret = 13;  // default to Eastern Time
	
	for ( i = 0; i < Size; i++ ) {
		if ( timeZones[i][0].search(timeZoneName) != -1 )
			break;
	}
	
	if ( i < Size )
		ret = i;
	
	return ret;
}

function getLocalTimeZoneNameIndex(localtimeZone) {
	var i = -1, ret = 13;  // default to Eastern Time
	var ltz_name = localtimeZone.split(',');
	if ( ltz_name.length >= 3 ) {
		var rule = ltz_name[1]+','+ltz_name[2];
		
		for ( i = 0; i < Size; i++ ) {
			if ( localtimeZone.search(timeZones[i][1]) != -1 && timeZones[i][2] != undefined ) {
				if ( daylightsaving[timeZones[i][2]] == rule ) {
					break;
				}
				else {
				}
			}
		}
	}
	
	if ( i < 0 || i == Size ) {
		for ( i = 0; i < Size; i++ ) {
			if ( localtimeZone.search(timeZones[i][1]) != -1 )
				break;
		}
	}


	if ( i < Size )
		ret = i;
	else { //NAB-8BBB,M3.2.1,M4.3.1
		if ( ltz_name.length >= 1 ) {
			var start = ltz_name[0].length;
			var end = 0;
			var invert = 0;
			
			for ( i = 0 ; i < ltz_name[0].length ; i ++ ) {
				if ( ltz_name[0].charCodeAt(i) == 45 ) {
					invert = 1;
				}
				else if ( ltz_name[0].charCodeAt(i) >= 48 && ltz_name[0].charCodeAt(i) <= 57 ) {
					start = start > i ? i : start;
				}
				else if ( ltz_name[0].charCodeAt(i) == 58 ) {
				
				}
				else if ( ltz_name[0].charCodeAt(i) < 48 || ltz_name[0].charCodeAt(i) > 57 ) {
					if ( start != ltz_name[0].length ) {
						end = i;
						break;
					}
				}
			}
			if ( i == ltz_name[0].length && end == 0 ) {
				end = i;
			}
			
			var len = end - start;
			var tz_offset = ltz_name[0].substr(start, len);
			
			var result = tz_offset.split(":");
			
			if ( result.length <= 1 ) {
				if (len < 2 )
					tz_offset = '0'+tz_offset;
				if ( invert != 1 ) {
					tz_offset = '-'+tz_offset;
				}
				else {
					tz_offset = '+'+tz_offset;
				}
			}
			else {
				if (result[0].length < 2 )
					tz_offset = '0'+tz_offset;
					if ( invert != 1 ) {
						tz_offset = '-'+tz_offset;
					}
					else {
						tz_offset = '+'+tz_offset;
					}
			}
			
			tz_offset = tz_offset+':';
			for ( i = 0; i < Size; i++ ) {
				if ( timeZones[i][0].indexOf(tz_offset) != -1 ) {
					ret = i;
				break;
				}
			}
		}
  }

  return ret;
}
 
function getLocalTimeZoneName(idx) {
	var ret = timeZones[idx][1]
	
	if ( timeZones[idx][2] != undefined ) {
		ret += ','+daylightsaving[timeZones[idx][2]];
	}
	
	return ret;
}
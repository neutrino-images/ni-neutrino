/*	yWeb NIlib by NI-Team
*/

//	next function is taken from the original yweb
//	to make them available in all of our pages
function goUrl(_url){
	var res = trim(loadSyncURL(_url));
	switch(res){
		case "1": res="on"; break;
		case "0": res="off"; break;
	}
	$("out").update(res);
}

function goPort(_port) {
	if (_port == "") return;
	var host = self.location.href;
	var pos1 = host.indexOf('//');
	var temp = host.substring(pos1+2,host.length);
	var pos2 = temp.indexOf('/');
	var host = host.substring(0,pos1+2+pos2);
	window.open(host + ":" + _port,"_blank");
}

function Y_NI_Tools(_cmd, _tout){
	$("out").update("");
	show_waitbox(true);
	goUrl("/control/exec?Y_NI_Tools&" + _cmd);
	if (typeof(_tout) == "undefined")
		{
		show_waitbox(false);
		}
	else
		{
		window.setTimeout("document.location.reload()", _tout);
		}
}

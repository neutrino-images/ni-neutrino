#!/bin/sh
# -----------------------------------------------------------
# yWeb Filemgr (by yjogol)
# -----------------------------------------------------------

# -----------------------------------------------------------
# Main
# -----------------------------------------------------------
case "$1" in
	list_users)
		cat $fpasswd|sed 's/^\([^:]*\).*/<input type=radio name=users value=\1>\1<br>/'
	;;
	filemgr_list)
		shift 1
		ls -al $*
	;;
	filemgr_chmod)
		shift 1
		chmod $*
	;;
	filemgr_mkdir)
		shift 1
		mkdir $*
	;;
	filemgr_rm)
		shift 1
		rm -f $*
	;;
	filemgr_rmdir)
		shift 1
		rm -rf $*
	;;
	filemgr_upload)
		shift 1
		mv /tmp/upload.tmp "$1/$2"
		rm -f /tmp/upload.tmp
	;;
	filemgr_ren)
		shift 1
		mv -f $1 $2
	;;
	filemgr_copy)
		shift 1
		cp -r -f $1 $2
	;;
	filemgr_check_movieplayer_xml)
		shift 1
		grep "neutrino commandversion" $*
	;;
	filemgr_vlc_file)
		shift 1
		echo "$*" >/tmp/vlc.m3u
	;; 		
	nhttpd_can_sendall)
		# Whether the old server would hand out any file it was asked for. That
		# server is gone and so is the switch; the answer can only be no now.
		# Said by answering nothing, which is what the caller reads as no, and
		# not by grepping a file that is no longer installed: a grep over a
		# missing file answers the same and says it was a question nobody had
		# looked at.
		:
	;;
	*)
		echo "[Y_NAS.sh] Parameter falsch: $*"
	;;
esac

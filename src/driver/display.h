/* helper for different display CVFD implementations */
#if HAVE_CST_HARDWARE
#include <driver/vfd.h>
#endif
#if HAVE_GENERIC_HARDWARE || HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
#include <driver/simple_display.h>
#endif
#ifdef ENABLE_GRAPHLCD
#include <driver/glcd/glcd.h>
#endif

#ifndef __DISPLAY_H__
#define __DISPLAY_H__

#include <string>

class CDisplay
{
	public:
		CDisplay(){};
		virtual ~CDisplay(){};

		virtual void showServicename(const std::string, const bool ){}; // UTF-8
		virtual void showServicename(const std::string, int){}; // UTF-8
};

#endif

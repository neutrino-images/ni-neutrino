/*
	imageinfo_ni

	(C) 2009-2016 NG-Team
	(C) 2016 NI-Team

	License: GPL

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifndef __imageinfo_ni__
#define __imageinfo_ni__

#include <configfile.h>

#include <gui/widget/menue.h>
#include <system/localize.h>
#include <driver/framebuffer.h>

class CImageInfoNI : public CMenuTarget
{
	private:
		void Init(void);
		CConfigFile     * configfile;
		CFrameBuffer	*frameBuffer;
		int x;
		int y;
		int ypos;
		int xcpu;
		int ycpu;
		int width;
		int height;
		int hheight,iheight,sheight;
		int swidth;

		int max_height;
		int max_width;
		int max_text_width;
		
		neutrino_locale_t name;
		int offset;

		int font_head;
		int font_info;
		int font_small;

		void paint();
		void paint_pig(int x, int y, int w, int h);
		void paintLine(int xpos, int font, std::string text);
		void clearLine(int xpos, int font);

		typedef struct {
			unsigned long usr;
			unsigned int nice;
			unsigned long system;
			unsigned long idle;
			unsigned long old_usr;
			unsigned int old_nice;
			unsigned long old_system;
			unsigned long old_idle;
		} _stat;

		struct {
			unsigned long blocks;
			unsigned long used;
			unsigned long available;
			int percent;
		} image_size;

		struct {
			int total;
			int free;
			int buffers;
			int cached;
			int tfree;
			int used;
		} mem_info;

		struct S_MTD_INFO {
			char dev[8];
			char size[9];
			char erasesize[9];
			char name[40];
		};
		std::vector<S_MTD_INFO> mtd_info;

		struct timeval tv, last_tv;
		uint64_t read_old;
		uint64_t write_old;
		uint64_t net_best;

		pthread_t	InfoThread;
		static void*	InfoProc(void *arg);
		void 		StartInfoThread();
		void 		StopInfoThread();

		int systemfs;

		int old_x;
		int old_y;

		int sigBox_pos;
		int sigBox_x;
		int sigBox_y;
		int sigBox_w;
		int sigBox_h;

		std::string get_systemRoot();
		int get_MTD_Info();
#if 0
		void paint_MTD_Info(int posx);
#endif
		void get_DF_Info();
		void paint_DF_Info(int posx);

		int get_MEM_Info();
		int get_MEM_Percent(int total, int used);
		void paint_MEM_Info(int posx, int posy);

		int Stat_Info(_stat *stat_info);
		void paint_Stat_Info_Box(int x, int y, int w, int h);
		int CPU_Percent(_stat *cpu);
		void paint_CPU_Percent(int percent);
		int y_cpu_pixel(int value, int max_range, int max_y);

		void get_NET_Info(uint64_t *read_akt, long *read_packet, long *dummy, uint64_t *write_akt, long *write_packet);
		void paint_NET_Info(int posx, int posy);

		std::string getYWebVersion();

	public:

		CImageInfoNI();
		~CImageInfoNI();

		void hide();
		int exec(CMenuTarget* parent, const std::string & actionKey);
};

#endif

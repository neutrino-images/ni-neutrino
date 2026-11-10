/*
 * script components window
 *
 * The window a script builds, in the language the scripts are written in:
 * neither this nor anything it calls knows which interpreter asked for it.
 * A binding reads its own table of parameters, fills a ScriptWindow::params
 * and hands it over; everything from there on is one implementation, so that
 * two interpreters cannot come to different behaviour.
 *
 * This is where the native logic of the script API lives. It sits below the
 * bindings and above the components, and the coreapi is deliberately not
 * involved: the components are gui/ and the coreapi may not reach into gui/
 * (see src/coreapi/check-layering.sh).
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _SCRIPTWINDOW_H
#define _SCRIPTWINDOW_H

#include <string>
#include <vector>

#include <gui/components/cc.h>
#include <gui/components/cc_frm_header.h>

/* The window a script draws in, with a header, a footer and the labels of the
 * keys under it. */
class ScriptWindow : public CCHeaderTypes
{
	public:
		/* One label of the footer: the icon of the key it belongs to and the
		 * text beside it. */
		typedef button_label_cc button_t;

		/* What a script asked for. A binding fills this from its own table,
		 * so the field names are the ones the bindings' parameters are. */
		typedef struct params {
			std::string		name;
			std::string		icon;
			int			x, y, dx, dy;
			int			shadow;		/* CC_SHADOW_OFF or CC_SHADOW_ON */
			unsigned long		color_frame;
			unsigned long		color_body;
			unsigned long		color_shadow;
			bool			show_header;
			bool			show_footer;
			std::vector<button_t>	buttons;
		} params_t;

		explicit ScriptWindow(const params_t &pars);
		~ScriptWindow();

		/* The window itself, for a binding that needs to hand it on as a
		 * parent to another component. */
		CComponentsWindow	*window(void) { return w; }

		/* The form a component a script adds to the window is parented to. */
		CComponentsForm		*body(void) { return w->getBodyObject(); }

		/* A colour as the box wants it, which both bindings run a colour
		 * through so that neither normalises it differently. */
		unsigned long	checkColor(unsigned long color);

		/* the calls a script makes */
		void	paint(bool do_save_bg);
		void	hide();
		void	setCaption(const std::string &name, int alignment);
		void	setColorFrame(unsigned long color);
		void	setColorBody(unsigned long color);
		void	setColorShadow(unsigned long color);
		void	paintHeader();
		int	headerHeight() const;
		int	footerHeight() const;
		void	setCenterPos(int along_mode);
		void	setDimensionsAll(int x, int y, int w, int h);
		void	setBodyImage(const std::string &image);

	private:
		CComponentsWindow	*w;
};

#endif /* _SCRIPTWINDOW_H */

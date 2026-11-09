/*
 * script components window
 *
 * See scriptwindow.h for what this is for.
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

#include "scriptwindow.h"

#include <global.h>
#include <driver/framebuffer.h>
#include <neutrino.h>

#include "scriptcolor.h"

ScriptWindow::ScriptWindow(const params_t &pars)
{
	w = new CComponentsWindow(pars.x, pars.y, pars.dx, pars.dy,
			pars.name.c_str(), pars.icon.c_str(), NULL,
			pars.shadow,
			(fb_pixel_t)checkColor(pars.color_frame),
			(fb_pixel_t)checkColor(pars.color_body),
			(fb_pixel_t)checkColor(pars.color_shadow));

	/* Ignore percent conversion of width and height, so that a script asking
	 * for a size gets the pixels it asked for. */
	w->setWidth(pars.dx);
	w->setHeight(pars.dy);

	if (!pars.show_header)
		w->showHeader(false);

	if (!pars.show_footer)
		w->showFooter(false);
	else if (!pars.buttons.empty())
	{
		CComponentsFooter *footer = w->getFooterObject();
		if (footer)
			footer->setButtonLabels(pars.buttons, footer->getWidth(),
					footer->getWidth() / pars.buttons.size());
	}
}

ScriptWindow::~ScriptWindow()
{
	delete w;
}

/* A colour a script names as one of the box's own is marked with the magic
 * the colour tables carry, and is resolved here to the pixel the display
 * holds for it. Both bindings did this themselves before handing a colour
 * on, which is how they could come to do it differently. */
unsigned long ScriptWindow::checkColor(unsigned long color)
{
	return ScriptColorResolve(color);
}

void ScriptWindow::paint(bool do_save_bg)
{
	w->paint(do_save_bg);
}

void ScriptWindow::hide()
{
	w->hide();
}

void ScriptWindow::setCaption(const std::string &name, int alignment)
{
	w->setWindowCaption(name, (cc_title_alignment_t)alignment);
}

void ScriptWindow::setColorFrame(unsigned long color)
{
	w->setColorFrame((fb_pixel_t)checkColor(color));
}

void ScriptWindow::setColorBody(unsigned long color)
{
	w->setColorBody((fb_pixel_t)checkColor(color));
}

void ScriptWindow::setColorShadow(unsigned long color)
{
	w->setColorShadow((fb_pixel_t)checkColor(color));
}

void ScriptWindow::paintHeader()
{
	CComponentsHeader *header = w->getHeaderObject();
	if (header)
	{
		w->showHeader();
		header->paint();
	}
}

int ScriptWindow::headerHeight() const
{
	CComponentsHeader *header = w->getHeaderObject();
	return header ? header->getHeight() : 0;
}

int ScriptWindow::footerHeight() const
{
	CComponentsFooter *footer = w->getFooterObject();
	return footer ? footer->getHeight() : 0;
}

void ScriptWindow::setCenterPos(int along_mode)
{
	w->setCenterPos(along_mode);
}

void ScriptWindow::setDimensionsAll(int x, int y, int dw, int dh)
{
	/* The window is kept on the screen, as the scripts have always had it:
	 * the size is clamped to what the box shows, and a position outside is
	 * pulled back to the corner. */
	if (x > -1 && y > -1 && dw > 1 && dh > 1)
	{
		int screen_h = (int)CFrameBuffer::getInstance()->getScreenHeight();
		int screen_w = (int)CFrameBuffer::getInstance()->getScreenWidth();

		if (dh > screen_h)
			dh = screen_h;
		if (dw > screen_w)
			dw = screen_w;
		if (x > dw)
			x = 0;
		if (y > dh)
			y = 0;
		this->w->setDimensionsAll(x, y, dw, dh);
	}
}

void ScriptWindow::setBodyImage(const std::string &image)
{
	w->setBodyBGImage(image);
}

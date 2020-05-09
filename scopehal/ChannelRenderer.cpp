/***********************************************************************************************************************
*                                                                                                                      *
* ANTIKERNEL v0.1                                                                                                      *
*                                                                                                                      *
* Copyright (c) 2012-2020 Andrew D. Zonenberg                                                                          *
* All rights reserved.                                                                                                 *
*                                                                                                                      *
* Redistribution and use in source and binary forms, with or without modification, are permitted provided that the     *
* following conditions are met:                                                                                        *
*                                                                                                                      *
*    * Redistributions of source code must retain the above copyright notice, this list of conditions, and the         *
*      following disclaimer.                                                                                           *
*                                                                                                                      *
*    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the       *
*      following disclaimer in the documentation and/or other materials provided with the distribution.                *
*                                                                                                                      *
*    * Neither the name of the author nor the names of any contributors may be used to endorse or promote products     *
*      derived from this software without specific prior written permission.                                           *
*                                                                                                                      *
* THIS SOFTWARE IS PROVIDED BY THE AUTHORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED   *
* TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL *
* THE AUTHORS BE HELD LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES        *
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR       *
* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT *
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE       *
* POSSIBILITY OF SUCH DAMAGE.                                                                                          *
*                                                                                                                      *
***********************************************************************************************************************/

/**
	@file
	@author Andrew D. Zonenberg
	@brief Implementation of ChannelRenderer
 */

#include "scopehal.h"
#include "ChannelRenderer.h"
#include "ProtocolDecoder.h"

using namespace std;

Gdk::Color ChannelRenderer::m_standardColors[STANDARD_COLOR_COUNT] =
{
	Gdk::Color("#336699"),	//COLOR_DATA
	Gdk::Color("#c000a0"),	//COLOR_CONTROL
	Gdk::Color("#ffff00"),	//COLOR_ADDRESS
	Gdk::Color("#808080"),	//COLOR_PREAMBLE
	Gdk::Color("#00ff00"),	//COLOR_CHECKSUM_OK
	Gdk::Color("#ff0000"),	//COLOR_CHECKSUM_BAD
	Gdk::Color("#ff0000"),	//COLOR_ERROR
	Gdk::Color("#404040")	//COLOR_IDLE
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

ChannelRenderer::ChannelRenderer(OscilloscopeChannel* channel)
: m_channel(channel)
{
}

ChannelRenderer::~ChannelRenderer()
{

}

void ChannelRenderer::MakePathSignalBody(
	const Cairo::RefPtr<Cairo::Context>& cr,
	float xstart, float /*xoff*/, float xend, float ybot, float /*ymid*/, float ytop)
{
	//If the signal is really tiny, shrink the rounding to avoid going out of bounds
	float rounding = 10;
	if(xstart + 2*rounding > xend)
		rounding = (xend - xstart) / 2;

	cr->begin_new_sub_path();
	cr->arc(xstart + rounding, ytop + rounding, rounding, M_PI, M_PI*1.5f);	//top left corner
	cr->move_to(xstart + rounding, ytop);									//top edge
	cr->line_to(xend - rounding, ytop);
	cr->arc(xend - rounding, ytop + rounding, rounding, M_PI*1.5f, 0);		//top right corner
	cr->move_to(xend, ytop + rounding);										//right edge
	cr->line_to(xend, ybot - rounding);
	cr->arc(xend - rounding, ybot - rounding, rounding, 0, M_PI_2);			//bottom right corner
	cr->move_to(xend - rounding, ybot);										//bottom edge
	cr->line_to(xstart + rounding, ybot);
	cr->arc(xstart + rounding, ybot - rounding, rounding, M_PI_2, M_PI);	//bottom left corner
	cr->move_to(xstart, ybot - rounding);									//left edge
	cr->line_to(xstart, ytop + rounding);
}

void ChannelRenderer::RenderComplexSignal(
		const Cairo::RefPtr<Cairo::Context>& cr,
		int visleft, int visright,
		float xstart, float xend, float xoff,
		float ybot, float ymid, float ytop,
		string str,
		Gdk::Color color)
{
	Pango::FontDescription font("sans normal 10");
	int width = 0, sheight = 0;
	GetStringWidth(cr, str, width, sheight, font);

	//First-order guess of position: center of the value
	float xp = xstart + (xend-xstart)/2;

	//Width within this signal outline
	float available_width = xend - xstart - 2*xoff;

	//Minimum width (if outline ends up being smaller than this, just fill)
	float min_width = 40;
	if(width < min_width)
		min_width = width;

	//Does the string fit at all? If not, skip all of the messy math
	if(available_width < min_width)
		str = "";
	else
	{
		//Center the text by moving it left half a width
		xp -= width/2;

		//Off the left end? Push it right
		int padding = 5;
		if(xp < (visleft + padding))
		{
			xp = visleft + padding;
			available_width = xend - xp - xoff;
		}

		//Off the right end? Push it left
		else if( (xp + width + padding) > visright)
		{
			xp = visright - (width + padding + xoff);
			if(xp < xstart)
				xp = xstart + xoff;

			if(xend < visright)
				available_width = xend - xp - xoff;
			else
				available_width = visright - xp - xoff;
		}

		//If we don't fit under the new constraints, give up
		if(available_width < min_width)
			str = "";
	}

	//Draw the text
	if(str != "")
	{
		//Text is always white (TODO: only in overlays?)
		cr->set_source_rgb(1, 1, 1);

		//If we need to trim, decide which way to do it.
		//If the text is all caps and includes an underscore, it's probably a macro with a prefix.
		//Trim from the left in this case. Otherwise, trim from the right.
		bool trim_from_right = true;
		bool is_all_upper = true;
		for(size_t i=0; i<str.length(); i++)
		{
			if(islower(str[i]))
				is_all_upper = false;
		}
		if(is_all_upper && (str.find("_") != string::npos))
			trim_from_right = false;

		//Some text fits, but maybe not all of it
		//We know there's enough room for "some" text
		//Try shortening the string a bit at a time until it fits
		//(Need to do an O(n) search since character width is variable and unknown to us without knowing details
		//of the font currently in use)
		string str_render = str;
		if(width > available_width)
		{
			for(int len = str.length() - 1; len > 1; len--)
			{
				if(trim_from_right)
					str_render = str.substr(0, len) + "...";
				else
					str_render = "..." + str.substr(str.length() - len - 1);

				int twidth = 0, theight = 0;
				GetStringWidth(cr, str_render, twidth, theight, font);
				if(twidth < available_width)
				{
					//Re-center text in available space
					//TODO: Move to avoid any time-split lines
					xp += (available_width - twidth)/2;
					if(xp < (xstart + xoff))
						xp = (xstart + xoff);
					break;
				}
			}
		}

		cr->save();
			Glib::RefPtr<Pango::Layout> tlayout = Pango::Layout::create (cr);
			cr->move_to(xp, ymid - sheight/2);
			font.set_weight(Pango::WEIGHT_NORMAL);
			tlayout->set_font_description(font);
			tlayout->set_text(str_render);
			tlayout->update_from_cairo_context(cr);
			tlayout->show_in_cairo_context(cr);
		cr->restore();
	}

	//If no text fit, draw filler instead
	else
	{
		cr->set_source_rgb(color.get_red_p() * 0.25, color.get_green_p() * 0.25, color.get_blue_p() * 0.25);
		MakePathSignalBody(cr, xstart, xoff, xend, ybot, ymid, ytop);
		cr->fill();
	}

	//Draw the body outline after any filler so it shows up on top
	if(xend > visright)
		xend = visright;
	cr->set_source_rgb(color.get_red_p(), color.get_green_p(), color.get_blue_p());
	MakePathSignalBody(cr, xstart, xoff, xend, ybot, ymid, ytop);
	cr->stroke();
}

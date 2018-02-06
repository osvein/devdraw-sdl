/* devdraw-sdl - SDL 2.0-based implementation of Plan 9 from User Space devdraw
 * Copyright (c) 2018 Oskar Sveinsen
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include	<u.h>
#include	<libc.h>
#include	<draw.h>
#include	<drawfcall.h>

#include	<SDL.h>

#include	"devdraw.h"

#define	HANDLER(type)	((type >> 1) - 1)
void	(*handlers[])(Wsysmsg *)	= {
	[HANDLER(Trdmouse)]	= &handle_rdmouse,
	[HANDLER(Tmoveto)]	= &handle_moveto,
	[HANDLER(Tcursor)]	= &handle_cursor,
	[HANDLER(Tbouncemouse)]	= &handle_bouncemouse,
	[HANDLER(Trdkbd)]	= &handle_rdkbd,
	[HANDLER(Tlabel)]	= &handle_label,
	[HANDLER(Tinit)]	= &handle_init,
	[HANDLER(Trdsnarf)]	= &handle_rdsnarf,
	[HANDLER(Twrsnarf)]	= &handle_wrsnarf,
	[HANDLER(Trddraw)]	= &handle_rddraw,
	[HANDLER(Twrdraw)]	= &handle_wrdraw,
	[HANDLER(Ttop)]	= &handle_top,
	[HANDLER(Tresize)]	= &handle_resize
};

void
main(int argc, char *argv[])
{
	uchar	*buf;
	int	nbuf;
	Wsysmsg	msg;

	ARGBEGIN {
	defualt:
	} ARGEND

	buf = malloc(nbuf = 4);
	while (read(1, buf, 4) == 4) {
		int	n;

		GET(buf, n);
		if (n > nbuf)  {
			free(buf);
			if (!(buf = malloc(4 + n)))
				sysfatal("malloc: %r");
			PUT(buf, n);
			nbuf = n;
		}
		if (readn(3, buf + 4, n - 4) != 4)
			sysfatal("message too short");
		if (!convM2W(buf, n, &msg))
			sysfatal("invalid message");
		handlers[HANDLER(msg.type)](&msg);
	}
}

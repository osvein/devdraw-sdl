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
#include	<cursor.h>
#include	<memdraw.h>
#include	<mouse.h>
#include	<drawfcall.h>

#include	<SDL.h>

#include	"devdraw.h"

#define	HANDLER(type)	((type >> 1) - 1)

#define	DEFAULT_WINW	640
#define	DEFAULT_WINH	480
#define	DEFAULT_WINFLAGS	SDL_WINDOW_RESIZABLE

#define	BUFSZ_RDDRAW	0x10000

static void	reply(Wsysmsg *msg);
static void	replyerrstr(Wsysmsg *msg);
static void	replyerrsdl(Wsysmsg *msg);

static void	handle_rdmouse(Wsysmsg *);
static void	handle_moveto(Wsysmsg *);
static void	handle_cursor(Wsysmsg *);
static void	handle_bouncemouse(Wsysmsg *);
static void	handle_rdkbd(Wsysmsg *);
static void	handle_label(Wsysmsg *);
static void	handle_init(Wsysmsg *);
static void	handle_rdsnarf(Wsysmsg *);
static void	handle_wrsnarf(Wsysmsg *);
static void	handle_rddraw(Wsysmsg *);
static void	handle_wrdraw(Wsysmsg *);
static void	handle_top(Wsysmsg *);
static void	handle_resize(Wsysmsg *);

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

SDL_Window	*win;

void
reply(Wsysmsg *msg)
{
	static uchar	*buf;
	static int	nbuf;
	uint n;

	msg->type |= 1;
	if ((n = sizeW2M(msg)) > nbuf) {
		free(buf);
		if (!(buf = malloc(nbuf = n)))
			sysfatal("malloc: %r");
	}
	convW2M(msg, buf, n);
	if (write(1, buf, n) != n)
		sysfatal("write: %r");
}

void
replyerrstr(Wsysmsg *msg)
{
	char	err[ERRMAX];

	rerrstr(err, sizeof(err));
	msg->type = Rerror;
	msg->error = err;
	reply(msg);
}

void
replyerrsdl(Wsysmsg *msg)
{
	msg->type = Rerror;
	msg->error = SDL_GetError();
	reply(msg);
	SDL_ClearError();
}

void
handle_moveto(Wsysmsg *msg)
{
	SDL_WarpMouseInWindow(win, msg->mouse.xy.x, msg->mouse.xy.y);
	reply(msg);
}

void
handle_cursor(Wsysmsg *msg)
{
	SDL_FreeCursor(SDL_GetCursor()); /* cursor will be reset to default as a side effect */
	if (!msg->arrowcursor) {
		Cursor	*cur;
		SDL_Cursor	*sdlcur;

		cur = &msg->cursor;
		sdlcur = SDL_CreateCursor(cur->set, cur->clr, 16, 16, cur->offset.x, cur->offset.y);
		if (!sdlcur) {
			replyerrsdl(msg);
			return;
		}
		SDL_SetCursor(sdlcur);
	}
	reply(msg);
}

void
handle_label(Wsysmsg *msg)
{
	SDL_SetWindowTitle(win, msg->label);
	reply(msg);
}

void
handle_init(Wsysmsg *msg)
{
	int x, y, w, h;

	if (!SDL_Init(SDL_INIT_VIDEO)) {
		replyerrsdl(msg);
		return;
	}

	x = y = SDL_WINDOWPOS_UNDEFINED;
	w = DEFAULT_WINW;
	h = DEFAULT_WINH;
	if (*msg->winsize) {
		Rectangle r;
		int havemin;

		if (parsewinsize(msg->winsize, &r, &havemin) < 0) {
			replyerrstr(msg);
			return;
		}
		if (havemin) {
			x = r.min.x;
			y = r.min.y;
		}
		w = Dx(r);
		h = Dy(r);
	}
	if (win = SDL_CreateWindow(msg->label, x, y, w, h, DEFAULT_WINFLAGS))
		reply(msg);
	else
		replyerrsdl(msg);
}

void
handle_rdsnarf(Wsysmsg *msg)
{
	if (msg->snarf = SDL_GetClipboardText()) {
		reply(msg);
		SDL_free(msg->snarf);
	} else {
		replyerrsdl(msg);
	}
}

void
handle_wrsnarf(Wsysmsg *msg)
{
	if (SDL_SetClipboardText(msg->snarf))
		replyerrsdl(msg);
	else
		reply(msg);
}

void
handle_rddraw(Wsysmsg *msg)
{
	uchar	buf[BUFSZ_RDDRAW];

	msg->data = buf;
	if (msg->count > sizeof(buf))
		msg->count = sizeof(buf);
	if ((msg->count = _drawmsgread(buf, msg->count)) >= 0)
		reply(msg);
	else
		replyerrstr(msg);
}

void
handle_wrdraw(Wsysmsg *msg)
{
	if (_drawmsgwrite(m->data, m->count) >= 0)
		reply(msg);
	else
		replyerrstr(msg);
}

void
handle_top(Wsysmsg *msg)
{
	SDL_RaiseWindow(win);
	reply(msg);
}

void
handle_resize(Wsysmsg *msg)
{
	SDL_SetWindowSize(win, Dx(msg->rect), Dy(msg->rect));
	reply(msg);
}

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

/*
 * $Id: onewire.c,v 1.3 2005/07/29 01:16:39 cjheath Exp $
 *
 * Ruby/OneWire
 *
 * Clifford Heath <cjh@polyplex.org>
 *
 * This code is hereby licensed for public consumption under either the
 * GNU LGPL v2 or greater.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#define VERSION 	"0.1"

#include <ruby.h>		/* ruby inclusion */

VALUE		cOneWire;	/* OneWire module */
VALUE		cPort;		/* OneWire port class */
VALUE		cDevice;	/* Device class */

#if defined(mswin) || defined(bccwin)

#include	<stdio.h>	/* Standard input/output definitions */
#include	<io.h>		/* Low-level I/O definitions */
#include	<fcntl.h>	/* File control definitions */
#include	<windows.h>	/* Windows standard function definitions */

#else /* defined(mswin) || defined(bccwin) */

#include	<stdio.h>	/* Standard input/output definitions */
#include	<unistd.h>	/* UNIX standard function definitions */
#include	<errno.h>	/* Error number definitions */

#endif /* defined(mswin) || defined(bccwin) */

/* Include files from the Dallas/Maxim Onw-Wire Public Domain API */
#include	<ownet.h>
#include	<rawmem.h>
#include	<mbnv.h>
#include	<time04.h>

/*
 * Open the specified OneWire port
 */
static VALUE
ow_initialize(VALUE self, VALUE _port)
{
	int	ow;

	Check_SafeStr(_port);
	if (TYPE(_port) != T_STRING)
		rb_raise(rb_eTypeError, "OneWire::Port port must be a string");
	rb_iv_set(self, "@port", _port);

	ow = owAcquireEx(STR2CSTR(_port));
	rb_iv_set(self, "@ow", INT2FIX(ow));
	if (ow < 0)
		rb_raise(rb_eIOError, owGetErrorMsg(owGetErrorNum()));

	return self;
}

/*
 * Close the OneWire Port
 */
static VALUE
ow_close(VALUE self)
{
	int	ow = FIX2INT(rb_iv_get(self, "@ow"));
	if (ow >= 0)
	{
		owRelease(ow);
		rb_iv_set(self, "@ow", INT2FIX(-1));
	}
	return Qnil;
}

/*
 * Enumerate devices on the port
 */
static VALUE
ow_enumerate(VALUE self)
{
	int	button_ar = rb_ary_new();
	int	ow = FIX2INT(rb_iv_get(self, "@ow"));
	if (ow >= 0)
	{
		int more;
		for (
			more = owFirst(ow, TRUE, FALSE);
			more != 0;
			more = owNext(ow, TRUE, FALSE)
		)
		{
			char	serial[8];
			VALUE	argv[2];
			owSerialNum(ow, serial, TRUE);

			argv[0] = self;		/* Port */
			argv[1] = rb_str_new(serial, 8);
			rb_ary_push(
				button_ar,
				rb_class_new_instance(2, argv, cDevice)
			);
		}
	}
	else
		rb_raise(rb_eArgError, "One-wire port is not active");

	return button_ar;
}

static VALUE
ow_initialize_device(VALUE self, VALUE _port, VALUE _serial)
{
	rb_iv_set(self, "@port", _port);
	rb_iv_set(self, "@serial", _serial);

	return self;
}

static VALUE
ow_name(VALUE self)
{
	char*	serial = STR2CSTR(rb_iv_get(self, "@serial"));

	return rb_str_new2(owGetName(serial));
}

static VALUE
ow_describe(VALUE self)
{
	char*	serial = STR2CSTR(rb_iv_get(self, "@serial"));

	return rb_str_new2(owGetDescription(serial));
}

static VALUE
ow_banks(VALUE self)
{
	char*	serial = STR2CSTR(rb_iv_get(self, "@serial"));
	int	i;
	int	banks;
	int	bank_ar = rb_ary_new();

	banks = owGetNumberBanks(serial[0]);
	for (i = 0; i < banks; i++)
	{
		rb_ary_push(
			bank_ar,
			rb_str_new2(
				owGetBankDescription(i, serial)
			)
		);
	}
	return bank_ar;
}

static VALUE
ow_bankPages(VALUE self, VALUE _bank)
{
	char*	serial = STR2CSTR(rb_iv_get(self, "@serial"));
	return INT2FIX(owGetNumberPages(FIX2INT(_bank), serial));
}

static VALUE
ow_readPage(VALUE self, VALUE _bank, VALUE _page)
{
	char*	serial = STR2CSTR(rb_iv_get(self, "@serial"));
	VALUE	port = rb_iv_get(self, "@port");
	int	ow = FIX2INT(rb_iv_get(port, "@ow"));
	int	bank = FIX2INT(_bank);
	int	page = FIX2INT(_page);
	char	page_buf[64];
	int	size;
	int	has_extra;
	int	has_autocrc;
	char	extra_buf[32];
	int	result = 0;
	VALUE	ret;

	has_extra = owHasExtraInfo(bank, serial);
	has_autocrc = owHasPageAutoCRC(bank, serial);

	switch (has_extra+has_autocrc*2)
	{
	case 0:			/* Neither */
		result = owReadPage(bank, ow, serial, page, FALSE, page_buf);
		break;
	case 1+0:		/* Extra data */
		result = owReadPageExtra(bank, ow, serial, page, FALSE,
				page_buf, extra_buf);
		break;
	case 0+2:		/* Autocrc */
		result = owReadPageCRC(bank, ow, serial, page, page_buf);
		break;
	case 1+2:		/* Both */
		result = owReadPageExtraCRC(bank, ow, serial, page,
				page_buf, extra_buf);
	}
	if (!result)
		rb_raise(rb_eIOError, owGetErrorMsg(owGetErrorNum()));
	size = owGetPageLength(bank, serial);
	ret = rb_str_new(page_buf, size);

	if (has_extra)
	{		/* Return an array of the page and the extra data */
		VALUE	data = ret;
		int	extra_length = owGetExtraInfoLength(bank, serial);
		VALUE	extra = rb_str_new(extra_buf, extra_length);
		ret = rb_ary_new();
		rb_ary_push(ret, data);
		rb_ary_push(ret, extra);
	}
	return ret;
}

static VALUE
ow_writeBlock(VALUE self, VALUE _bank, VALUE _addr, VALUE _data)
{
	char*	serial = STR2CSTR(rb_iv_get(self, "@serial"));
	VALUE	port = rb_iv_get(self, "@port");
	int	ow = FIX2INT(rb_iv_get(port, "@ow"));
	int	bank = FIX2INT(_bank);
	int	addr = FIX2INT(_addr);
	// Get _data as a string; beware NULLs in the data!
	long	length;
	char*	data = rb_str2cstr(_data, &length);

	if (!owWrite(bank, ow, serial, addr, data, length))
		rb_raise(rb_eIOError, owGetErrorMsg(owGetErrorNum()));
	return Qnil;
}

/* Set the clock and start it running */
static VALUE
ow_setRTC(VALUE self, VALUE _time)
{
	char*		serial = STR2CSTR(rb_iv_get(self, "@serial"));
	VALUE		port = rb_iv_get(self, "@port");
	int		ow = FIX2INT(rb_iv_get(port, "@ow"));
	unsigned long	time = NUM2ULONG(_time);
	unsigned char	tb[4];

	/* Must be DS1994 or DS1904 */
	if (serial[0] != 0x04 && serial[0] != 0x24)
		rb_raise(rb_eArgError, "Button lacks clock features");

	tb[0] = time;
	tb[1] = (time>>=8);
	tb[2] = (time>>=8);
	tb[3] = (time>>=8);

	if (!writeNV(2, ow, serial, 0x03, tb, 4)
	 || setOscillator(ow, serial, 1) != TRUE)
		rb_raise(rb_eIOError, owGetErrorMsg(owGetErrorNum()));
	return Qnil;
}

/* Stop the clock */
static VALUE
ow_stopRTC(VALUE self)
{
	char*		serial = STR2CSTR(rb_iv_get(self, "@serial"));
	VALUE		port = rb_iv_get(self, "@port");
	int		ow = FIX2INT(rb_iv_get(port, "@ow"));

	/* Must be DS1994 or DS1904 */
	if (serial[0] != 0x04 && serial[0] != 0x24)
		rb_raise(rb_eArgError, "Button lacks clock features");

	if (setOscillator(ow, serial, 0) != TRUE)
		rb_raise(rb_eIOError, owGetErrorMsg(owGetErrorNum()));
	return Qnil;
}

/* Read the clock in seconds */
static VALUE
ow_getRTC(VALUE self)
{
	char*		serial = STR2CSTR(rb_iv_get(self, "@serial"));
	VALUE		port = rb_iv_get(self, "@port");
	int		ow = FIX2INT(rb_iv_get(port, "@ow"));
	unsigned char	tb[4];
	unsigned long	time;

	/* Must be DS1994 or DS1904 */
	if (serial[0] != 0x04 && serial[0] != 0x24)
		rb_raise(rb_eArgError, "Button lacks clock features");

	if (!readNV(2, ow, serial, 0x03, FALSE, tb, 4))
		rb_raise(rb_eIOError, owGetErrorMsg(owGetErrorNum()));

	time = (((((tb[3]<<8)|tb[2])<<8)|tb[1])<<8)|tb[0];
	return ULONG2NUM(time);
}

void
Init_onewire()
{
	cOneWire = rb_define_module("OneWire");

	cPort = rb_define_class_under(cOneWire, "Port", rb_cObject);
	rb_define_method(cPort, "initialize", ow_initialize, 1);
	rb_define_method(cPort, "close", ow_close, 0);
	rb_define_method(cPort, "enumerate", ow_enumerate, 0);

	cDevice = rb_define_class_under(cOneWire, "Device", rb_cObject);
	rb_define_method(cDevice, "initialize", ow_initialize_device, 2);
	rb_define_method(cDevice, "name", ow_name, 0);
	rb_define_method(cDevice, "describe", ow_describe, 0);

	/* Memory banks, for devices having memory */
	rb_define_method(cDevice, "banks", ow_banks, 0);
	rb_define_method(cDevice, "bankPages", ow_bankPages, 1);
	rb_define_method(cDevice, "readPage", ow_readPage, 2);
	rb_define_method(cDevice, "writeBlock", ow_writeBlock, 3);

	/* Clock methods, for real-time clock buttons */
	rb_define_method(cDevice, "setRTC", ow_setRTC, 1);
	rb_define_method(cDevice, "getRTC", ow_getRTC, 0);
	rb_define_method(cDevice, "stopRTC", ow_stopRTC, 0);
}
